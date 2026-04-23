/* LC SURFACE AST — what the lc parser produces and the lc printer
   renders. Carries string names for binders and variables; names are
   eliminated by Resolver.resolve_lc.

   Carried from p4 verbatim (renamed from Surface_ast to Lc_surface_ast). */

[@deriving (eq, show, ord)]
type t =
  | Var(string)
  | Lam(string, t)
  | App(t, t);
