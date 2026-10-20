(* The substrate pipeline behind the UI gestures.

   Analysis is inert: it parses, abstracts and resolves without storing or
   binding anything, so the editor can show what a commit WOULD do. That is
   cheap here in a way it is not in p21, where the same preview has to simulate
   an ingest against a binding group that does not exist yet and nothing it
   computes can be cached because none of it is keyed by a hash. Here
   abstraction is a pure function of the source text and its result is already
   the stored object's identity. *)
open! Core
open P22_substrate

type slot_row =
  { slot : int
  ; name : string
  ; outcome : string (* a chip: where this free variable resolves *)
  ; target : string
  }

type preview =
  { name : string
  ; declared : string (* the signature, if one was written *)
  ; is_edit : bool
  ; in_cycle : bool (* committed as part of a bundle *)
  ; arity : int
  ; open_hash : string
  ; slots : slot_row list
  ; status : string (* typechecking verdict, or why not *)
  ; ok : bool
  }

type analysis =
  { previews : preview list
  ; errors : string list
  ; below_fold : string
  }

(* `pending` is how many of the names in the resolution context are bindings
   that a LATER unit of this same commit will see as already stored, rather than
   members of a cycle. The distinction is display-only -- both are To_group
   during analysis -- but it matters to a reader, because one of them becomes a
   bundle and the other becomes an ordinary namespace reference. *)
let outcome_chip ~pending (o : Bind.outcome) =
  match o with
  | Bind.To_def _ -> "namespace"
  | Bind.To_builtin _ -> "builtin"
  | Bind.To_group i -> if i < pending then "buffer" else "cycle"
  | Bind.Unresolved -> "unresolved"

let outcome_target ~pending ~(ctx : string list) (o : Bind.outcome) =
  match o with
  | Bind.To_def h -> Hash.short ~len:8 h
  | Bind.To_builtin b -> b
  | Bind.To_group i ->
    if i < pending
    then "defined earlier in this commit"
    else
      Printf.sprintf
        "self.%d (%s)"
        (i - pending)
        (Option.value (List.nth ctx i) ~default:"?")
  | Bind.Unresolved -> "\xe2\x80\x94"

(* Analyse a buffer without touching the substrate.

   It walks the SAME plan the commit path walks (Substrate.plan_of_file), so
   the preview cannot disagree with what committing would do. An earlier version
   resolved every binding against the whole buffer as one group, and the preview
   confidently reported `self.0` for references that the commit path was going
   to resolve through the namespace. *)
let analyze (text : string) : analysis =
  let s = Substrate.global in
  let file, errs = Parse.file text in
  let errors = List.map errs ~f:(fun (e : Parse.located_error) -> e.Parse.message) in
  let sig_of name =
    List.find_map file.Surface.stanzas ~f:(function
      | Surface.Signature (n, t) when String.equal n name -> Some t
      | _ -> None)
  in
  (* `pending` names are committed by an earlier unit of this same plan; `cycle`
     names are the other members of this unit's bundle. Both resolve through the
     same mechanism during analysis; only the label differs. *)
  let preview_of ~pending ~cycle (name, src) =
    let group = pending @ cycle in
    let np = List.length pending in
    let a = Open_term.abstract src in
    let outcomes =
      Array.to_list a.Open_term.slot_names
      |> List.map ~f:(fun n -> n, Bind.resolve_name s.Session.ns ~group n)
    in
    let slots =
      List.mapi outcomes ~f:(fun i (n, o) ->
        { slot = i
        ; name = n
        ; outcome = outcome_chip ~pending:np o
        ; target = outcome_target ~pending:np ~ctx:group o
        })
    in
    let link : Link.t =
      { Link.arity = a.Open_term.arity
      ; Link.targets =
          Array.of_list (List.map outcomes ~f:(fun (_, o) -> Bind.target_of_outcome o))
      }
    in
    let self_types i = List.nth group i |> Option.bind ~f:sig_of in
    ignore np;
    let env =
      { Typecheck.builtin = Builtins.type_of
      ; Typecheck.def = (fun h -> Store.type_of s.Session.store h)
      ; Typecheck.self = self_types
      }
    in
    let status, ok =
      match sig_of name with
      | Some t ->
        (match Typecheck.check_top env link a.Open_term.body t with
         | Ok () -> "checks : " ^ Ty.to_string t, true
         | Error m -> m, false)
      | None ->
        (match Typecheck.synth_top env link a.Open_term.body with
         | Ok t -> "infers : " ^ Ty.to_string t, true
         | Error m -> m, false)
    in
    { name
    ; declared =
        (match sig_of name with
         | Some t -> Ty.to_string t
         | None -> "")
    ; is_edit = Option.is_some (Namespace.resolve s.Session.ns name)
    ; in_cycle = not (List.is_empty cycle)
    ; arity = a.Open_term.arity
    ; open_hash = Hash.short ~len:8 (Open_term.hash a.Open_term.body)
    ; slots
    ; status
    ; ok
    }
  in
  (* Walk the plan in order, accumulating the names earlier units will have
     defined by the time each later one is committed. *)
  let previews, _ =
    List.fold
      (Substrate.plan_of_file file)
      ~init:([], [])
      ~f:(fun (acc, pending) unit_ ->
        match unit_ with
        | Substrate.Single (n, src) ->
          acc @ [ preview_of ~pending ~cycle:[] (n, src) ], pending @ [ n ]
        | Substrate.Group members ->
          let cycle = List.map members ~f:fst in
          ( acc @ List.map members ~f:(preview_of ~pending ~cycle)
          , pending @ cycle ))
  in
  { previews; errors; below_fold = file.Surface.below_fold }

let commit ~(strategy : Propagate.strategy) (text : string) : string * string =
  let results, errors = Substrate.commit_buffer ~strategy text in
  if not (List.is_empty errors)
  then String.concat ~sep:"; " errors, "err"
  else if List.is_empty results
  then "nothing to commit", ""
  else
    ( String.concat
        ~sep:"; "
        (List.map results ~f:(fun (n, what) -> n ^ ": " ^ what))
    , "ok" )

(* Render a stored definition back into the buffer, for editing. Display only --
   nothing propagates through this text. *)
let edit_into_buffer ~(buffer : string) ~(name : string) : string =
  let s = Substrate.global in
  match Namespace.resolve s.Session.ns name with
  | None -> buffer
  | Some h ->
    (match Store.get_def s.Session.store h with
     | None -> buffer
     | Some d ->
       (match Store.open_of s.Session.store d, Store.link_of s.Session.store d with
        | Some o, Some _l ->
          let slot_names =
            match d with
            | Def.Simple (_, lh) ->
              let n = Open_term.arity_of o in
              Array.init n ~f:(fun i -> Store.slot_name s.Session.store ~link:lh ~slot:i)
            | Def.Member (_, _) -> [||]
          in
          let ty =
            Option.value
              (Store.type_of s.Session.store h)
              ~default:Ty.Nat
          in
          let rendered = Pretty.definition ~slot_names name o ty in
          if String.is_substring buffer ~substring:rendered
          then buffer
          else String.rstrip buffer ^ "\n\n" ^ rendered
        | _ -> buffer))
