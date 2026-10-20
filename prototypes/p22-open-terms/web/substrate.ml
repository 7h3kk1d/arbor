(* The substrate singleton, and the buffer the app opens on.

   Bonsai's model holds UI state only. The store, namespace, cache and operation
   log are a module-level mutable value, because they are not a function of the
   view and re-deriving them on every render would be a lie about what they
   are. Every mutation bumps State.version to force a re-render. *)
open! Core
open P22_substrate

let global : Session.t = Session.create ()

(* A seed that exists to make the three histories non-trivial the moment the app
   opens: `scale` and `double` are carried along by edits to `base` that nobody
   made to them. *)
let seed_buffer =
  String.concat
    ~sep:"\n"
    [ "base : Nat"
    ; "base = 1"
    ; ""
    ; "scale : Nat -> Nat"
    ; "scale = \\n. Nat.* n base"
    ; ""
    ; "double : Nat -> Nat"
    ; "double = \\n. scale n"
    ]

let ty_of_stanzas stanzas name =
  List.find_map stanzas ~f:(function
    | Surface.Signature (n, t) when String.equal n name -> Some t
    | _ -> None)

(* A buffer's bindings, decomposed into the units that get committed together.

   Only a genuine cycle becomes a Group. Everything else is committed one at a
   time, in dependency order, so that a definition's references resolve through
   the namespace to things already stored. That matters for more than tidiness:
   a Group becomes a Bundle, and a Bundle is the one place in p22 where a
   definition carries an index (def.re). Treating the whole buffer as a group --
   which is what p21 must do, since its ingest is group-at-a-time -- would put
   every definition in a bundle and throw away the confinement. *)
type unit_plan =
  | Single of string * Term.Source.t
  | Group of (string * Term.Source.t) list

let plan_of_file (file : Surface.file) : unit_plan list =
  let bindings =
    List.filter_map file.Surface.stanzas ~f:(function
      | Surface.Binding (n, src) -> Some (n, src)
      | _ -> None)
  in
  let names = List.map bindings ~f:fst in
  let graph =
    List.map bindings ~f:(fun (n, src) ->
      ( n
      , List.filter (Term.free_vars src) ~f:(fun v ->
          List.mem names v ~equal:String.equal) ))
  in
  List.filter_map (Scc.components graph) ~f:(fun comp ->
    let members =
      List.filter_map comp ~f:(fun n ->
        List.find_map bindings ~f:(fun (m, src) ->
          if String.equal m n then Some (n, src) else None))
    in
    match members with
    | [] -> None
    | [ (n, src) ] ->
      let self_referential =
        List.mem (Term.free_vars src) n ~equal:String.equal
      in
      Some (if self_referential then Group members else Single (n, src))
    | _ -> Some (Group members))

(* Bind everything in a buffer. A name that already resolves is an edit, which
   propagates; a name that does not is a definition. *)
let commit_buffer ~(strategy : Propagate.strategy) (text : string) :
    (string * string) list * string list =
  let file, errs = Parse.file text in
  let errors = List.map errs ~f:(fun (e : Parse.located_error) -> e.Parse.message) in
  let results = ref [] in
  let say name what = results := !results @ [ (name, what) ] in
  List.iter (plan_of_file file) ~f:(function
    | Single (name, src) ->
      let ty = ty_of_stanzas file.Surface.stanzas name in
      (match Namespace.resolve global.Session.ns name with
       | Some _ ->
         (match Session.edit global ?ty ~strategy name src with
          | Ok (_, rewrites) ->
            say
              name
              (Printf.sprintf
                 "edited, %d dependent%s relinked"
                 (List.length rewrites)
                 (if List.length rewrites = 1 then "" else "s"))
          | Error m -> say name ("error: " ^ m)
          | exception e -> say name ("raised: " ^ Exn.to_string e))
       | None ->
         (match Session.define global ?ty name src with
          | r -> say name ("defined " ^ Hash.short r.Bind.hash)
          | exception e -> say name ("raised: " ^ Exn.to_string e)))
    | Group members ->
      let types =
        List.filter_map members ~f:(fun (n, _) ->
          Option.map (ty_of_stanzas file.Surface.stanzas n) ~f:(fun t -> n, t))
      in
      (match Session.define_group global ~types members with
       | bound ->
         List.iter bound ~f:(fun (n, (r : Bind.result_t)) ->
           say n ("defined in a bundle of " ^ Int.to_string (List.length bound)
                  ^ ": " ^ Hash.short r.Bind.hash))
       | exception e ->
         List.iter members ~f:(fun (n, _) -> say n ("raised: " ^ Exn.to_string e))));
  !results, errors

