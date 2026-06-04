(* The substrate pipeline behind the editor's buttons: parse -> resolve ->
   commit / create / eval, returning a State.feedback. Mutations (ingest,
   namespace bind) happen here; app.ml bumps the version so views refresh. *)

open! Core
open P12_substrate

let ctx_of_open_set (open_set : string list) : Editing_context.t =
  let ctx = Editing_context.make () in
  List.iter open_set ~f:(fun h -> Editing_context.open_type ctx h);
  ctx

(* :let — minimal sealing decides Normal vs Sealed inside commit. *)
let define ~(s : Substrate.t) ~(open_set : string list) ~(name : string)
    ~(ty : string) ~(expr : string) : State.feedback =
  if String.is_empty (String.strip name) then State.Err "name is required"
  else
    match Parse.parse_ty ty, Parse.parse_expr expr with
    | Error e, _ -> State.Err ("type: " ^ e)
    | _, Error e -> State.Err ("expr: " ^ e)
    | Ok sty, Ok sexpr -> (
        match Resolver.resolve_ty ~ns:s.ns ~st:s.store sty with
        | Error e -> State.Err e
        | Ok ann -> (
            match Resolver.resolve ~ctx:[] ~ns:s.ns ~st:s.store sexpr with
            | Error e -> State.Err e
            | Ok node -> (
                let ctx = ctx_of_open_set open_set in
                match Editing_context.commit s.store ctx ~term:node ~ann with
                | Error e -> State.Err e
                | Ok (h, k) ->
                    (try Namespace.rebind s.ns ~name h with _ -> ());
                    let kind =
                      match k with `Sealed -> State.Sealed | `Normal -> State.Normal
                    in
                    State.Bound
                      {
                        name;
                        kind;
                        ty = Pretty.ty ~ns:s.ns ~st:s.store ann;
                        hash = Hash.short h;
                      })))

(* New type: abstract mints a fresh opaque (returns its hash so the caller can
   auto-open it); concrete binds the witness hash directly (an alias). *)
let create_type ~(s : Substrate.t) ~(name : string) ~(body : string)
    ~(abstract : bool) : State.feedback * string option =
  if String.is_empty (String.strip name) then (State.Err "name is required", None)
  else
    match Parse.parse_ty body with
    | Error e -> (State.Err ("type: " ^ e), None)
    | Ok sty -> (
        match Resolver.resolve_ty ~ns:s.ns ~st:s.store sty with
        | Error e -> (State.Err e, None)
        | Ok witness ->
            if abstract then (
              let m = Mint.fresh s.mint_src in
              let opaque =
                Store.ingest_type s.store (Tnode.Opaque { mint = m; witness })
              in
              (try Namespace.rebind s.ns ~name opaque with _ -> ());
              ( State.Info
                  (Printf.sprintf "abstract type %s = opaque(%s) over %s" name
                     (Mint.short m)
                     (Pretty.ty ~ns:s.ns ~st:s.store witness)),
                Some opaque ))
            else (
              (try Namespace.rebind s.ns ~name witness with _ -> ());
              ( State.Info
                  (Printf.sprintf "type %s = %s" name
                     (Pretty.ty ~ns:s.ns ~st:s.store witness)),
                None )))

(* A test's status: derived live by evaluating it (no caching). *)
let test_status (s : Substrate.t) (h : Hash.t) : [ `Pass | `Fail | `Error of string ] =
  match Eval.eval_top s.store (Node.Ref h) with
  | Ok (Eval.VBool true) -> `Pass
  | Ok (Eval.VBool false) -> `Fail
  | Ok _ -> `Error "not a boolean"
  | Error m -> `Error m

(* Evaluate a bare expression under the current open set; show type + value. *)
let eval ~(s : Substrate.t) ~(open_set : string list) ~(expr : string) :
    State.feedback =
  if String.is_empty (String.strip expr) then State.Empty
  else
    match Parse.parse_expr expr with
    | Error e -> State.Err e
    | Ok sexpr -> (
        match Resolver.resolve ~ctx:[] ~ns:s.ns ~st:s.store sexpr with
        | Error e -> State.Err e
        | Ok node -> (
            let env = Store.build_env s.store in
            match Typecheck.synth_top env open_set node with
            | Error e -> State.Err ("type error: " ^ e)
            | Ok t -> (
                let ty = Pretty.ty ~ns:s.ns ~st:s.store t in
                match Eval.eval_top s.store node with
                | Ok v -> State.Typed { ty; value = Eval.to_string v }
                | Error m -> State.Typed { ty; value = "(stuck: " ^ m ^ ")" })))
