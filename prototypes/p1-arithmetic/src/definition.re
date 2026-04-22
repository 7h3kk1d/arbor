/* At Phase 1 scale, Definition.t is a type alias for Ast.t — the full
   sum-over-languages model collapses to a single type. When a second
   language arrives the alias becomes a sum; callers barely notice. */

type t = Ast.t;

let hash = (d: t): Hash.t => Hash.of_ast(Canonicalize.canonicalize(d));
