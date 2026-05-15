/* Attachment — the substrate's aspect store. Carries from p4-p6 with
   p9-specific aspect_value variants:
     - Type_of(Hash.t)          : typecheck cache, hole-free term;
                                  hash references a `Definition.Type`
                                  whose Ty.t is the inferred type
     - Type_with_holes(Hash.t)  : typecheck cache, term has holes (best
                                  guess; may be refined as holes fill
                                  in); hash references a Definition.Type
     - Has_holes(bool)          : has-holes:v1 cache; true if the root
                                  hash, transitively, has any Hole node
                                  in the stored DAG

   Type_of / Type_with_holes were inline `Ty.t` values in p6 / p9-pre.
   Hashing them through the Store gives free deduplication of common
   types, makes the type-aliasing identity cleanly observable in the
   aspect store (a name bound to the same type hash points at the same
   aspect value), and unifies "what's stored" with "what's
   referenced." See `docs/design/03-content-addressing.md` §"Hashing
   types as well as terms."

   Keyed by (target hash, aspect id, procedure identity). Derived aspects
   are immutable once written for a given key; asserted aspects can be
   overwritten. */

type aspect_id = string;
type procedure_id = string;

type disposition =
  | Asserted
  | Derived;

[@deriving (eq, show)]
type aspect_value =
  | Eval_value(Hash.t)
  | Eval_stuck(Hash.t)
  | Eval_step_limit(Hash.t)
  | Type_of(Hash.t)
  | Type_with_holes(Hash.t)
  | Has_holes(bool)
  | Follow_clean(bool);

/* Synthetic key for pair-keyed aspects (e.g. follow-clean:v1, keyed by
   `(h_old, h_new)`). Hashes the concatenation so the result is a
   regular `Hash.t` that fits the existing single-target aspect key.
   Two pair-key collisions across distinct aspects would be a false
   share; the aspect-id namespace keeps that from happening. */
let pair_key = (~h_old: Hash.t, ~h_new: Hash.t): Hash.t => {
  let buf = Buffer.create(String.length(h_old) + String.length(h_new));
  Buffer.add_string(buf, h_old);
  Buffer.add_string(buf, h_new);
  Hash.digest_buffer(buf);
};

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

let attach = (att: t, ~target, ~aspect, ~procedure, value) => {
  let descriptor =
    switch (find_descriptor(att, aspect)) {
    | Some(d) => d
    | None => raise(Unknown_aspect(aspect))
    };
  let k = {target, aspect, procedure};
  switch (descriptor.disposition, Hashtbl.find_opt(att.store, k)) {
  | (Derived, Some(_)) => ()
  | _ => Hashtbl.replace(att.store, k, value)
  };
};

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

let peek = (att: t, ~target, ~aspect, ~procedure): option(aspect_value) =>
  Hashtbl.find_opt(att.store, {target, aspect, procedure});

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

let entries_for = (att: t, ~aspect, ~procedure): list((Hash.t, aspect_value)) =>
  Hashtbl.fold(
    (k, v, acc) =>
      if (String.equal(k.aspect, aspect)
          && String.equal(k.procedure, procedure)) {
        [(k.target, v), ...acc];
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
