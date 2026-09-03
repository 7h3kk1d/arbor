/* The store. (def:ustore)
 *
 *   Sigma : Hash -> Component,   Component = Term^+
 *
 * NOT Hash -> Definition. It is natural to summarize content addressing as "a
 * definition is identified by its hash", and for non-recursive code that is
 * what happens -- but the map is from a hash to a COMPONENT, and a definition
 * is a PAIR (rem:storetype). Mutual recursion is the reason, and the price is
 * that every reference is positional and every property is a property of
 * components rather than of definitions.
 *
 * Content is immutable: register never overwrites. Nothing is ever removed. The
 * only kind of deletion the system supports is a name ceasing to resolve. */

type component = array(Term.t);

type t = {
  defs: Hashtbl.t(Hash.t, component),
  /* A definition's recorded type. Not an aspect ABOUT the hash -- the type is
     inside the hash (def:typeinhash), so this is a projection of stored data
     rather than a claim about it, and it cannot disagree with the term. */
  types: Hashtbl.t((Hash.t, int), Ty.t),
  /* The reverse index. It can exist because a reference remains a node in the
     stored representation even though it hashes transparently at position 0:
     transparency is a property of the hash, not of the representation.
     (def:dependents) */
  dependents: Hashtbl.t(Reference.t, list(Reference.t)),
};

let create = (): t => {
  defs: Hashtbl.create(64),
  types: Hashtbl.create(64),
  dependents: Hashtbl.create(64),
};

let mem = (st: t, h: Hash.t): bool => Hashtbl.mem(st.defs, h);

let lookup_component = (st: t, h: Hash.t): option(component) =>
  Hashtbl.find_opt(st.defs, h);

let lookup = (st: t, r: Reference.t): option(Term.t) =>
  switch (r) {
  | Reference.Builtin(_) => None
  | Reference.Derived(h, i) =>
    switch (Hashtbl.find_opt(st.defs, h)) {
    | Some(c) when i >= 0 && i < Array.length(c) => Some(c[i])
    | _ => None
    }
  };

let type_of = (st: t, r: Reference.t): option(Ty.t) =>
  switch (r) {
  | Reference.Builtin(b) => Builtins.type_of(b)
  | Reference.Derived(h, i) => Hashtbl.find_opt(st.types, (h, i))
  };

/* Register a component at its hash, with each member's type. Idempotent:
   content-addressed, so re-registering the same component is a no-op. */
let register =
    (st: t, h: Hash.t, members: list((Term.t, Ty.t))): unit =>
  if (!Hashtbl.mem(st.defs, h)) {
    let arr = Array.of_list(List.map(fst, members));
    Hashtbl.replace(st.defs, h, arr);
    List.iteri(
      (i, (tm, ty)) => {
        Hashtbl.replace(st.types, (h, i), ty);
        let me = Reference.Derived(h, i);
        List.iter(
          r => {
            let prev =
              switch (Hashtbl.find_opt(st.dependents, r)) {
              | None => []
              | Some(l) => l
              };
            if (!List.exists(Reference.equal(me), prev)) {
              Hashtbl.replace(st.dependents, r, [me, ...prev]);
            };
          },
          Term.refs(tm),
        );
      },
      members,
    );
  };

/* deps(r): the references occurring in r's member. (def:dependents) */
let deps = (st: t, r: Reference.t): list(Reference.t) =>
  switch (lookup(st, r)) {
  | None => []
  | Some(tm) => Term.refs(tm)
  };

/* dependents(r): the converse, maintained as an index. */
let dependents = (st: t, r: Reference.t): list(Reference.t) =>
  switch (Hashtbl.find_opt(st.dependents, r)) {
  | None => []
  | Some(l) => l
  };

/* The transitive closure, excluding self-references -- i.e. excluding other
   members of the target's own component, which are not "dependents" in the
   sense update means. (def:uscope) */
let transitive_dependents = (st: t, seeds: list(Reference.t)): list(Reference.t) => {
  let own_components =
    List.filter_map(
      r =>
        switch (r) {
        | Reference.Derived(h, _) => Some(h)
        | Reference.Builtin(_) => None
        },
      seeds,
    );
  let seen = Hashtbl.create(16);
  let out = ref([]);
  let rec go = r =>
    List.iter(
      d =>
        if (!Hashtbl.mem(seen, Reference.to_string(d))) {
          Hashtbl.replace(seen, Reference.to_string(d), true);
          let in_own =
            switch (d) {
            | Reference.Derived(h, _) => List.exists(Hash.equal(h), own_components)
            | Reference.Builtin(_) => false
            };
          if (!in_own) {
            out := [d, ...out^];
          };
          go(d);
        },
      dependents(st, r),
    );
  List.iter(go, seeds);
  List.rev(out^);
};

let all_hashes = (st: t): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], st.defs, []) |> List.sort(Hash.compare);

let component_size = (st: t, h: Hash.t): int =>
  switch (Hashtbl.find_opt(st.defs, h)) {
  | None => 0
  | Some(c) => Array.length(c)
  };

let build_env = (st: t): Typecheck.env => {
  builtin: Builtins.type_of,
  derived: r => type_of(st, r),
};
