/* Propagation — pin, follow, explicit, as operations on links.
 *
 * 04-naming-layer.md frames pin/follow/explicit as points in a two-axis space
 * (scope x user-in-loop) and notes "No prototype has exercised the cascade
 * primitives yet." This is that. What the framing does not say, and what falls
 * out here, is that the three are not three policies: they are one primitive --
 * rewrite a link -- applied to three different sets of sites.
 *
 *   pin       rewrite nothing. The definition you have keeps the link it has.
 *   follow    rewrite every transitive caller.
 *   explicit  rewrite the sites you chose.
 *
 * WHAT IS NOT HERE, and is the result.
 *
 * There is no printer, no parser, no pretty-print environment, and no name
 * lookup anywhere in this file. Unison's `update` and `upgrade` both work by
 * rendering every dependent back to source through a PPE and re-parsing it
 * against a new namespace, which is why:
 *
 *   - an alias can defeat propagation (the PPE renders the old hash under a
 *     name that still points at the old definition, so the reparse is a no-op),
 *   - WHICH alias you update decides whether propagation happens at all (the
 *     PPE picks one name per hash by shortest-then-alphabetical),
 *   - and a dependent's source text can be character-for-character unchanged
 *     while its hash changes.
 *
 * None of those have a surface here. `retarget` replaces hashes in an array.
 * There is nothing for a name to steer.
 *
 * The third bullet is worth keeping in mind as the positive claim rather than
 * the absence of a bug: a followed dependent keeps its OPEN hash exactly, and
 * only its link moves. That is what makes "did the author edit this, or did
 * something underneath it move?" answerable by comparing two hashes. */

type strategy =
  | Pin
  | Follow
  | Explicit(list(Hash.t)); /* the definition hashes to rewrite */

/* One rewritten site. */
type rewrite = {
  old_hash: Hash.t,
  new_hash: Hash.t,
  open_hash: Hash.t, /* unchanged by construction -- see `assert` in the tests */
  old_link: Hash.t,
  new_link: Hash.t,
  names: list(string), /* names that pointed at old_hash, now moved */
};

/* Rewrite one definition's link so that every target at `from_` becomes `to_`.
   Returns None if nothing pointed there.
 *
 * `via` is the capability p21 and Unison do not have. When a definition is
 * reachable under several names -- an alias -- a link target alone cannot say
 * which name the author had in mind, because the link holds a hash. Unison is
 * in exactly this position and resolves it by rendering through a pretty-print
 * environment, which picks one name per hash by shortest-then-alphabetical; the
 * consequence is that updating `foo` propagates and updating its alias `bar`
 * silently does not, or the reverse, depending on which one sorts first.
 *
 * p22 recorded the name at bind time (Store.intent_of), so it can simply be
 * asked. Passing ~via restricts the rewrite to slots whose recorded intent
 * includes that name. This is not a reintroduction of name-sensitivity: nothing
 * is reconstructed and nothing is printed. The author said `foo`, that was
 * written down, and it is being read back. */
