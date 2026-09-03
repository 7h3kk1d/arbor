/* The object language, in TWO grammars. (def:uterm, def:urterm)
 *
 * A term as WRITTEN mentions other definitions by name, and names are
 * variables: there is no way to write a hash, and no term former denotes a
 * stored definition. A term as STORED mentions them by reference. Name
 * resolution is the map between the two, and it is partial (def:uresolution).
 *
 * In Unison these are one datatype and the distinction is which constructors
 * occur at which stage. Splitting them is an idealization, and it is the
 * idealization that makes the rest legible — so p21 makes it a type error to
 * confuse them, which Unison cannot.
 *
 * Binders carry NAMES, and those names are stored. Hashing erases them; nothing
 * else does. That is load-bearing rather than incidental: update renders stored
 * definitions back to source a human reads and edits, so what is rendered has
 * to use the author's variable names (rem:names). Every arbor prototype from p4
 * on stored de Bruijn indices and invented display names; p21 cannot. */

/* Source terms — what a user writes. (def:uterm) */
module Source = {
  type t =
    | Var(string)
    | Lam(string, t)
    | App(t, t)
    | Nat(int)
    | Text(string)
    | Ann(t, Ty.t);
};

/* Resolved terms — Source extended with references. (def:urterm) */
type t =
  | Var(string)
  | Lam(string, t)
  | App(t, t)
  | Nat(int)
  | Text(string)
  | Ref(Reference.t)
  | Ann(t, Ty.t);

/* Free variables. Ingest needs this to build the dependency graph of a binding
   group, and hashing treats a free variable as an error (def:uhash). */
let free_vars = (tm: t): list(string) => {
  let rec go = (bound, acc, tm) =>
    switch (tm) {
    | Var(x) => List.mem(x, bound) || List.mem(x, acc) ? acc : [x, ...acc]
    | Lam(x, b) => go([x, ...bound], acc, b)
    | App(f, a) => go(bound, go(bound, acc, f), a)
    | Ann(e, _) => go(bound, acc, e)
    | Nat(_)
    | Text(_)
    | Ref(_) => acc
    };
  List.rev(go([], [], tm));
};

/* References occurring in a term. (def:dependents) */
let refs = (tm: t): list(Reference.t) => {
  let rec go = (acc, tm) =>
    switch (tm) {
    | Ref(r) =>
      List.exists(Reference.equal(r), acc) ? acc : [r, ...acc]
    | Lam(_, b) => go(acc, b)
    | App(f, a) => go(go(acc, f), a)
    | Ann(e, _) => go(acc, e)
    | Var(_)
    | Nat(_)
    | Text(_) => acc
    };
  List.rev(go([], tm));
};

/* Substitute a reference for a free variable. Ingest uses this to discharge the
   names of already-hashed components before hashing the next (def:uingest). */
let rec subst_ref = (x: string, r: Reference.t, tm: t): t =>
  switch (tm) {
  | Var(y) => String.equal(x, y) ? Ref(r) : tm
  | Lam(y, b) => String.equal(x, y) ? tm : Lam(y, subst_ref(x, r, b))
  | App(f, a) => App(subst_ref(x, r, f), subst_ref(x, r, a))
  | Ann(e, ty) => Ann(subst_ref(x, r, e), ty)
  | Nat(_)
  | Text(_)
  | Ref(_) => tm
  };

/* Rename a free variable. Used only by tests, to exercise alpha-equivalence.
 *
 * (def:ualpha) Note that unlike Unison, where alpha-equivalence is the equality
 * on terms and alpha-equivalent terms are EQUAL before any hashing takes place,
 * p21's terms are compared structurally: \x. x and \y. y are distinct values
 * that hash alike. Nothing downstream can tell the difference, since only the
 * hash is ever stored -- but it is a divergence, and it is why thm:ualpha is
 * tested through the hash rather than through equality. */
let rec rename = (x: string, y: string, tm: t): t =>
  switch (tm) {
  | Var(z) => String.equal(x, z) ? Var(y) : tm
  | Lam(z, b) => String.equal(x, z) ? tm : Lam(z, rename(x, y, b))
  | App(f, a) => App(rename(x, y, f), rename(x, y, a))
  | Ann(e, ty) => Ann(rename(x, y, e), ty)
  | Nat(_)
  | Text(_)
  | Ref(_) => tm
  };
