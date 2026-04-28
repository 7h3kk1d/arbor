/* DEEP AST — internal, name-free, in de Bruijn form.

   What Store.ingest consumes and Store.reconstruct produces. The parser
   does NOT produce this type directly; it produces Surface_ast.t
   (with string binders and variable names), which Resolver.resolve
   walks with a context stack to produce an Ast.t.

   Binders carry no names: a Lam is just a body. Variables are integer
   de Bruijn indices, 0 = innermost enclosing binder. Two α-equivalent
   surface terms produce identical Ast.t values (and identical hashes
   after Store.ingest). That is the whole point of storing in de Bruijn
   form: α-equivalence is resolved at hashing time.

   p8 adds `Hole` as a fourth constructor. Holes are opaque leaves
   with no payload: `\x. ?` and `\y. ?` produce identical Ast.t values
   and thus identical content hashes, just as `\x. x` and `\y. y` do.
   Holes are name-free and never bind or reference variables, so they
   don't interact with shift/subst beyond the trivial identity arm. */

[@deriving (eq, show, ord)]
type t =
  | Var(int)
  | Lam(t)
  | App(t, t)
  | Hole;

/* Highest de Bruijn index that refers to a *free* variable in t,
   given a binder depth `d` at the point we look at t. None if t is
   closed under d binders. Useful for tests and assertions. */
let rec max_free_index = (~depth: int=0, t: t): option(int) =>
  switch (t) {
  | Var(k) => k >= depth ? Some(k - depth) : None
  | Hole => None
  | Lam(body) => max_free_index(~depth=depth + 1, body)
  | App(f, a) =>
    switch (max_free_index(~depth, f), max_free_index(~depth, a)) {
    | (None, b) => b
    | (a, None) => a
    | (Some(x), Some(y)) => Some(max(x, y))
    }
  };

let is_closed = (t: t): bool => max_free_index(t) == None;

/* TAPL Ch. 6 shift: ↑_d^c(t). Shift free variables of t (those with
   index ≥ cutoff c) by d. d may be negative, in which case the caller
   must ensure no post-shift index goes below c. */
let rec shift = (~cutoff: int, ~by: int, t: t): t =>
  switch (t) {
  | Var(k) =>
    if (k < cutoff) {
      Var(k);
    } else {
      Var(k + by);
    }
  | Hole => Hole
  | Lam(body) => Lam(shift(~cutoff=cutoff + 1, ~by, body))
  | App(f, a) => App(shift(~cutoff, ~by, f), shift(~cutoff, ~by, a))
  };

/* TAPL Ch. 6 substitution: [j ↦ s]t. Replace every occurrence of Var(j)
   in t (accounting for binders) with s. s is shifted as needed when
   crossing binders. */
let rec subst = (~j: int, ~s: t, t: t): t =>
  switch (t) {
  | Var(k) => k == j ? s : Var(k)
  | Hole => Hole
  | Lam(body) =>
    Lam(subst(~j=j + 1, ~s=shift(~cutoff=0, ~by=1, s), body))
  | App(f, a) => App(subst(~j, ~s, f), subst(~j, ~s, a))
  };

/* Beta-reduce (Lam body) applied to arg. Convention from TAPL:
     (λ. t) v  →  ↑_{-1}^0 ([0 ↦ ↑_1^0(v)] t) */
let beta = (~body: t, ~arg: t): t =>
  shift(
    ~cutoff=0,
    ~by=-1,
    subst(~j=0, ~s=shift(~cutoff=0, ~by=1, arg), body),
  );
