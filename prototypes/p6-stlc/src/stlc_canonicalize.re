/* Canonicalization slot for STLC. The Resolver already produces de
   Bruijn form (α-canonical by construction); we additionally run
   Ty.canonicalize on every Lam annotation so that syntactically equal
   types at different binders share the same canonical form. Today
   Ty.canonicalize is the identity, so this walk is effectively a
   no-op, but it's the hook for future type-level normalization. */

let rec canonicalize = (t: Stlc_ast.t): Stlc_ast.t =>
  switch (t) {
  | Stlc_ast.Var(_)
  | Stlc_ast.True
  | Stlc_ast.False => t
  | Stlc_ast.Lam(ty, body) =>
    Stlc_ast.Lam(Ty.canonicalize(ty), canonicalize(body))
  | Stlc_ast.App(f, a) => Stlc_ast.App(canonicalize(f), canonicalize(a))
  | Stlc_ast.If(c, th, el) =>
    Stlc_ast.If(canonicalize(c), canonicalize(th), canonicalize(el))
  };
