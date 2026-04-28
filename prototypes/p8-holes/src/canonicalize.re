/* Canonicalization slot. For untyped λ-calculus, the Resolver already
   produces de Bruijn form, which is α-canonical by construction — so
   this step is the identity. The module is kept for symmetry with the
   other language layers and as the hook for any future per-language
   normalization (η-reduction, reordering of mutually recursive groups,
   etc.). */

let canonicalize = (t: Ast.t): Ast.t => t;
