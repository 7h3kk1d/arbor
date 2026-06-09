(* The substrate pipeline behind the editor's buttons: parse -> resolve ->
   commit / create / eval, returning a State.feedback. Mutations (ingest,
   namespace bind) happen here; app.ml bumps the version so views refresh. *)

open! Core
open P17_substrate

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
                | Ok sty ->
                    Resolver.resolve_ty ~ns:s.ns ~st:s.store ~mint:(Some s.mint_src) sty
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
        match Resolver.resolve_ty ~ns:s.ns ~st:s.store ~mint:(Some s.mint_src) sty with
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

(* leaf segment of a (possibly dotted) label name *)
let leaf nm = match String.rsplit2 nm ~on:'.' with Some (_, l) -> l | None -> nm

(* Generative n-ary open of a module [node] (a Sig-typed value): ONE Open node
   mints one fresh witness-less Abstract per opaque component. Every binding's
   name is recovered from the sig's own labels — `name.t` per type component
   (manifest components bind to their equations: the translucency payoff),
   `name` for the module, `name.f` per value member. Re-opening mints fresh,
   incompatible abstracts. *)
let open_module_node ~(s : Substrate.t) ~(name : string) ~(node : Node.t) :
    State.feedback =
  let name = String.strip name in
  if String.is_empty name then State.Err "module name is required"
  else
    match Open_module.open_package s.store s.mint_src node with
    | Error e -> State.Err ("open: " ^ e)
    | Ok { type_bindings; module_hash; fields } ->
        let bind_labeled (lh, h) =
          match Namespace.name_of s.ns lh with
          | Some nm -> (
              try Namespace.rebind s.ns ~name:(name ^ "." ^ leaf nm) h with _ -> ())
          | None -> ()
        in
        List.iter type_bindings ~f:bind_labeled;
        (try Namespace.rebind s.ns ~name module_hash with _ -> ());
        List.iter fields ~f:bind_labeled;
        let tystr =
          match Store.type_of s.store module_hash with
          | Some t -> Pretty.ty ~ns:s.ns ~st:s.store t
          | None -> "?"
        in
        let tlist =
          String.concat ~sep:", "
            (List.filter_map type_bindings ~f:(fun (lh, _) ->
                 Option.map (Namespace.name_of s.ns lh) ~f:(fun nm ->
                     name ^ "." ^ leaf nm)))
        in
        State.Info (Printf.sprintf "opened %s [%s];  %s : %s" name tlist name tystr)

(* Parse + resolve an expression, then open it (work-area gesture). *)
let open_module ~(s : Substrate.t) ~(name : string) ~(expr : string) :
    State.feedback =
  match Parse.parse_expr expr with
  | Error e -> State.Err ("expr: " ^ e)
  | Ok se -> (
      match Resolver.resolve ~ctx:[] ~ns:s.ns ~st:s.store se with
      | Error e -> State.Err e
      | Ok node -> open_module_node ~s ~name ~node)

(* Is the definition at [h] a term whose type is a sig? Used by the detail
   pane to show its one-click open affordance. *)
let is_module_term (s : Substrate.t) (h : Hash.t) : bool =
  match Store.find s.store h with
  | Some (Definition.Term _) -> (
      match Store.type_of s.store h with
      | Some t -> (
          match Store.find s.store t with
          | Some (Definition.Type (Tnode.Sig _)) -> true
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
                (* a module's sig drives the open form: every binding it would
                   create is previewed by name, types included *)
                let label_name lh =
                  match Namespace.name_of s.ns lh with
                  | Some nm -> leaf nm
                  | None -> Hash.short lh
                in
                let open_shape =
                  match Open_module.inspect s.store node with
                  | Some { type_comps; val_comps } ->
                      (* opaques come first in rank order, so their leaf names
                         are exactly the tenv for printing the member types *)
                      let tenv =
                        List.filter_map type_comps ~f:(fun (lh, m) ->
                            match m with None -> Some (label_name lh) | Some _ -> None)
                      in
                      Some
                        {
                          State.type_comps =
                            List.map type_comps ~f:(fun (lh, m) ->
                                {
                                  State.cname = label_name lh;
                                  cty =
                                    (match m with
                                     | None -> "type (opaque — mints fresh)"
                                     | Some eq ->
                                         "type = "
                                         ^ Pretty.ty_to_string ~ns:s.ns ~st:s.store
                                             ~prec:0 ~tenv eq);
                                });
                          fields =
                            List.map val_comps ~f:(fun (lh, ft) ->
                                {
                                  State.cname = label_name lh;
                                  cty =
                                    Pretty.ty_to_string ~ns:s.ns ~st:s.store ~prec:0
                                      ~tenv ft;
                                });
                        }
                  | None -> None
                in
                match Eval.eval_top s.store node with
                | Ok v -> State.Typed { ty; value = Eval.to_string v; open_shape }
                | Error m -> State.Typed { ty; value = "(stuck: " ^ m ^ ")"; open_shape })))
