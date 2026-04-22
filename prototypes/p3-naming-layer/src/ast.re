/* DEEP AST — internal, name-free. What Store.ingest consumes and
   Store.reconstruct produces. The parser does NOT produce this type
   directly in p3; it produces Surface_ast.t (with Name leaves), which
   Resolver.resolve substitutes into an Ast.t before ingest.

   By keeping Ast.t distinct from Surface_ast.t, "no Name constructor in
   the Store" is a type-level invariant, not a runtime check. */

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
