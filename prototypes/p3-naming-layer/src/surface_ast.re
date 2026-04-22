/* SURFACE AST — what the parser produces and the printer renders.
   Mirrors Ast.t exactly except it can also hold Name(string) leaves.

   A Name(s) leaf means "substitute the hash bound to `s` in the namespace."
   Name leaves are eliminated by Resolver.resolve before Store.ingest runs,
   so the Store never holds a Name. Keeping the surface and internal types
   structurally distinct makes that invariant a type-level fact. */

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

let rec of_ast: Ast.t => t =
  fun
  | Ast.True => True
  | Ast.False => False
  | Ast.Zero => Zero
  | Ast.Succ(a) => Succ(of_ast(a))
  | Ast.Pred(a) => Pred(of_ast(a))
  | Ast.IsZero(a) => IsZero(of_ast(a))
  | Ast.If(c, t, e) => If(of_ast(c), of_ast(t), of_ast(e));
