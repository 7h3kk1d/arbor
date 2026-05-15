/* Canonicalization slot. The Resolver already produces de Bruijn form,
   which is α-canonical by construction — so this step is the identity
   for both Lam and Let bindings. The module is kept for symmetry with
   p4-p6 and as the hook for any future per-prototype normalization. */

let canonicalize = (t: Ast.t): Ast.t => t;
