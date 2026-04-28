/* SURFACE AST — what the parser produces and the printer renders.

   Carries string names for binders and variables. Those names are
   either (a) bound by an enclosing Lam in the same term, or (b) free
   — to be resolved against the definition-level Namespace at resolve
   time. Either way, the name does not survive past the Resolver:
   Resolver.resolve produces an Ast.t where all names are gone, binders
   are anonymous, and variables are de Bruijn indices.

   p8 adds the `Hole` constructor: a placeholder leaf. Two sources:
   an explicit `?` in user-entered syntax, or an implicit insertion by
   Parse_recover when the grammar couldn't otherwise consume the
   input. All holes are structurally equal (no payload) — the
   canonical-form question in docs/design/open-questions.md answered
   by "bare hole" for this prototype. */

[@deriving (eq, show, ord)]
type t =
  | Var(string)
  | Lam(string, t)
  | App(t, t)
  | Hole;

/* Count holes in a surface term. Useful for the REPL's "N holes
   inserted" banner and for test assertions. */
let rec count_holes = (t: t): int =>
  switch (t) {
  | Var(_) => 0
  | Hole => 1
  | Lam(_, body) => count_holes(body)
  | App(f, a) => count_holes(f) + count_holes(a)
  };
