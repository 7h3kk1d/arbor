/* Canonicalization slot for λ-calculus. The Resolver already produces
   de Bruijn form (α-canonical by construction), so this step is the
   identity. Kept as the hook for any future per-language normalization
   (η-reduction, etc.). */

let canonicalize = (t: Lc_ast.t): Lc_ast.t => t;
