/* SURFACE AST — what the parser produces and the printer renders.

   Carries string names for binders and variables. Those names are
   either (a) bound by an enclosing Lam in the same term, or (b) free
   — to be resolved against the definition-level Namespace at resolve
   time. Either way, the name does not survive past the Resolver:
   Resolver.resolve produces an Ast.t where all names are gone, binders
   are anonymous, and variables are de Bruijn indices. */

[@deriving (eq, show, ord)]
type t =
  | Var(string)
  | Lam(string, t)
  | App(t, t);
