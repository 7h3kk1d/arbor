/* ARITH DEEP AST — internal, name-free. What Store.ingest_arith consumes
   and Store.reconstruct_arith produces. The parser produces
   Arith_surface_ast.t (with Name leaves); Resolver.resolve_arith
   substitutes those into an Arith_ast.t before ingest.

   Carried from p3 verbatim. */

[@deriving (eq, show, ord)]
type t =
  | True
  | False
  | Zero
  | Succ(t)
  | Pred(t)
  | IsZero(t)
  | If(t, t, t);

let rec is_numeric =
  fun
  | Zero => true
  | Succ(a) => is_numeric(a)
  | _ => false;
