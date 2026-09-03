(* The substrate pipeline, as the buffer drives it.

   Read-only analysis on every keystroke, via Scratch.analyze; one mutating
   gesture, commit. Note that this module no longer recomputes the pipeline
   itself — Scratch owns it, so the same value the UI renders is the value the
   commit path would consume. *)
open! Core
open P21_substrate

(* --- live analysis ------------------------------------------------------- *)

let analyze ~(s : Substrate.t) ~(buffer : string) : Scratch.t =
  Scratch.analyze ~st:s.store ~ns:s.ns buffer

(* Evaluate a watch, but only when it is evaluable: a watch mentioning a binding
   the buffer has not committed has no stored body to run. That is not a
   limitation of the display, it is what "identity is conferred by typechecking"
   means at the granularity of one expression. *)
(* The value the save produced. There is no longer any "evaluates on commit"
   case: a watch is evaluated at save, against an overlay carrying the buffer's
   own definitions, so it can mention a binding nothing has committed. *)
let watch_value (w : Scratch.watch) : string =
  match w.error, w.eval_error, w.value with
  | Some m, _, _ -> "type error: " ^ m
  | _, Some m, _ -> m
  | _, _, Some v -> Pretty.term (Ppe.make (Names.create ())) v
  | _, _, None -> "—" 

(* --- commit -------------------------------------------------------------- *)

type commit_result =
  { message : string
  ; kind : string
  ; new_buffer : string option
  }

let commit ~(s : Substrate.t) ~(buffer : string) : commit_result =
  let text, _below = Parse.split_fold buffer in
  let file, perrs = Parse.file buffer in
  if not (List.is_empty perrs)
  then
    { message =
        "parse error: "
        ^ String.concat
            ~sep:"; "
            (List.map perrs ~f:(fun (e : Parse.located_error) ->
               Int.to_string e.line ^ ": " ^ e.message))
    ; kind = "err"
    ; new_buffer = None
    }
  else (
    match Update.run ~st:s.store ~ns:s.ns ~w:s.watch ~text ~file with
    | Update.Conflicted n ->
      { message =
          "refused: the name `"
          ^ n
          ^ "` is conflicted. update asserts unconflictedness before doing \
             anything (rem:conflict)."
      ; kind = "err"
      ; new_buffer = None
      }
    | Update.Succeeded o ->
      let bound = List.map o.bound ~f:fst in
      let rebuilt =
        if List.is_empty o.rebuilt
        then ""
        else " Rebuilt " ^ String.concat ~sep:", " o.rebuilt ^ "."
      in
      let watched =
        if List.is_empty o.watched
        then ""
        else
          " Cached "
          ^ Int.to_string (List.length o.watched)
          ^ (if List.length o.watched = 1 then " watch: " else " watches: ")
          ^ String.concat
              ~sep:", "
              (List.map o.watched ~f:(fun (r, ty, v) ->
                 Reference.to_string r ^ " = " ^ v ^ " : " ^ Ty.to_string ty))
          ^ "."
      in
      { message =
          (if List.is_empty bound
           then "committed nothing."
           else "committed " ^ String.concat ~sep:", " bound ^ ".")
          ^ rebuilt
          ^ watched
      ; kind = "ok"
      ; new_buffer = None
      }
    | Update.Failed f ->
      let below = snd (Parse.split_fold buffer) in
      let restored =
        if String.is_empty (String.strip below)
        then f.buffer
        else f.buffer ^ "\n\n---\n" ^ below
      in
      { message =
          (if List.is_empty f.evicted
           then "not committed: " ^ f.message
           else
             "not committed: "
             ^ f.message
             ^ " — evicted "
             ^ String.concat ~sep:", " f.evicted
             ^ " from the namespace; their source is now in the buffer \
                (rem:step5).")
      ; kind = "err"
      ; new_buffer = Some restored
      })

(* `edit foo` — render a stored definition back into the buffer. Names already in
   the buffer are skipped, so re-editing is idempotent. *)
let edit_into_buffer ~(s : Substrate.t) ~(buffer : string) ~(name : string) : string =
  let file, _ = Parse.file buffer in
  let entries, _ = Slurp.entries file in
  if List.exists entries ~f:(fun (n, _, _) -> String.equal n name)
  then buffer
  else (
    match Names.resolve s.ns name with
    | None -> buffer
    | Some referent ->
      let r = Reference.referent_ref referent in
      (match Store.lookup s.store r, Store.type_of s.store r with
       | Some tm, Some t -> Pretty.definition (Ppe.make s.ns) name tm t ^ "\n\n" ^ buffer
       | _ -> buffer))

(* --- codebase browser ---------------------------------------------------- *)

type comp_view =
  { hash : string
  ; members : (int * string list * string) list
  }

let components ~(s : Substrate.t) : comp_view list =
  List.map (Store.all_hashes s.store) ~f:(fun h ->
    let n = Store.component_size s.store h in
    let members =
      List.init n ~f:(fun i ->
        let r = Reference.Derived (h, i) in
        let names = Names.names_of s.ns (Reference.Ref r) in
        let ty =
          match Store.type_of s.store r with
          | Some t -> Ty.to_string t
          | None -> "?"
        in
        (i, names, ty))
    in
    { hash = h; members })

let value_of ~(s : Substrate.t) (r : Reference.t) : string =
  match Eval.eval_ref s.store s.watch r with
  | Ok tm -> Pretty.term (Ppe.make s.ns) tm
  | Error m -> m
