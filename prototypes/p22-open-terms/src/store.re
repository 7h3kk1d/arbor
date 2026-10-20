/* The store — open terms, links, bundles, definitions, and recorded intent.
 *
 * Append-only and keyed by content, like every arbor store. What is new is that
 * it holds FOUR populations rather than one, because a definition is no longer
 * a single object:
 *
 *   opens    Hash -> Open_term.t   the shape, with free variables as slots
 *   links    Hash -> Link.t        what the slots point at
 *   bundles  Hash -> Bundle.t      recursive groups, cycle closed in the link
 *   defs     Hash -> Def.t         the pairing, and what a reference points at
 *
 * Splitting them is the point. Editing a definition's body produces a new open
 * term and reuses the link; a dependency moving produces a new link and reuses
 * the open term. p21 (and Unison, and every prior arbor prototype) cannot tell
 * those two apart after the fact, because both produce one new opaque hash.
 *
 * THE INTENT TABLE.
 *
 * `intent` maps (link hash, slot) to every name anyone has used for that slot.
 * It is the thing the design doc says Unison throws away at bind time, kept.
 *
 * It is a table rather than a field on the link because link hashes cover
 * targets only -- names must not reach stored bytes (04-naming-layer.md) -- and
 * because if names were a field of a hashed object then two authors who wrote
 * different names for identical targets would be one object, and one set of
 * names would silently win. That is exactly the clobber the design doc
 * identifies in Unison's binder names. Accumulating into a list means nothing
 * clobbers: every name anyone used is recoverable, and which one a given reader
 * sees is a question for the namespace, not for the store.
 *
 * `dependents` is the reverse index: definition hash -> the definitions whose
 * links point at it. It is the one index that is NOT recomputable by traversal
 * (docs/systems/unison/05-deletions.md: Unison deleted its forward-derivable
 * name index and kept exactly this one), and propagate.re is its consumer. */

type t = {
  opens: Hashtbl.t(Hash.t, Open_term.t),
  links: Hashtbl.t(Hash.t, Link.t),
  bundles: Hashtbl.t(Hash.t, Bundle.t),
  defs: Hashtbl.t(Hash.t, Def.t),
  types: Hashtbl.t(Hash.t, Ty.t),
  intent: Hashtbl.t((Hash.t, int), list(string)),
  dependents: Hashtbl.t(Hash.t, list(Hash.t)),
};

let create = (): t => {
  opens: Hashtbl.create(64),
  links: Hashtbl.create(64),
  bundles: Hashtbl.create(16),
  defs: Hashtbl.create(64),
  types: Hashtbl.create(64),
  intent: Hashtbl.create(64),
  dependents: Hashtbl.create(64),
};

/* Registration is idempotent: the key is the content, so writing the same
   object twice is writing the same bytes to the same address. Nothing is ever
   overwritten and nothing is ever removed. */

let put_open = (st: t, o: Open_term.t): Hash.t => {
  let h = Open_term.hash(o);
  if (!Hashtbl.mem(st.opens, h)) {
    Hashtbl.replace(st.opens, h, o);
  };
  h;
};

let put_link = (st: t, l: Link.t): Hash.t => {
  let h = Link.hash(l);
  if (!Hashtbl.mem(st.links, h)) {
    Hashtbl.replace(st.links, h, l);
  };
  h;
};

/* The member open terms are the caller's to register -- they exist before the
   bundle does, which is the whole point of them being independent. */
let put_bundle = (st: t, b: Bundle.t): Hash.t => {
  let h = Bundle.hash(b);
  if (!Hashtbl.mem(st.bundles, h)) {
    Hashtbl.replace(st.bundles, h, b);
    Array.iter((m: Bundle.member) => ignore(put_link(st, m.link)), b.members);
  };
  h;
};

let get_open = (st: t, h: Hash.t): option(Open_term.t) =>
  Hashtbl.find_opt(st.opens, h);
let get_link = (st: t, h: Hash.t): option(Link.t) =>
  Hashtbl.find_opt(st.links, h);
let get_bundle = (st: t, h: Hash.t): option(Bundle.t) =>
  Hashtbl.find_opt(st.bundles, h);
let get_def = (st: t, h: Hash.t): option(Def.t) =>
  Hashtbl.find_opt(st.defs, h);

