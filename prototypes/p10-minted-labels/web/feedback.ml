(* Keystroke→ingest pipeline. Pure compute; called from the editor's
   on_input handler. The substrate ingest happens in here, not in
   apply_action, so views see fresh state on the next render. *)

open! Core
open P10_minted_labels_substrate

let format_type_result ~(ns : Namespace.t) :
    Typecheck.check_result -> string = function
  | Well_typed ty -> Pretty.print_ty_named ~namespace:ns ty
  | Well_typed_with_holes ty ->
      Pretty.print_ty_named ~namespace:ns ty
      ^ "  (best guess; contains holes)"
  | Ill_typed _ -> "ill-typed"

let compute_ty ~(s : Substrate.t) ~(buffer : string) :
    State.ty_ingest_result =
  if String.is_empty (String.strip buffer) then State.Ty_empty
  else
    let surface = Parse_recover.parse_ty buffer in
    let hole_count = Surface_ty.count_holes surface in
    let outcome : State.ty_ingest_outcome =
      match Resolver.ingest_ty ~namespace:s.ns ~store:s.store surface with
      | Ok r ->
          let resolved =
            Resolver.collect_resolved_names_ty ~namespace:s.ns surface
          in
          State.Ty_ingested
            {
              hash = r.hash;
              was_new = r.was_new;
              ty_summary = Pretty.print_ty_named ~namespace:s.ns r.ty;
              resolved;
            }
      | Error (Unbound_name n) -> State.Ty_resolve_unbound n
      | Error (Ambiguous_name (n, cs)) ->
          State.Ty_resolve_ambiguous { name = n; candidates = cs }
      | Error (Kind_mismatch _ as e) ->
          State.Ty_kind_mismatch (Resolver.error_to_string e)
      | Error (Missing_hash (n, h)) ->
          State.Ty_kind_mismatch
            (Printf.sprintf "missing hash for %s -> %s" n (Hash.short h))
      | Error (Type_error msg) -> State.Ty_kind_mismatch msg
    in
    State.Ty_recovered { surface; hole_count; ingest = outcome }

let compute ~(s : Substrate.t) ~(buffer : string) : State.ingest_result =
  if String.is_empty (String.strip buffer) then State.Empty
  else
    let surface = Parse_recover.parse buffer in
    let hole_count = Surface_ast.count_holes surface in
    let outcome : State.ingest_outcome =
      match Resolver.ingest ~namespace:s.ns ~store:s.store ~att:s.att surface with
      | Ok r ->
          let resolved =
            Resolver.collect_resolved_names ~namespace:s.ns surface
          in
          State.Ingested
            {
              hash = r.hash;
              was_new = r.was_new;
              type_summary = format_type_result ~ns:s.ns r.type_result;
              has_holes = r.has_holes;
              resolved;
            }
      | Error (Unbound_name n) -> State.Resolve_unbound n
      | Error (Ambiguous_name (n, cs)) ->
          State.Resolve_ambiguous { name = n; candidates = cs }
      | Error (Missing_hash (n, h)) -> State.Resolve_missing_hash (n, h)
      | Error (Kind_mismatch _ as e) ->
          State.Type_error (Resolver.error_to_string e)
      | Error (Type_error msg) -> State.Type_error msg
    in
    State.Recovered { surface; hole_count; ingest = outcome }
