/* Binding — the explicit transformation from an open term to a definition.
 *
 * This is the operation the design doc asks for: "Binding could then be an
 * explicit transformation that performs the edit time linking. That way you can
 * see how the linking history of a term changes independent of its open form
 * changing."
 *
 * In Unison and in p21 this step has no output of its own. Resolution consumes
 * a name, produces a hash, splices it into the term, and the name is gone; the
 * only artifact is the term. Here it produces a LINK, which is a stored object
 * with its own hash and its own history, and the names it consumed are recorded
 * rather than discarded.
 *
 * Everything else in the prototype follows from that one change. Propagation
 * rewrites links. The three histories are the histories of the two halves plus
 * the namespace. And the question "did the author edit this, or did something
 * underneath it move?" is answered by comparing hashes rather than by hashing
 * modulo names (Unison's synhash) or by carrying a mint (p11's threads). */

/* What happened to one slot. All five outcomes are recorded, including the two
   that fail, because p21's finding was that resolution's partiality is
   invisible in its own output -- three of its five routes produce an identical
   Var node. Here the outcome is a value. */
type outcome =
  | To_def(Hash.t) /* resolved to a definition in the namespace */
  | To_builtin(string) /* resolved to a primitive */
  | To_group(int) /* a sibling in the same binding group: Self(i) */
  | Unresolved; /* no such name -- stored anyway, as an unbound slot */

type slot_trace = {
  slot: int,
  name: string,
  outcome,
};

type result_t = {
  def: Def.t,
  hash: Hash.t,
  open_hash: Hash.t,
  link: Link.t,
  link_hash: Hash.t,
  trace: list(slot_trace),
};

let target_of_outcome = (o: outcome): Link.target =>
  switch (o) {
  | To_def(h) => Link.Def(h)
  | To_builtin(b) => Link.Builtin(b)
  | To_group(i) => Link.Self(i)
  | Unresolved => Link.Unbound
  };

/* Resolve one name. `group` is the names of the definitions being bound
   together in this gesture, in bundle order; a name in it becomes Self(i)
   rather than being looked up, which is what lets a recursive group be bound
   before any of its members exists. */
let resolve_name =
    (ns: Namespace.t, ~group: list(string), name: string): outcome =>
  switch (List.find_index(n => String.equal(n, name), group)) {
  | Some(i) => To_group(i)
  | None =>
    switch (Namespace.resolve(ns, name)) {
    | Some(h) => To_def(h)
    | None =>
      switch (Builtins.type_of(name)) {
      | Some(_) => To_builtin(name)
      | None => Unresolved
      }
    }
  };

/* Bind one abstraction. Stores the open term and the link, records intent for
   every slot, and registers the definition.
 *
 * Note what does NOT gate this: nothing. An abstraction with unresolvable names
 * produces a definition with unbound slots, which is stored and hashed like any
 * other. Typing it will fail, with a slot index and a name to report, but that
 * is a question you ask of the definition afterwards rather than a condition it
 * had to satisfy to exist. p21's ingest stores nothing until an entire binding
 * group typechecks; this is the inversion of that. */
let bind_abstraction =
    (
      st: Store.t,
      ns: Namespace.t,
      ~group: list(string)=[],
      a: Open_term.abstraction,
    )
    : result_t => {
  let open_hash = Store.put_open(st, a.Open_term.body);
  let trace =
    Array.to_list(
      Array.mapi(
        (i, name) => {slot: i, name, outcome: resolve_name(ns, ~group, name)},
        a.Open_term.slot_names,
      ),
    );
  let link: Link.t = {
    arity: a.Open_term.arity,
    targets:
      Array.of_list(List.map(t => target_of_outcome(t.outcome), trace)),
  };
  let link_hash = Store.put_link(st, link);
  List.iter(
    t => Store.record_intent(st, ~link=link_hash, ~slot=t.slot, ~name=t.name),
    trace,
  );
  let def = Def.Simple(open_hash, link_hash);
  let hash = Store.put_def(st, def);
  {def, hash, open_hash, link, link_hash, trace};
};

/* Bind a source term directly. */
let bind_source =
    (st: Store.t, ns: Namespace.t, ~group: list(string)=[], src: Term.Source.t)
    : result_t =>
  bind_abstraction(st, ns, ~group, Open_term.abstract(src));

/* Bind a source term and give it a name. */
let define =
    (
      st: Store.t,
      ns: Namespace.t,
      ~group: list(string)=[],
      ~ty: option(Ty.t)=?,
      name: string,
      src: Term.Source.t,
    )
    : result_t => {
  let r = bind_source(st, ns, ~group, src);
  switch (ty) {
  | Some(t) => Store.set_type(st, r.hash, t)
  | None =>
    switch (Typecheck.type_def(st, r.def)) {
    | Ok(t) => Store.set_type(st, r.hash, t)
    | Error(_) => ()
    }
  };
  Namespace.bind(ns, name, r.hash);
  r;
};

/* Bind a recursive group as one bundle. The members' open terms are abstracted
   and hashed independently -- none of them mentions any other -- and only the
   links form the cycle, through Self. */
let define_group =
    (
      st: Store.t,
      ns: Namespace.t,
      ~types: list((string, Ty.t))=[],
      members: list((string, Term.Source.t)),
    )
    : list((string, result_t)) => {
  let names = List.map(fst, members);
  let abstractions =
    List.map(((n, src)) => (n, Open_term.abstract(src)), members);
  let bundle_members =
    List.map(
      ((_, a: Open_term.abstraction)) => {
        let open_ = Store.put_open(st, a.Open_term.body);
        let targets =
          Array.map(
            name => target_of_outcome(resolve_name(ns, ~group=names, name)),
            a.Open_term.slot_names,
          );
        let link: Link.t = {arity: a.Open_term.arity, targets};
        {Bundle.open_, link};
      },
      abstractions,
    );
  let bundle: Bundle.t = {members: Array.of_list(bundle_members)};
  let bundle_hash = Store.put_bundle(st, bundle);
  List.mapi(
    (i, (name, a: Open_term.abstraction)) => {
      let def = Bundle.member_def(bundle, i);
      let closed = Bundle.close(bundle)[i];
      let link_hash = Store.put_link(st, closed);
      List.iteri(
        (slot, n) => Store.record_intent(st, ~link=link_hash, ~slot, ~name=n),
        Array.to_list(a.Open_term.slot_names),
      );
      let hash = Store.put_def(st, def);
      switch (List.assoc_opt(name, types)) {
      | Some(t) => Store.set_type(st, hash, t)
      | None => ()
      };
      Namespace.bind(ns, name, hash);
      (
        name,
        {
          def,
          hash,
          open_hash: bundle.Bundle.members[i].Bundle.open_,
          link: closed,
          link_hash,
          trace:
            Array.to_list(
              Array.mapi(
                (slot, n) =>
                  {slot, name: n, outcome: resolve_name(ns, ~group=names, n)},
                a.Open_term.slot_names,
              ),
            ),
        },
      );
    },
    abstractions,
  )
  |> (r => {
       ignore(bundle_hash);
       r;
     });
};

let outcome_to_string = (o: outcome): string =>
  switch (o) {
  | To_def(h) => "def " ++ Hash.short(h)
  | To_builtin(b) => "builtin " ++ b
  | To_group(i) => "group " ++ string_of_int(i)
  | Unresolved => "unresolved"
  };
