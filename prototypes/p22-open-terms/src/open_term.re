/* Open terms — the stored form, with free variables abstracted into a telescope.
 *
 * This is the p22 delta, and the whole prototype turns on it. In p21 (and in
 * Unison, and in every arbor prototype from p3 on) a free variable is resolved
 * to a hash at edit time and the term as stored contains that hash. The name is
 * discarded. Everything downstream that needs to know what the author meant --
 * propagation, merge, "did the author edit this or did a dependency move" --
 * has to reconstruct it from the namespace, and that reconstruction is where
 * Unison's naming-sensitive failures live.
 *
 * Here a free variable becomes Slot(i): an anonymous index into a telescope.
 * The open term records the SHAPE of a definition and nothing about what it
 * depends on. What it depends on is a separate stored object (Link), and the
 * pair of them is a definition.
 *
 * Two consequences worth stating up front, because they are what the prototype
 * is for:
 *
 *   - Two definitions with the same shape but different dependencies share one
 *     open hash. `double n = scale n` and `double n = shrink n` are the same
 *     open term. This is maximal sharing, and it is what makes the open hash a
 *     usable answer to "did the author edit this?".
 *
 *   - A recursive group has no cycle at this layer. Each member abstracts its
 *     references to the others into slots, so each member hashes independently.
 *     The cycle lives in the links (see bundle.re). Nothing here needs an SCC,
 *     a component, or a canonical ordering over a group.
 *
 * Binders are de Bruijn and carry no names, so alpha-equivalence holds by
 * canonicalization: \x. x and \y. y are one stored object. p21 could not do
 * this -- it stores binder names because `update` renders definitions back to
 * source a human edits (rem:names) -- and p22 can precisely because it never
 * renders to propagate. Display names are invented on the way out. */

/* The stored structure. No names anywhere: not binder names, not the names the
   author wrote for the free variables. (04-naming-layer.md: "Names never appear
   in stored programs.") */
type t =
  | Bound(int) /* de Bruijn index into enclosing Lams */
  | Slot(int) /* index into the telescope of free variables */
  | Lam(t)
  | App(t, t)
  | Nat(int)
  | Text(string)
  | Ann(t, Ty.t);

/* The result of abstracting a source term.
 *
 * `slot_names` is what the author wrote for each slot, in telescope order. It
 * is NOT part of the open term and is NOT hashed -- it is the input to binding
 * (link.re resolves each name to a target) and is retained there as recorded
 * intent. Keeping it out of the hash is what keeps names out of stored
 * programs; keeping it at all is the point of the prototype. */
type abstraction = {
  body: t,
  arity: int,
  slot_names: array(string),
};

/* Open-term layer tag. Ty uses 0 and p21's Abt used 1; a fresh layer here means
   an open term can never collide with either. (def:tokens) */
let layer_tag = 2;

let t_bound = 0
and t_slot = 1
and t_lam = 2
and t_app = 3
and t_nat = 4
and t_text = 5
and t_ann = 6;

/* hash(t) -- structural, total, and independent of any link.
 *
 * Total, unlike p21's Abt.hash, which raises Free_variable: there is no free
 * variable left to fail on. Every name the author wrote has already become a
 * slot index. That totality is the property the rest of the prototype leans on,
 * and it is why ingest here needs no binding group, no SCC and no gate. */
let rec hash = (tm: t): Hash.t =>
  Hash.acc(
    switch (tm) {
    | Bound(i) => [Hash.Tag(layer_tag), Hash.Tag(t_bound), Hash.Nat(i)]
    | Slot(i) => [Hash.Tag(layer_tag), Hash.Tag(t_slot), Hash.Nat(i)]
    | Lam(b) => [Hash.Tag(layer_tag), Hash.Tag(t_lam), Hash.Hashed(hash(b))]
    | App(f, a) => [
        Hash.Tag(layer_tag),
        Hash.Tag(t_app),
        Hash.Hashed(hash(f)),
        Hash.Hashed(hash(a)),
      ]
    | Nat(n) => [Hash.Tag(layer_tag), Hash.Tag(t_nat), Hash.Nat(n)]
    | Text(s) => [Hash.Tag(layer_tag), Hash.Tag(t_text), Hash.Text(s)]
    | Ann(e, ty) => [
        Hash.Tag(layer_tag),
        Hash.Tag(t_ann),
        Hash.Hashed(hash(e)),
        Hash.Hashed(Ty.hash(ty)),
      ]
    },
  );

/* The telescope's arity, read off the body. Kept as a field on `abstraction`
   too, but recomputable, so nothing has to trust the field. */
let arity_of = (tm: t): int => {
  let rec go = (acc, tm) =>
    switch (tm) {
    | Slot(i) => max(acc, i + 1)
    | Lam(b) => go(acc, b)
    | App(f, a) => go(go(acc, f), a)
    | Ann(e, _) => go(acc, e)
    | Bound(_)
    | Nat(_)
    | Text(_) => acc
    };
  go(0, tm);
};

