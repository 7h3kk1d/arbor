/* Attachment — the substrate's aspect store. Holds derived and
   asserted associated data keyed by (target hash, aspect id, procedure
   identity). Supports bidirectional queries (hash → aspects;
   aspect-value → hashes). Asserted entries are mutable; derived
   entries are immutable for a given procedure identity (re-attach with
   a different value is a no-op).

   This prototype registers one descriptor: the eval cache. The API
   shape matches docs/design/06-architecture.md:34-42. */

type aspect_id = string; /* e.g. "arith:eval" */
type procedure_id = string; /* e.g. "arith:eval:v1" */

type disposition =
  | Asserted
  | Derived;

[@deriving (eq, show)]
type aspect_value =
  | Eval_value(Hash.t)
  | Eval_stuck(Hash.t);

type descriptor = {
  id: aspect_id,
  disposition,
  languages: list(string),
};

type key = {
  target: Hash.t,
  aspect: aspect_id,
  procedure: procedure_id,
};

type stats = {
  entries: int,
  hits: int,
  misses: int,
};

type t = {
  descriptors: Hashtbl.t(aspect_id, descriptor),
  store: Hashtbl.t(key, aspect_value),
  mutable hits: int,
  mutable misses: int,
};

let create = (): t => {
  descriptors: Hashtbl.create(4),
  store: Hashtbl.create(64),
  hits: 0,
  misses: 0,
};

let register_descriptor = (att: t, d: descriptor): unit =>
  Hashtbl.replace(att.descriptors, d.id, d);

let find_descriptor = (att: t, id: aspect_id): option(descriptor) =>
  Hashtbl.find_opt(att.descriptors, id);

exception Unknown_aspect(aspect_id);

/* Attach a value. For derived aspects, the first write for a given key
   wins; subsequent writes with any value are no-ops. For asserted
   aspects, later writes overwrite. */
let attach = (att: t, ~target, ~aspect, ~procedure, value) => {
  let descriptor =
    switch (find_descriptor(att, aspect)) {
    | Some(d) => d
    | None => raise(Unknown_aspect(aspect))
    };
  let k = {target, aspect, procedure};
  switch (descriptor.disposition, Hashtbl.find_opt(att.store, k)) {
  | (Derived, Some(_)) => () /* immutable once written */
  | _ => Hashtbl.replace(att.store, k, value)
  };
};

/* Direct lookup. Updates hit/miss counters. */
let get = (att: t, ~target, ~aspect, ~procedure): option(aspect_value) => {
  let k = {target, aspect, procedure};
  switch (Hashtbl.find_opt(att.store, k)) {
  | Some(_) as some =>
    att.hits = att.hits + 1;
    some;
  | None =>
    att.misses = att.misses + 1;
    None;
  };
};

/* Passive lookup — no hit/miss accounting. Useful for tests and for
   the REPL's (cached) marker, which needs to check before `get`. */
let peek = (att: t, ~target, ~aspect, ~procedure): option(aspect_value) =>
  Hashtbl.find_opt(att.store, {target, aspect, procedure});

/* Bidirectional query: all targets whose (aspect, procedure) value
   equals the given aspect_value. Linear scan — fine at prototype
   scale; see open-questions.md for when to add a reverse index. */
let by_value = (att: t, ~aspect, ~procedure, value: aspect_value): list(Hash.t) =>
  Hashtbl.fold(
    (k, v, acc) =>
      if (String.equal(k.aspect, aspect)
          && String.equal(k.procedure, procedure)
          && equal_aspect_value(v, value)) {
        [k.target, ...acc];
      } else {
        acc;
      },
    att.store,
    [],
  );

let stats = (att: t): stats => {
  entries: Hashtbl.length(att.store),
  hits: att.hits,
  misses: att.misses,
};

let reset_counters = (att: t): unit => {
  att.hits = 0;
  att.misses = 0;
};
