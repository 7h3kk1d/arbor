(* The substrate pipeline behind the editor's buttons: parse -> resolve ->
   commit / create / eval, returning a State.feedback. Mutations (ingest,
   namespace bind) happen here; app.ml bumps the version so views refresh. *)

open! Core
open P15_substrate

let ctx_of_open_set (open_set : string list) : Editing_context.t =
  let ctx = Editing_context.make () in
  List.iter open_set ~f:(fun h -> Editing_context.open_type ctx h);
  ctx

(* :let — minimal sealing decides Normal vs Sealed inside commit. The annotation
   is optional: a blank [ty] is synthesized from the term under the current open
   set (the same type the scratch area shows), so binding a checked expression
   needs no second type entry. An explicit annotation is still honored (e.g. to
   bind at an opaque type, or to assert a particular shape). *)
let define ~(s : Substrate.t) ~(open_set : string list) ~(name : string)
    ~(ty : string) ~(expr : string) : State.feedback =
  if String.is_empty (String.strip name) then State.Err "name is required"
  else
    match Parse.parse_expr expr with
    | Error e -> State.Err ("expr: " ^ e)
    | Ok sexpr -> (
        match Resolver.resolve ~ctx:[] ~ns:s.ns ~st:s.store sexpr with
        | Error e -> State.Err e
        | Ok node -> (
            (* resolve the annotation: explicit if given, else synthesize *)
            let ann_result =
              if String.is_empty (String.strip ty) then (
                let env = Store.build_env s.store in
                match Typecheck.synth_top env open_set node with
                | Error e -> Error ("cannot infer a type (annotate it?): " ^ e)
                | Ok h -> Ok h)
              else
                match Parse.parse_ty ty with
                | Error e -> Error ("type: " ^ e)
                | Ok sty -> Resolver.resolve_ty ~ns:s.ns ~st:s.store sty
            in
            match ann_result with
            | Error e -> State.Err e
            | Ok ann -> (
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

(* Generative open of an existential package [node]: mints a fresh abstract type
   bound to N.t, binds N to the package value, and (optionally) binds each
   positional field by name. Mirrors the REPL gesture; each open is generative (a
   distinct Abstract mint), so re-opening makes an incompatible type. *)
let open_existential_node ~(s : Substrate.t) ~(name : string) ~(fields : string)
    ~(node : Node.t) : State.feedback =
  let name = String.strip name in
  if String.is_empty name then State.Err "module name is required"
  else
    let env = Store.build_env s.store in
    match Typecheck.synth_top env [] node with
    | Error e -> State.Err ("type error: " ^ e)
    | Ok ty_h -> (
        match Store.find s.store ty_h with
        | Some (Definition.Type (Tnode.Exists _)) -> (
            let m = Mint.fresh s.mint_src in
            let at = Store.ingest_type s.store (Tnode.Abstract m) in
            (try Namespace.rebind s.ns ~name:(name ^ ".t") at with _ -> ());
            match Store.ingest_term s.store (Node.Open { pkg = node; mint = m }) with
            | Error e -> State.Err ("open failed: " ^ e)
            | Ok oh ->
                (try Namespace.rebind s.ns ~name oh with _ -> ());
                let field_names =
                  String.split fields ~on:','
                  |> List.map ~f:String.strip
                  |> List.filter ~f:(fun x -> not (String.is_empty x))
                in
                let n = List.length field_names in
                let rec snds k nd =
                  if k <= 0 then nd else snds (k - 1) (Node.Snd nd)
                in
                List.iteri field_names ~f:(fun i f ->
                    let base = snds i (Node.Ref oh) in
                    let proj = if i = n - 1 then base else Node.Fst base in
                    match Store.ingest_term s.store proj with
                    | Ok ph -> (try Namespace.rebind s.ns ~name:(name ^ "." ^ f) ph with _ -> ())
                    | Error _ -> ());
                let tystr =
                  match Store.type_of s.store oh with
                  | Some t -> Pretty.ty ~ns:s.ns ~st:s.store t
                  | None -> "?"
                in
                let fields_msg =
                  if n = 0 then ""
                  else
                    "; bound "
                    ^ String.concat ~sep:", "
                        (List.map field_names ~f:(fun f -> name ^ "." ^ f))
                in
                State.Info
                  (Printf.sprintf "opened %s.t = abstract(%s);  %s : %s%s" name
                     (Mint.short m) name tystr fields_msg))
        | _ -> State.Err "open: this is not an existential package")

(* Parse + resolve an expression, then open it (work-area gesture). *)
let open_existential ~(s : Substrate.t) ~(name : string) ~(fields : string)
    ~(expr : string) : State.feedback =
  match Parse.parse_expr expr with
  | Error e -> State.Err ("expr: " ^ e)
  | Ok se -> (
      match Resolver.resolve ~ctx:[] ~ns:s.ns ~st:s.store se with
      | Error e -> State.Err e
      | Ok node -> open_existential_node ~s ~name ~fields ~node)

(* Is the definition at [h] a term whose type is an existential? Used by the
   detail pane to show its one-click open affordance. *)
let is_existential_term (s : Substrate.t) (h : Hash.t) : bool =
  match Store.find s.store h with
  | Some (Definition.Term _) -> (
      match Store.type_of s.store h with
      | Some t -> (
          match Store.find s.store t with
          | Some (Definition.Type (Tnode.Exists _)) -> true
          | _ -> false)
      | None -> false)
  | _ -> false

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
