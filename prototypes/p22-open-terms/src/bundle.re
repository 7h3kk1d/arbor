/* Bundles — recursive groups, closed in the LINK.
 *
 * This is p22's answer to the question 03-content-addressing.md leaves open,
 * and it is a third option beside the two that document compares.
 *
 *   Option A (Unison, p21): close the cycle in the HASHER. The component is the
 *     unit of hashing, references become (hash, position) pairs, and canonical
 *     ordering of the group is a precondition -- one that fails when two
 *     members are structurally identical modulo their intra-cycle references,
 *     at which point the order falls back on the members' NAMES and ingest
 *     aborts rather than store a name-dependent identity.
 *
 *   Options B1/B2 (03-content-addressing.md): close the cycle in the OBJECT
 *     LANGUAGE, with a fixpoint over a tuple (B1) or a labelled record (B2).
 *     The store's type is untouched; the cost is either a rare loss of
 *     presentation-independence (B1) or a mint (B2).
 *
 *   Option C (here): close the cycle in the LINK. The members' open terms are
 *     already independent -- each abstracted its references to the others into
 *     slots, so none of them mentions any other -- and what is cyclic is only
 *     which slots point where. So the fixpoint lands on an array of links, and
 *     `Link.Self(i)` is the bound occurrence.
 *
 * WHAT THIS BUYS, stated so it can be checked rather than believed:
 *
 *   - The object language gains nothing. No Fix, no tuple, no record, no
 *     projection former, no new reduction rule. B1's "cheapest option in
 *     machinery" is still cheaper here, because the machinery is zero.
 *
 *   - Every member's OPEN term keeps its own hash, which is the thing p21
 *     cannot offer. Editing one member of a cycle does not rehash the group's
 *     open terms; it produces one new open term and one new bundle.
 *
 *   - The ordering problem does not disappear. It cannot: ordering a recursive
 *     group is graph canonization (03-content-addressing.md:152) and no
 *     encoding dissolves that. What changes is where it lands and what it
 *     costs.
 *
 * ON THAT LAST POINT, A CORRECTION. This comment first said B1's automorphism
 * argument does not reach the link layer, because a link array has no
 * projections to renumber. That is wrong, and the test that was written to
 * demonstrate the cost failed instead (test_symmetric_group_is_presentation_
 * independent).
 *
 * The argument arrives intact. For a symmetric group the members share an open
 * term, and reordering them renumbers the Self indices in a way that exactly
 * undoes the permutation: [ping, pong] gives members [(o, [Self 1]),
 * (o, [Self 0])], and so does [pong, ping]. The bundle is the same array, so
 * the bundle hash and every member hash are invariant under how the group was
 * written.
 *
 * What varies is which NAME denotes which member. That is weaker than the cost
 * 03-content-addressing.md predicts for B1, where the residual ambiguity
 * produces two structurally distinct but observationally identical projections:
 * here it produces one object with two names for it, and since the members are
 * observationally identical nothing downstream can distinguish them. p21
 * refuses to ingest this case at all.
 *
 * Residual presentation-dependence remains for ASYMMETRIC groups, where members
 * are distinguishable and declaration order does reach the bundle hash. That is
 * what `canonical_order` below would have to fix, and for asymmetric groups
 * sorting on member open hash is total -- the incompleteness of erase-and-sort
 * bites only in the symmetric case, which is exactly the case that no longer
 * needs it. */

type member = {
  open_: Hash.t,
  link: Link.t, /* may contain Self(i) */
};

type t = {members: array(member)};

/* Bundle-layer tag. Ty 0, Abt 1, Open_term 2, Link 3, Def 5. */
let layer_tag = 4;

let hash = (b: t): Hash.t => {
  let toks =
    ref([Hash.Tag(layer_tag), Hash.Nat(Array.length(b.members))]);
  Array.iter(
    m =>
      toks := toks^ @ [Hash.Hashed(m.open_), Hash.Hashed(Link.hash(m.link))],
    b.members,
  );
  Hash.acc(toks^);
};

let size = (b: t): int => Array.length(b.members);

/* The definition of member i: a Def.Member pointing into this bundle. Its hash
   is an ordinary opaque hash, so a Link.Def target referring to a cycle member
   looks like any other target. The index is consumed here and goes no further:
   this is the confinement claim in def.re, in one function. */
let member_def = (b: t, i: int): Def.t => Def.Member(hash(b), i);

let member_hash = (b: t, i: int): Hash.t => Def.hash(member_def(b, i));

/* Resolve Self(i) to the member's definition hash, closing the fixpoint. A
   bundle's links are written with Self; everything downstream sees Def. */
let close = (b: t): array(Link.t) => {
  let h = hash(b);
  Array.map(
    (m: member) =>
      {
        ...m.link,
        targets:
          Array.map(
            fun
            | Link.Self(i) => Link.Def(Def.hash(Def.Member(h, i)))
            | t => t,
            m.link.targets,
          ),
      },
    b.members,
  );
};

/* Whether this group genuinely needs to be a bundle. A single member with no
   Self target is just a Def.Simple, and building a bundle for it would put an
   index where none is needed. Callers use this to keep the confinement tight. */
let is_recursive = (b: t): bool => {
  let self_somewhere =
    Array.exists(
      (m: member) =>
        Array.exists(
          fun
          | Link.Self(_) => true
          | _ => false,
          m.link.targets,
        ),
      b.members,
    );
  self_somewhere || Array.length(b.members) > 1;
};

/* Where a better ordering would go.
 *
 * Today: declaration order. Per the correction above, that is already
 * presentation-independent for symmetric groups, and presentation-DEPENDENT for
 * asymmetric ones -- the opposite of the usual difficulty, and the reason this
 * is still identity rather than a sort.
 *
 * Sorting by member open hash would fix the asymmetric case completely, because
 * distinguishable members have distinct open hashes by construction. Its known
 * incompleteness (ties) lands only on symmetric groups, which do not need it.
 * So the two halves compose, and this function has a defensible implementation
 * available. It is left unimplemented deliberately: the composition is a claim
 * the prototype has not tested, and writing it down as an open question is
 * cheaper than writing it down as code nothing exercises. */
let canonical_order = (b: t): t => b;
