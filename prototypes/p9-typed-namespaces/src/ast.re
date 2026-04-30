/* DEEP AST — internal, name-free, in de Bruijn form. What Store.ingest
   consumes and Store.reconstruct produces.

   Two binders: Lam (carries Ty.t annotation; body has fresh binder at
   index 0) and Let (rhs in outer scope; body has fresh binder at
   index 0). Both eliminate names via the same de Bruijn trick — `let x
   = 1 in x` and `let y = 1 in y` produce identical Ast.t values, hence
   identical hashes. Lam and Let are NOT mutually equivalent: Let has a
   distinct shallow node and tag byte. */

[@deriving (eq, show, ord)]
type t =
  | Var(int)
  | Int_lit(int)
  | Bool_lit(bool)
  | String_lit(string)
  | Lam(Ty.t, t)
  | App(t, t)
  | Let(t, t)
  | If(t, t, t)
  | Pair(t, t)
  | Fst(t)
  | Snd(t)
  | Prim(Surface_ast.prim_op, list(t))
  | Prim_call(string /* primitive id */, list(t))
  | Hole;

/* Highest free de Bruijn index in t, given binder depth d at the lookup
   point. None if t is closed under d binders. */
let rec max_free_index = (~depth: int=0, t: t): option(int) => {
  let mix = (x, y) =>
    switch (x, y) {
    | (None, b) => b
    | (a, None) => a
    | (Some(x), Some(y)) => Some(max(x, y))
    };
  switch (t) {
  | Var(k) => k >= depth ? Some(k - depth) : None
  | Int_lit(_)
  | Bool_lit(_)
  | String_lit(_)
  | Hole => None
  | Lam(_, body) => max_free_index(~depth=depth + 1, body)
  | Let(rhs, body) =>
    mix(
      max_free_index(~depth, rhs),
      max_free_index(~depth=depth + 1, body),
    )
  | App(f, a) =>
    mix(max_free_index(~depth, f), max_free_index(~depth, a))
  | If(c, t', e) =>
    mix(
      max_free_index(~depth, c),
      mix(max_free_index(~depth, t'), max_free_index(~depth, e)),
    )
  | Pair(a, b) =>
    mix(max_free_index(~depth, a), max_free_index(~depth, b))
  | Fst(a)
  | Snd(a) => max_free_index(~depth, a)
  | Prim(_, args)
  | Prim_call(_, args) =>
    List.fold_left(
      (acc, t') => mix(acc, max_free_index(~depth, t')),
      None,
      args,
    )
  };
};

let is_closed = (t: t): bool => max_free_index(t) == None;

/* TAPL Ch. 6 shift: shift free variables of t (those with index ≥ cutoff
   c) by `by`. Crosses Lam and Let bodies (each adds 1 to the cutoff). */
let rec shift = (~cutoff: int, ~by: int, t: t): t =>
  switch (t) {
  | Var(k) =>
    if (k < cutoff) {
      Var(k);
    } else {
      Var(k + by);
    }
  | Int_lit(_)
  | Bool_lit(_)
  | String_lit(_)
  | Hole => t
  | Lam(ty, body) => Lam(ty, shift(~cutoff=cutoff + 1, ~by, body))
  | Let(rhs, body) =>
    Let(
      shift(~cutoff, ~by, rhs),
      shift(~cutoff=cutoff + 1, ~by, body),
    )
  | App(f, a) => App(shift(~cutoff, ~by, f), shift(~cutoff, ~by, a))
  | If(c, t', e) =>
    If(
      shift(~cutoff, ~by, c),
      shift(~cutoff, ~by, t'),
      shift(~cutoff, ~by, e),
    )
  | Pair(a, b) => Pair(shift(~cutoff, ~by, a), shift(~cutoff, ~by, b))
  | Fst(a) => Fst(shift(~cutoff, ~by, a))
  | Snd(a) => Snd(shift(~cutoff, ~by, a))
  | Prim(op, args) =>
    Prim(op, List.map(t' => shift(~cutoff, ~by, t'), args))
  | Prim_call(id, args) =>
    Prim_call(id, List.map(t' => shift(~cutoff, ~by, t'), args))
  };

/* Substitution: [j ↦ s]t. */
let rec subst = (~j: int, ~s: t, t: t): t =>
  switch (t) {
  | Var(k) => k == j ? s : Var(k)
  | Int_lit(_)
  | Bool_lit(_)
  | String_lit(_)
  | Hole => t
  | Lam(ty, body) =>
    Lam(ty, subst(~j=j + 1, ~s=shift(~cutoff=0, ~by=1, s), body))
  | Let(rhs, body) =>
    Let(
      subst(~j, ~s, rhs),
      subst(~j=j + 1, ~s=shift(~cutoff=0, ~by=1, s), body),
    )
  | App(f, a) => App(subst(~j, ~s, f), subst(~j, ~s, a))
  | If(c, t', e) =>
    If(subst(~j, ~s, c), subst(~j, ~s, t'), subst(~j, ~s, e))
  | Pair(a, b) => Pair(subst(~j, ~s, a), subst(~j, ~s, b))
  | Fst(a) => Fst(subst(~j, ~s, a))
  | Snd(a) => Snd(subst(~j, ~s, a))
  | Prim(op, args) =>
    Prim(op, List.map(t' => subst(~j, ~s, t'), args))
  | Prim_call(id, args) =>
    Prim_call(id, List.map(t' => subst(~j, ~s, t'), args))
  };

/* β-reduce a Lam body applied to arg. */
let beta = (~body: t, ~arg: t): t =>
  shift(
    ~cutoff=0,
    ~by=-1,
    subst(~j=0, ~s=shift(~cutoff=0, ~by=1, arg), body),
  );
