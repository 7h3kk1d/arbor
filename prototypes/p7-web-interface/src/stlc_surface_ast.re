/* STLC SURFACE AST — what the stlc parser produces and the stlc
   printer renders. Carries string names for binders and variables, and
   type annotations on Lam; names are eliminated and annotations carried
   through by Resolver.resolve_stlc.

   TAPL Ch. 8 + Ch. 9: pure λ→ over a single Bool base with native
   true/false/if. */

[@deriving (eq, show, ord)]
type t =
  | Var(string)
  | Lam(string, Ty.t, t)
  | App(t, t)
  | True
  | False
  | If(t, t, t);
