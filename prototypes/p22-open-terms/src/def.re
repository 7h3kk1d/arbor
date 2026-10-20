/* Definitions — an open term paired with a link.
 *
 * This is the closed object: the thing that has a type, evaluates, is named in
 * the namespace, and is what a reference points at. Neither half is a
 * definition on its own. An open term without a link is a shape nobody has
 * committed to; a link without an open term is a list of dependencies with
 * nothing depending on them.
 *
 * TWO CONSTRUCTORS, AND WHY THE SECOND ONE EXISTS.
 *
 * `Simple` is the ordinary case and it is a pair of hashes. `Member` is a
 * member of a recursive group, and it is the ONLY place in p22 where an index
 * appears beside a hash.
 *
 * That is the delta from Unison worth measuring. 03-content-addressing.md's
 * first cost for Option A is that "the store's type changes ... and EVERY
 * reference in the system becomes a pair." Here no ordinary reference becomes a
 * pair: Link.Def carries a bare Hash.t, and a cycle member is reached through a
 * hash of its own (hash(Member(b, i)) below) that is as opaque as any other.
 * The index is confined to constructing that hash, which happens once, inside a
 * recursive group, at the closed layer. The open terms of a cycle -- the things
 * you edit, diff, and track history on -- never see it.
 *
 * Whether that confinement holds up is one of the things the prototype is for.
 * See bundle.re for where the ordering question reappears. */

type t =
  | Simple(Hash.t, Hash.t) /* open term, link */
  | Member(Hash.t, int); /* bundle, index within it */

/* Definition-layer tag. Ty 0, Abt 1, Open_term 2, Link 3, Bundle 4. */
let layer_tag = 5;

let t_simple = 0
and t_member = 1;

let hash = (d: t): Hash.t =>
  Hash.acc(
    switch (d) {
    | Simple(o, l) => [
        Hash.Tag(layer_tag),
        Hash.Tag(t_simple),
        Hash.Hashed(o),
        Hash.Hashed(l),
      ]
    | Member(b, i) => [
        Hash.Tag(layer_tag),
        Hash.Tag(t_member),
        Hash.Hashed(b),
        Hash.Nat(i),
      ]
    },
  );

let equal = (a: t, b: t): bool =>
  switch (a, b) {
  | (Simple(o1, l1), Simple(o2, l2)) =>
    Hash.equal(o1, o2) && Hash.equal(l1, l2)
  | (Member(b1, i1), Member(b2, i2)) => Hash.equal(b1, b2) && i1 == i2
  | _ => false
  };

let open_hash = (d: t): option(Hash.t) =>
  switch (d) {
  | Simple(o, _) => Some(o)
  | Member(_, _) => None /* the bundle holds it; Store.open_of resolves */
  };

let to_string = (d: t): string =>
  switch (d) {
  | Simple(o, l) => Hash.short(o) ++ " @ " ++ Hash.short(l)
  | Member(b, i) => Hash.short(b) ++ "." ++ string_of_int(i)
  };
