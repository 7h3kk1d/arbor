/* STLC DEEP AST — internal, name-free, in de Bruijn form. What
   Store.ingest_stlc consumes and Store.reconstruct_stlc produces.

   Binders carry no names (just the type annotation), variables are
   integer de Bruijn indices (0 = innermost enclosing binder). Terms
   are TAPL Ch. 8 + Ch. 9: true, false, if, plus the usual λ-calculus
   core. shift/subst/beta carry over from p4's lc_ast — they traverse
   uniformly across all term constructors; type annotations have no
   term variables so they pass through untouched. */

[@deriving (eq, show, ord)]
type t =
  | Var(int)
  | Lam(Ty.t, t)
  | App(t, t)
  | True
  | False
  | If(t, t, t);

let rec max_free_index = (~depth: int=0, t: t): option(int) =>
  switch (t) {
  | Var(k) => k >= depth ? Some(k - depth) : None
  | Lam(_, body) => max_free_index(~depth=depth + 1, body)
  | App(f, a) =>
    switch (max_free_index(~depth, f), max_free_index(~depth, a)) {
    | (None, b) => b
    | (a, None) => a
    | (Some(x), Some(y)) => Some(max(x, y))
    }
  | True
  | False => None
  | If(c, t, e) =>
    let mix = (x, y) =>
      switch (x, y) {
      | (None, b) => b
      | (a, None) => a
      | (Some(x), Some(y)) => Some(max(x, y))
      };
    mix(
      max_free_index(~depth, c),
      mix(max_free_index(~depth, t), max_free_index(~depth, e)),
    );
  };

let is_closed = (t: t): bool => max_free_index(t) == None;

let rec shift = (~cutoff: int, ~by: int, t: t): t =>
  switch (t) {
  | Var(k) =>
    if (k < cutoff) {
      Var(k);
    } else {
      Var(k + by);
    }
  | Lam(ty, body) => Lam(ty, shift(~cutoff=cutoff + 1, ~by, body))
  | App(f, a) => App(shift(~cutoff, ~by, f), shift(~cutoff, ~by, a))
  | True => True
  | False => False
  | If(c, t, e) =>
    If(
      shift(~cutoff, ~by, c),
      shift(~cutoff, ~by, t),
      shift(~cutoff, ~by, e),
    )
  };

let rec subst = (~j: int, ~s: t, t: t): t =>
  switch (t) {
  | Var(k) => k == j ? s : Var(k)
  | Lam(ty, body) =>
    Lam(ty, subst(~j=j + 1, ~s=shift(~cutoff=0, ~by=1, s), body))
  | App(f, a) => App(subst(~j, ~s, f), subst(~j, ~s, a))
  | True => True
  | False => False
  | If(c, t, e) =>
    If(subst(~j, ~s, c), subst(~j, ~s, t), subst(~j, ~s, e))
  };

let beta = (~body: t, ~arg: t): t =>
  shift(
    ~cutoff=0,
    ~by=-1,
    subst(~j=0, ~s=shift(~cutoff=0, ~by=1, arg), body),
  );
