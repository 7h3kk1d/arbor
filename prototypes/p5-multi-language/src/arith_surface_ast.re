/* ARITH SURFACE AST — what the arith parser produces and the arith
   printer renders. Mirrors Arith_ast.t except it can also hold
   Name(string) leaves, eliminated by Resolver.resolve_arith before
   Store.ingest_arith runs.

   Carried from p3 verbatim (renamed from Surface_ast to avoid
   collision with the lc surface AST). */

[@deriving (eq, show, ord)]
type t =
  | True
  | False
  | Zero
  | Succ(t)
  | Pred(t)
  | IsZero(t)
  | If(t, t, t)
  | Name(string);

let rec of_ast: Arith_ast.t => t =
  fun
  | Arith_ast.True => True
  | Arith_ast.False => False
  | Arith_ast.Zero => Zero
  | Arith_ast.Succ(a) => Succ(of_ast(a))
  | Arith_ast.Pred(a) => Pred(of_ast(a))
  | Arith_ast.IsZero(a) => IsZero(of_ast(a))
  | Arith_ast.If(c, t, e) => If(of_ast(c), of_ast(t), of_ast(e));
