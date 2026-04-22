/* LC DEEP AST — internal, name-free, in de Bruijn form. What
   Store.ingest_lc consumes and Store.reconstruct_lc produces.

   Carried from p4 verbatim (renamed from Ast to Lc_ast).

   Binders carry no names: a Lam is just a body. Variables are integer
   de Bruijn indices, 0 = innermost enclosing binder. Two α-equivalent
   surface terms produce identical Lc_ast.t values (and identical
   hashes after Store.ingest_lc). */

[@deriving (eq, show, ord)]
type t =
  | Var(int)
  | Lam(t)
  | App(t, t);

let rec max_free_index = (~depth: int=0, t: t): option(int) =>
  switch (t) {
  | Var(k) => k >= depth ? Some(k - depth) : None
  | Lam(body) => max_free_index(~depth=depth + 1, body)
  | App(f, a) =>
    switch (max_free_index(~depth, f), max_free_index(~depth, a)) {
    | (None, b) => b
    | (a, None) => a
    | (Some(x), Some(y)) => Some(max(x, y))
    }
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
  | Lam(body) => Lam(shift(~cutoff=cutoff + 1, ~by, body))
  | App(f, a) => App(shift(~cutoff, ~by, f), shift(~cutoff, ~by, a))
  };

let rec subst = (~j: int, ~s: t, t: t): t =>
  switch (t) {
  | Var(k) => k == j ? s : Var(k)
  | Lam(body) =>
    Lam(subst(~j=j + 1, ~s=shift(~cutoff=0, ~by=1, s), body))
  | App(f, a) => App(subst(~j, ~s, f), subst(~j, ~s, a))
  };

let beta = (~body: t, ~arg: t): t =>
  shift(
    ~cutoff=0,
    ~by=-1,
    subst(~j=0, ~s=shift(~cutoff=0, ~by=1, arg), body),
  );
