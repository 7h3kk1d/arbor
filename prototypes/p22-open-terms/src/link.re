/* Links — what an open term's slots are bound to.
 *
 * A link is the other half of a definition. The open term says what shape the
 * code has; the link says what it depends on. Binding is the operation that
 * produces one (bind.re), and propagation is the operation that produces a new
 * one from an old one (propagate.re). Neither ever renders a term to text.
 *
 * That is the whole claim. In Unison, and in p21, propagating a change means
 * rendering every dependent back to source through a pretty-print environment
 * and re-parsing it, so whatever name the printer chooses steers the result --
 * which is why an alias can defeat propagation, and why WHICH alias you update
 * decides whether propagation happens at all. Here propagation rewrites an
 * array of targets. There is no printer in the loop and no name to choose, so
 * those failures have no surface to exist on.
 *
 * WHAT IS AND IS NOT HASHED, and why it took a decision.
 *
 * The link's hash covers its arity and its targets. It does NOT cover the names
 * the author wrote. Those names are the "recorded intent" the design doc is
 * about -- the thing Unison discards at bind time and then tries to reconstruct
 * -- so p22 has to keep them, but keeping them IN the hash would put names back
 * into stored programs, which 04-naming-layer.md forbids and which is half of
 * what makes Unison's propagation name-sensitive in the first place.
 *
 * Excluding them has its own cost, and it is exactly the clobber the design
 * doc's finding 3 identifies in Unison's binder names: if names are not in the
 * hash then two links with identical targets are one stored object, and one
 * author's names would overwrite the other's. So the names do not live on the
 * stored link at all. They go in a separate intent table keyed by (link hash,
 * slot) that accumulates every name anyone used (store.re). Nothing clobbers,
 * and nothing named reaches the hash. */

/* What a slot is bound to.
 *
 * `Self(i)` only occurs inside a bundle, where it means member i of the
 * enclosing recursive group. It is the one construct that makes a cycle
 * expressible without a cycle in the hash graph -- see bundle.re.
 *
 * `Unbound` is a slot nobody has linked yet. It is a legitimate stored state,
 * not an error: a definition can be written, abstracted, hashed and stored
 * before anyone decides what its free variables mean. All unbound slots are
 * equal, so the hash is constant -- p8's answer for holes, reused. */
type target =
  | Def(Hash.t)
  | Builtin(string)
  | Self(int)
  | Unbound;

type t = {
  arity: int,
  targets: array(target),
};

/* Link-layer tag. Ty is 0, p21's Abt is 1, Open_term is 2. */
let layer_tag = 3;

let t_def = 0
and t_builtin = 1
and t_self = 2
and t_unbound = 3;

let hash = (l: t): Hash.t => {
  let toks = ref([Hash.Tag(layer_tag), Hash.Nat(l.arity)]);
  Array.iter(
    tgt =>
      toks :=
        toks^
        @ (
          switch (tgt) {
          | Def(h) => [Hash.Tag(t_def), Hash.Hashed(h)]
          | Builtin(b) => [Hash.Tag(t_builtin), Hash.Text(b)]
          | Self(i) => [Hash.Tag(t_self), Hash.Nat(i)]
          | Unbound => [Hash.Tag(t_unbound)]
          }
        ),
    l.targets,
  );
  Hash.acc(toks^);
};

let equal_target = (a: target, b: target): bool =>
  switch (a, b) {
  | (Def(x), Def(y)) => Hash.equal(x, y)
  | (Builtin(x), Builtin(y)) => String.equal(x, y)
  | (Self(i), Self(j)) => i == j
  | (Unbound, Unbound) => true
  | _ => false
  };

let equal = (a: t, b: t): bool =>
  a.arity == b.arity
  && Array.length(a.targets) == Array.length(b.targets)
  && {
    let ok = ref(true);
    Array.iteri((i, t) => ok := ok^ && equal_target(t, b.targets[i]), a.targets);
    ok^;
  };

/* A link with every slot unbound: the state a freshly abstracted term is in
   before anyone binds it. */
let unlinked = (arity: int): t => {
  arity,
  targets: Array.make(arity, Unbound),
};

let is_complete = (l: t): bool =>
  Array.for_all(t => t != Unbound, l.targets);

/* The defs this link depends on. Used to build the reverse index, and by
   propagate.re to find the sites a cascade has to touch. */
let dependencies = (l: t): list(Hash.t) =>
  Array.to_list(l.targets)
  |> List.filter_map(
       fun
       | Def(h) => Some(h)
       | Builtin(_)
       | Self(_)
       | Unbound => None,
     );

/* Replace one slot. The propagation primitive: everything pin/follow/explicit
   does is built out of this. */
let relink = (l: t, slot: int, tgt: target): t => {
  let targets = Array.copy(l.targets);
  targets[slot] = tgt;
  {...l, targets};
};

/* Replace every slot pointing at `from_` with `to_`. This is what following a
   change does to one dependent. Note what it does not do: look at a name,
   render anything, or consult a namespace. */
let retarget = (l: t, ~from_: Hash.t, ~to_: Hash.t): t => {
  let targets =
    Array.map(
      fun
      | Def(h) when Hash.equal(h, from_) => Def(to_)
      | t => t,
      l.targets,
    );
  {...l, targets};
};

let target_to_string = (t: target): string =>
  switch (t) {
  | Def(h) => Hash.short(h)
  | Builtin(b) => b
  | Self(i) => "self." ++ string_of_int(i)
  | Unbound => "?"
  };

let to_string = (l: t): string =>
  "["
  ++ String.concat(", ", Array.to_list(Array.map(target_to_string, l.targets)))
  ++ "]";