/* abstract(src) -- the map from what was written to what is stored.
 *
 * Slot order is FIRST OCCURRENCE in a left-to-right traversal. That choice has
 * to be made somewhere and it has to be a function of the term's structure
 * alone, never of the names (03-content-addressing.md:146). First-occurrence is
 * deterministic, and it is presentation-independent in the only sense available
 * to a single term: reordering is not something a single term admits. (Ordering
 * a GROUP is the hard version of this problem and it does not arise here --
 * see the header note about recursive groups.)
 *
 * Note what is NOT consulted: the namespace. Abstraction is a pure function of
 * the source term. A term can be abstracted, hashed and stored before anyone
 * has decided what its free variables mean, which is what lets p22 ingest
 * without a commit gate and without resolution having to succeed. */
let abstract = (src: Term.Source.t): abstraction => {
  let order: ref(list(string)) = ref([]); /* reverse first-occurrence order */
  let count = ref(0);
  let tbl: Hashtbl.t(string, int) = Hashtbl.create(16);
  let slot_of = (x: string): int =>
    switch (Hashtbl.find_opt(tbl, x)) {
    | Some(i) => i
    | None =>
      let i = count^;
      Hashtbl.add(tbl, x, i);
      order := [x, ...order^];
      incr(count);
      i;
    };
  /* Position of x in `bound`, which holds enclosing binder names innermost
     first -- so the position IS the de Bruijn index. */
  let rec depth_of = (j: int, bound: list(string), x: string): option(int) =>
    switch (bound) {
    | [] => None
    | [y, ...rest] => String.equal(x, y) ? Some(j) : depth_of(j + 1, rest, x)
    };
  /* App evaluates its two subterms with explicit lets: slot numbering is
     first-occurrence in a left-to-right traversal, so the order the two
     recursive calls run in is part of the specification, not an accident of
     OCaml's unspecified argument evaluation order. */
  let rec go = (bound: list(string), src: Term.Source.t): t =>
    switch (src) {
    | Term.Source.Var(x) =>
      switch (depth_of(0, bound, x)) {
      | Some(j) => Bound(j)
      | None => Slot(slot_of(x))
      }
    | Term.Source.Lam(x, b) => Lam(go([x, ...bound], b))
    | Term.Source.App(f, a) =>
      let f' = go(bound, f);
      let a' = go(bound, a);
      App(f', a');
    | Term.Source.Nat(n) => Nat(n)
    | Term.Source.Text(s) => Text(s)
    | Term.Source.Ann(e, ty) => Ann(go(bound, e), ty)
    };
  let body = go([], src);
  {body, arity: count^, slot_names: Array.of_list(List.rev(order^))};
};

/* Rebuild a source term for display. Binder names are invented, because none
   were stored; slot names come from whatever the caller knows (the link's
   recorded intent, or a placeholder). p4 through p17 all did this. */
let to_source = (~slot_names: array(string)=[||], tm: t): Term.Source.t => {
  let name_at = (i: int) =>
    if (i < Array.length(slot_names) && String.length(slot_names[i]) > 0) {
      slot_names[i];
    } else {
      "?" ++ string_of_int(i);
    };
  let binder = (depth: int) => {
    let letters = "xyzabcdefghijklmnopqrstuvw";
    let n = String.length(letters);
    let c = String.make(1, letters.[depth mod n]);
    depth < n ? c : c ++ string_of_int(depth / n);
  };
  let rec go = (depth: int, tm: t): Term.Source.t =>
    switch (tm) {
    | Bound(i) => Term.Source.Var(binder(depth - 1 - i))
    | Slot(i) => Term.Source.Var(name_at(i))
    | Lam(b) => Term.Source.Lam(binder(depth), go(depth + 1, b))
    | App(f, a) => Term.Source.App(go(depth, f), go(depth, a))
    | Nat(n) => Term.Source.Nat(n)
    | Text(s) => Term.Source.Text(s)
    | Ann(e, ty) => Term.Source.Ann(go(depth, e), ty)
    };
  go(0, tm);
};

let rec equal = (a: t, b: t): bool =>
  switch (a, b) {
  | (Bound(i), Bound(j)) => i == j
  | (Slot(i), Slot(j)) => i == j
  | (Lam(x), Lam(y)) => equal(x, y)
  | (App(f1, a1), App(f2, a2)) => equal(f1, f2) && equal(a1, a2)
  | (Nat(m), Nat(n)) => m == n
  | (Text(s), Text(u)) => String.equal(s, u)
  | (Ann(e1, t1), Ann(e2, t2)) => equal(e1, e2) && Ty.equal(t1, t2)
  | _ => false
  };