/* Record that `name` was used for slot `slot` of `link`. Accumulates; never
   replaces. */
let record_intent = (st: t, ~link: Hash.t, ~slot: int, ~name: string): unit => {
  let key = (link, slot);
  let prior = Option.value(~default=[], Hashtbl.find_opt(st.intent, key));
  if (!List.mem(name, prior)) {
    Hashtbl.replace(st.intent, key, prior @ [name]);
  };
};

/* Every name anyone used for this slot, in the order they were first used.
   The head is the earliest, which is what display falls back to. */
let intent_of = (st: t, ~link: Hash.t, ~slot: int): list(string) =>
  Option.value(~default=[], Hashtbl.find_opt(st.intent, (link, slot)));

let slot_name = (st: t, ~link: Hash.t, ~slot: int): string =>
  switch (intent_of(st, ~link, ~slot)) {
  | [n, ..._] => n
  | [] => "?" ++ string_of_int(slot)
  };

/* The link of a definition, whether it is simple or a bundle member. */
let link_of = (st: t, d: Def.t): option(Link.t) =>
  switch (d) {
  | Def.Simple(_, l) => get_link(st, l)
  | Def.Member(b, i) =>
    switch (get_bundle(st, b)) {
    | Some(bu) when i < Bundle.size(bu) => Some(Bundle.close(bu)[i])
    | _ => None
    }
  };

/* The open term of a definition, likewise. */
let open_of = (st: t, d: Def.t): option(Open_term.t) =>
  switch (d) {
  | Def.Simple(o, _) => get_open(st, o)
  | Def.Member(b, i) =>
    switch (get_bundle(st, b)) {
    | Some(bu) when i < Bundle.size(bu) =>
      get_open(st, bu.Bundle.members[i].Bundle.open_)
    | _ => None
    }
  };

let open_hash_of = (st: t, d: Def.t): option(Hash.t) =>
  switch (d) {
  | Def.Simple(o, _) => Some(o)
  | Def.Member(b, i) =>
    switch (get_bundle(st, b)) {
    | Some(bu) when i < Bundle.size(bu) =>
      Some(bu.Bundle.members[i].Bundle.open_)
    | _ => None
    }
  };

let index_dependents = (st: t, ~of_: Hash.t, ~link: Link.t): unit =>
  List.iter(
    dep => {
      let prior = Option.value(~default=[], Hashtbl.find_opt(st.dependents, dep));
      if (!List.exists(Hash.equal(of_), prior)) {
        Hashtbl.replace(st.dependents, dep, prior @ [of_]);
      };
    },
    Link.dependencies(link),
  );

/* Register a definition and index it. Returns its hash. */
let put_def = (st: t, ~ty: option(Ty.t)=?, d: Def.t): Hash.t => {
  let h = Def.hash(d);
  if (!Hashtbl.mem(st.defs, h)) {
    Hashtbl.replace(st.defs, h, d);
    switch (link_of(st, d)) {
    | Some(l) => index_dependents(st, ~of_=h, ~link=l)
    | None => ()
    };
  };
  switch (ty) {
  | Some(t) when !Hashtbl.mem(st.types, h) => Hashtbl.replace(st.types, h, t)
  | _ => ()
  };
  h;
};

let type_of = (st: t, h: Hash.t): option(Ty.t) => Hashtbl.find_opt(st.types, h);

let set_type = (st: t, h: Hash.t, t: Ty.t): unit =>
  if (!Hashtbl.mem(st.types, h)) {
    Hashtbl.replace(st.types, h, t);
  };

/* The definitions whose links point at h. Not recomputable by traversal, which
   is why it is maintained rather than derived. */
let callers_of = (st: t, h: Hash.t): list(Hash.t) =>
  Option.value(~default=[], Hashtbl.find_opt(st.dependents, h));

let count_opens = (st: t) => Hashtbl.length(st.opens);
let count_links = (st: t) => Hashtbl.length(st.links);
let count_bundles = (st: t) => Hashtbl.length(st.bundles);
let count_defs = (st: t) => Hashtbl.length(st.defs);

let all_defs = (st: t): list((Hash.t, Def.t)) =>
  Hashtbl.fold((h, d, acc) => [(h, d), ...acc], st.defs, []);
