(* Keystroke→ingest pipeline. Pure compute; called from the editor's
   on_input handler. The substrate ingest happens in here, not in
   apply_action, so views see fresh state on the next render. *)

open! Core
open P9_typed_namespaces_substrate

let format_type_result : Typecheck.check_result -> string = function
  | Well_typed ty -> Ty.print ty
  | Well_typed_with_holes ty -> Ty.print ty ^ "  (best guess; contains holes)"
  | Ill_typed _ -> "ill-typed"

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
              type_summary = format_type_result r.type_result;
              has_holes = r.has_holes;
              resolved;
            }
      | Error (Unbound_name n) -> State.Resolve_unbound n
      | Error (Ambiguous_name (n, cs)) ->
          State.Resolve_ambiguous { name = n; candidates = cs }
      | Error (Missing_hash (n, h)) -> State.Resolve_missing_hash (n, h)
      | Error (Type_error msg) -> State.Type_error msg
    in
    State.Recovered { surface; hole_count; ingest = outcome }
