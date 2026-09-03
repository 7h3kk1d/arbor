/* Reference — what a stored definition mentions. (def:uref)
 *
 *   r ::= Builtin(b) | Derived(h, i)
 *
 * Derived is a PAIR. That is the shape the whole prototype is organized around:
 * a member of a mutually recursive cycle cannot carry its neighbours' hashes,
 * so the cycle is hashed as a unit and members are addressed by position
 * (rem:storetype). A non-recursive definition is position 0 of a singleton.
 *
 * Builtin names a primitive by TEXT rather than by hash: the system is
 * intrinsically identified everywhere except at its primitives, and the sum
 * type makes that seam visible rather than hiding it in a convention
 * (rem:builtin). */

type pos = int;

type t =
  | Builtin(string)
  | Derived(Hash.t, pos);

/* Referent — what a NAME denotes. (def:ureferent)
 *
 * The Con arm is vestigial here: with only Nat, Text and arrows there are no
 * data declarations. It is retained so the namespace relation has the right
 * shape, and so that adding declarations later is an extension rather than a
 * change. */
type referent =
  | Ref(t)
  | Con(t, int);

let equal = (a: t, b: t): bool =>
  switch (a, b) {
  | (Builtin(x), Builtin(y)) => String.equal(x, y)
  | (Derived(h1, i1), Derived(h2, i2)) => Hash.equal(h1, h2) && i1 == i2
  | _ => false
  };

let compare = (a: t, b: t): int =>
  switch (a, b) {
  | (Builtin(x), Builtin(y)) => String.compare(x, y)
  | (Builtin(_), Derived(_, _)) => (-1)
  | (Derived(_, _), Builtin(_)) => 1
  | (Derived(h1, i1), Derived(h2, i2)) =>
    let c = Hash.compare(h1, h2);
    c != 0 ? c : compare(i1, i2);
  };

let referent_equal = (a: referent, b: referent): bool =>
  switch (a, b) {
  | (Ref(x), Ref(y)) => equal(x, y)
  | (Con(x, i), Con(y, j)) => equal(x, y) && i == j
  | _ => false
  };

let referent_compare = (a: referent, b: referent): int =>
  switch (a, b) {
  | (Ref(x), Ref(y)) => compare(x, y)
  | (Ref(_), Con(_, _)) => (-1)
  | (Con(_, _), Ref(_)) => 1
  | (Con(x, i), Con(y, j)) =>
    let c = compare(x, y);
    c != 0 ? c : Stdlib.compare(i, j);
  };

let to_string = (r: t): string =>
  switch (r) {
  | Builtin(b) => "##" ++ b
  | Derived(h, 0) => Hash.short(h)
  | Derived(h, i) => Hash.short(h) ++ "." ++ string_of_int(i)
  };

let referent_to_string = (r: referent): string =>
  switch (r) {
  | Ref(x) => to_string(x)
  | Con(x, i) => to_string(x) ++ "#" ++ string_of_int(i)
  };

let referent_ref = (r: referent): t =>
  switch (r) {
  | Ref(x) => x
  | Con(x, _) => x
  };
