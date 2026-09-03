/* Hashing terms and components. (def:uhash, def:comphash)
 *
 * This is the deepest mechanism in the model and the one whose properties are
 * least apparent from reading it. Two things are worth flagging before the
 * code:
 *
 *   - Hashing carries an environment of binders, innermost first, in which an
 *     ordinary binder is R(x) and a CYCLE FRAME L(V) names a whole set at once.
 *     A variable bound by a cycle frame hashes to the frame's index -- THE SAME
 *     INDEX FOR EVERY MEMBER OF V. That is what makes step 1 below possible:
 *     member bodies become comparable before any of them has an identity.
 *
 *   - Component hashing computes TWO distinct orderings of the same component.
 *     One fixes the de Bruijn indices used while hashing member bodies; the
 *     other fixes the stored positions that appear in references. They coincide
 *     only by accident (rem:twoorders). */

/* A hashing environment entry. (def:uhash) */
type frame =
  | R(string) /* an ordinary binder */
  | L(list(string)); /* a cycle frame: a whole set at one index */

type env = list(frame);

exception Free_variable(string);

/* Index of x in the environment: the least j with x in env_j. A free variable
   is an error -- hashing a term with one is meaningless, since the index it
   would receive depends on nothing. (def:uhash) */
let index_of = (env: env, x: string): int => {
  let rec go = (j, env) =>
    switch (env) {
    | [] => raise(Free_variable(x))
    | [R(y), ...rest] => String.equal(x, y) ? j : go(j + 1, rest)
    | [L(vs), ...rest] => List.mem(x, vs) ? j : go(j + 1, rest)
    };
  go(0, env);
};

/* Term-layer tag. Domain-separates terms from types. (def:tokens) */
let layer_tag = 1;

/* Constructor tags within the term layer. */
let t_var = 0
and t_lam = 1
and t_app = 2
and t_nat = 3
and t_text = 4
and t_ref_builtin = 5
and t_ann = 6;

/* hash(env, t). (def:uhash)
 *
 * Binder names are discarded on the way in, so alpha-equivalent terms hash
 * equally (thm:ualpha).
 *
 * The Ref case is where reference transparency lives, and where it stops
 * (prop:transparent). At position 0 a reference contributes LITERALLY the
 * referent's hash -- no tag, no wrapper -- so it is interchangeable, inside any
 * enclosing hash, with any term hashing to h. At any other position it is
 * tagged with its position and therefore opaque. The boundary falls at Pos = 0,
 * which means transparency and the component model are not independent
 * features. */
let rec hash = (env: env, tm: Term.t): Hash.t =>
  switch (tm) {
  | Term.Var(x) => Hash.acc([Hash.Tag(layer_tag), Hash.Tag(t_var), Hash.Nat(index_of(env, x))])
  | Term.Lam(x, b) =>
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(t_lam),
      Hash.Hashed(hash([R(x), ...env], b)),
    ])
  | Term.App(f, a) =>
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(t_app),
      Hash.Hashed(hash(env, f)),
      Hash.Hashed(hash(env, a)),
    ])
  | Term.Nat(n) => Hash.acc([Hash.Tag(layer_tag), Hash.Tag(t_nat), Hash.Nat(n)])
  | Term.Text(s) => Hash.acc([Hash.Tag(layer_tag), Hash.Tag(t_text), Hash.Text(s)])
  | Term.Ref(Reference.Builtin(b)) =>
    Hash.acc([Hash.Tag(layer_tag), Hash.Tag(t_ref_builtin), Hash.Text(b)])
  /* (prop:transparent) */
  | Term.Ref(Reference.Derived(h, 0)) => h
  | Term.Ref(Reference.Derived(h, i)) =>
    Hash.acc([Hash.Tag(1), Hash.Hashed(h), Hash.Nat(i)])
  | Term.Ann(e, ty) =>
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(t_ann),
      Hash.Hashed(hash(env, e)),
      Hash.Hashed(Ty.hash(ty)),
    ])
  };

/* The hash of a closed term, in the empty environment. */
let hash_closed = (tm: Term.t): Hash.t => hash([], tm);

