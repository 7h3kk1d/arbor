/* Identity. Arithmetic has no α, no reorderings — the deep Arith_ast.t
   is its own canonical form. Kept as a module slot for symmetry with
   the lc canonicalizer. */

let canonicalize: Arith_ast.t => Arith_ast.t = t => t;
