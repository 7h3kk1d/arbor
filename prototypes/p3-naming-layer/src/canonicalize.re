/* Identity for untyped arithmetic — no α, no reorderings.
   Applies to the deep Ast.t before ingest. Kept as a module slot so
   future languages can plug their own canonicalization in. */

let canonicalize: Ast.t => Ast.t = t => t;