/* A warning, not an error: hashing succeeds even when it produces one. What
   fails is not definedness but presentation-independence, and the decision to
   refuse belongs to the caller (rem:notpartial). */
type warning =
  | Incomplete_element_ordering(list(list(string)));

let warning_to_string = (w: warning): string =>
  switch (w) {
  | Incomplete_element_ordering(groups) =>
    let g = List.map(vs => "{" ++ String.concat(", ", vs) ++ "}", groups);
    "these definitions are structurally equivalent modulo their references to "
    ++ "each other, so the order of the cycle -- and therefore its hash -- would "
    ++ "depend on their names: "
    ++ String.concat(" ", g);
  };

/* (thm:ordering) Sorting that is STABLE, which step 2 depends on: when two members tie, the
   sort returns them in listed order, and the listed order is name order. That
   is the whole content of thm:ordering(b). OCaml's List.stable_sort is stable
   by contract; using List.sort here would silently change the model. */
let stable_sort_by = (key, xs) =>
  List.stable_sort((a, b) => String.compare(key(a), key(b)), xs);

/* hashComponent(C). (def:comphash)
 *
 * C is listed in NAME ORDER -- the caller always supplies it that way, because
 * Unison always obtains it as Map.toList over a name-keyed map.
 *
 * Returns the component hash, the members in STORAGE order (which fixes the
 * Pos in Derived(h, i)), and any warnings. */
let hash_component =
    (members: list((string, Term.t)))
    : (Hash.t, list((string, Term.t)), list(warning)) => {
  let names = List.map(fst, members);

  /* Step 1 -- permutation hashes. In the environment L(V) every intra-cycle
     reference resolves to the same index, so member bodies become comparable
     without any of them yet having an identity. Note that ph(y) is a function
     of y's BODY alone: the environment mentions no member individually, which
     is why thm:ordering(a) holds. */
  let env_p: env = [L(names)];
  let ph = List.map(((y, u)) => (y, u, hash(env_p, u)), members);

  /* The side condition. Injectivity of ph on V is decided by grouping the
     members by permutation hash. (cor:detect) */
  let warnings = {
    let tbl = Hashtbl.create(8);
    List.iter(
      ((y, _, h)) => {
        let prev = try(Hashtbl.find(tbl, h)) {
        | Not_found => []
        };
        Hashtbl.replace(tbl, h, [y, ...prev]);
      },
      ph,
    );
    let groups =
      Hashtbl.fold(
        (_, vs, acc) =>
          List.length(vs) > 1 ? [List.sort(String.compare, vs), ...acc] : acc,
        tbl,
        [],
      );
    groups == [] ? [] : [Incomplete_element_ordering(List.sort(compare, groups))];
  };

  /* Step 2 -- the binding order. Sort by ph, STABLY, giving the permutation pi.
     Each member now has a distinct de Bruijn index. */
  let permuted = stable_sort_by(((_, _, h)) => h, ph);
  let env: env = List.map(((y, _, _)) => R(y), permuted);
  let member_hashes = List.map(((_, u, _)) => hash(env, u), permuted);

  /* Step 3 -- the storage order. A second hash per member, over tokens shared
     by every member plus the member's own index in env, and a second sort by
     THAT. sigma need not agree with pi (rem:twoorders): it sorts by a hash of
     the pi-index, which is a different thing from the pi-index.

     Note that this step is applied to the ORIGINAL (name-ordered) listing, not
     to the pi-permuted one. */
  let common_tokens =
    [Hash.Tag(1), ...List.map(h => Hash.Hashed(h), member_hashes)];
  let nh = (y: string): Hash.t =>
    Hash.acc(common_tokens @ [Hash.Hashed(hash(env, Term.Var(y)))]);
  let keyed = List.map(((y, u)) => (nh(y), (y, u)), members);
  let sorted = stable_sort_by(fst, keyed);
  let storage_order = List.map(snd, sorted);
  let overall =
    Hash.acc(List.map(((h, _)) => Hash.Hashed(h), sorted));

  (overall, storage_order, warnings);
};