let relink_one =
    (
      st: Store.t,
      ~via: option(string)=?,
      ~from_: Hash.t,
      ~to_: Hash.t,
      h: Hash.t,
    )
    : option(rewrite) =>
  switch (Store.get_def(st, h)) {
  | None => None
  | Some(d) =>
    switch (Store.open_of(st, d), Store.link_of(st, d), Store.open_hash_of(st, d)) {
    | (Some(_), Some(l), Some(oh)) =>
      let old_link_h = Link.hash(l);
      let slot_matches = (slot: int): bool =>
        switch (via) {
        | None => true
        | Some(n) => List.mem(n, Store.intent_of(st, ~link=old_link_h, ~slot))
        };
      let l' = {
        ...l,
        Link.targets:
          Array.mapi(
            (slot, t) =>
              switch (t) {
              | Link.Def(x) when Hash.equal(x, from_) && slot_matches(slot) =>
                Link.Def(to_)
              | other => other
              },
            l.Link.targets,
          ),
      };
      if (Link.equal(l, l')) {
        None;
      } else {
        let old_link = old_link_h;
        let new_link = Store.put_link(st, l');
        /* Intent carries over: the author's names for these slots did not
           change because a dependency moved. This is the line that keeps a
           followed definition's recorded intent intact, which Unison cannot do
           because it never had it. */
        Array.iteri(
          (slot, _) =>
            List.iter(
              name => Store.record_intent(st, ~link=new_link, ~slot, ~name),
              Store.intent_of(st, ~link=old_link, ~slot),
            ),
          l'.Link.targets,
        );
        let d' = Def.Simple(oh, new_link);
        let new_hash = Store.put_def(st, d');
        /* The rebuilt definition keeps a type. Re-deriving it is cheap and is
           the honest thing to do, but note that it CANNOT differ from the old
           one when the edit was type-preserving: a definition's type is a
           function of its open term and the types of its link's targets, and
           the open term did not move. That is also why follow_clean can
           short-circuit on equal types rather than walking the cascade. */
        switch (Typecheck.type_def(st, d')) {
        | Ok(t) => Store.set_type(st, new_hash, t)
        | Error(_) =>
          switch (Store.type_of(st, h)) {
          | Some(t) => Store.set_type(st, new_hash, t)
          | None => ()
          }
        };
        Some({
          old_hash: h,
          new_hash,
          open_hash: oh,
          old_link,
          new_link,
          names: [],
        });
      };
    | _ => None
    }
  };

/* Move every name that pointed at old_hash to new_hash. This is the ONLY place
   propagation touches names, and it is after the fact: the rewrite already
   happened and did not consult them. */
let move_names = (ns: Namespace.t, r: rewrite): rewrite => {
  let names = Namespace.names_of(ns, r.old_hash);
  List.iter(n => Namespace.bind(ns, n, r.new_hash), names);
  {...r, names};
};

/* Follow: rewrite every transitive caller, breadth-first, rebinding names as
   it goes. Each rewritten definition becomes the next frontier's `from_`. */
let follow =
    (
      st: Store.t,
      ns: Namespace.t,
      ~via: option(string)=?,
      ~from_: Hash.t,
      ~to_: Hash.t,
      (),
    )
    : list(rewrite) => {
  let out = ref([]);
  let seen = Hashtbl.create(16);
  /* `via` applies to the FIRST hop only, and that is not a convenience -- it is
     where the ambiguity is. The sites that directly named the edited definition
     are the ones an alias can make ambiguous, so their recorded intent is what
     decides whether they meant this name. Deeper in the cascade there is
     nothing to disambiguate: a dependent of a dependent is following a hash
     that has exactly one meaning, and the name it used for the intermediate is
     its own business. Carrying `via` down would silently stop the cascade at
     depth one, which is what it did before this comment existed. */
  let rec go = (~via: option(string)=?, from_: Hash.t, to_: Hash.t, ()) => {
    let callers = Store.callers_of(st, from_);
    List.iter(
      c =>
        if (!Hashtbl.mem(seen, c)) {
          Hashtbl.add(seen, c, ());
          switch (relink_one(st, ~via?, ~from_, ~to_, c)) {
          | None => ()
          | Some(r) =>
            let r = move_names(ns, r);
            out := out^ @ [r];
            go(r.old_hash, r.new_hash, ());
          };
        },
      callers,
    );
  };
  go(~via?, from_, to_, ());
  out^;
};

/* Explicit: rewrite only the chosen sites, and do not recurse. Their own
   callers keep pointing at the versions they have, which is pin applied one
   level out -- the hybrid 04-naming-layer.md describes, with the set given
   rather than computed. */
let explicit =
    (st: Store.t, ns: Namespace.t, ~via: option(string)=?, ~from_: Hash.t, ~to_: Hash.t, sites: list(Hash.t))
    : list(rewrite) =>
  List.filter_map(
    h =>
      switch (relink_one(st, ~via?, ~from_, ~to_, h)) {
      | None => None
      | Some(r) => Some(move_names(ns, r))
      },
    sites,
  );

let apply =
    (st: Store.t, ns: Namespace.t, ~via: option(string)=?, ~from_: Hash.t, ~to_: Hash.t, s: strategy)
    : list(rewrite) =>
  switch (s) {
  | Pin => []
  | Follow => follow(st, ns, ~via?, ~from_, ~to_, ())
  | Explicit(sites) => explicit(st, ns, ~via?, ~from_, ~to_, sites)
  };

/* The sites a follow WOULD touch, without touching them. The reverse index
   makes this a graph walk rather than a search. */
let rec reachable_callers =
        (st: Store.t, ~seen: Hashtbl.t(Hash.t, unit)=Hashtbl.create(16), h: Hash.t)
        : list(Hash.t) => {
  let direct = Store.callers_of(st, h);
  List.concat_map(
    c =>
      if (Hashtbl.mem(seen, c)) {
        [];
      } else {
        Hashtbl.add(seen, c, ());
        [c, ...reachable_callers(st, ~seen, c)];
      },
    direct,
  );
};

/* follow-clean: would the whole cascade still typecheck?
 *
 * 04-naming-layer.md proposes this as a cacheable derived aspect keyed by
 * (h_old, h_new), short-circuited when the edit is type-preserving. The
 * short-circuit is worth noting because here it is nearly always available: if
 * the new definition has the same type as the old, no dependent's type can
 * change, because a dependent's type is a function of its open term and the
 * TYPES of its link's targets -- and the open term did not move. */
type clean_result =
  | Clean
  | Dirty(list((Hash.t, string)));

let follow_clean =
    (st: Store.t, ~from_: Hash.t, ~to_: Hash.t): clean_result => {
  switch (Store.type_of(st, from_), Store.type_of(st, to_)) {
  | (Some(a), Some(b)) when Ty.equal(a, b) => Clean
  | _ =>
    /* Simulate against a scratch copy of the type table: walk the cascade,
       retarget each link, and typecheck the result without storing anything. */
    let problems = ref([]);
    let sites = reachable_callers(st, from_);
    List.iter(
      h =>
        switch (Store.get_def(st, h)) {
        | None => ()
        | Some(d) =>
          switch (Store.open_of(st, d), Store.link_of(st, d)) {
          | (Some(o), Some(l)) =>
            let l' = Link.retarget(l, ~from_, ~to_);
            let env =
              Typecheck.{
                builtin: Builtins.type_of,
                def: x => Hash.equal(x, to_) ? Store.type_of(st, to_) : Store.type_of(st, x),
                self: _ => None,
              };
            switch (Typecheck.synth_top(env, l', o)) {
            | Ok(_) => ()
            | Error(m) => problems := problems^ @ [(h, m)]
            };
          | _ => ()
          }
        },
      sites,
    );
    problems^ == [] ? Clean : Dirty(problems^);
  };
};
