/* A session — store, namespace, cache and operation log, with the gestures a
 * user actually performs.
 *
 * The layers below are deliberately free of any log: Bind and Propagate are
 * pure with respect to history, and this is the one module that records. That
 * split matters because it is what makes the three histories a VIEW rather than
 * a feature -- nothing below knows the log exists, so nothing below can be
 * written in a way that depends on it. */

type t = {
  store: Store.t,
  ns: Namespace.t,
  watch: Watch.t,
  log: Oplog.t,
};

let create = (): t => {
  store: Store.create(),
  ns: Namespace.create(),
  watch: Watch.create(),
  log: Oplog.create(),
};

let record_define = (s: t, ~name: string, r: Bind.result_t): unit => {
  Oplog.record(
    s.log,
    Oplog.Defined({
      def: r.Bind.hash,
      open_hash: r.Bind.open_hash,
      link_hash: r.Bind.link_hash,
    }),
  );
  Oplog.record(s.log, Oplog.Bound({name, def: r.Bind.hash}));
};

/* Define a new name. */
let define =
    (s: t, ~ty: option(Ty.t)=?, name: string, src: Term.Source.t): Bind.result_t =>
  Oplog.gesture(s.log, "define " ++ name, () => {
    let r = Bind.define(s.store, s.ns, ~ty?, name, src);
    record_define(s, ~name, r);
    r;
  });

/* Define a recursive group as one act. The grouping id is what makes this one
   act rather than several: the log records a Defined and a Bound per member,
   and they share a group. */
let define_group =
    (s: t, ~types: list((string, Ty.t))=[], members: list((string, Term.Source.t)))
    : list((string, Bind.result_t)) =>
  Oplog.gesture(
    s.log,
    "define group " ++ String.concat(", ", List.map(fst, members)),
    () => {
      let bound = Bind.define_group(s.store, s.ns, ~types, members);
      List.iter(((name, r)) => record_define(s, ~name, r), bound);
      bound;
    },
  );

/* Edit an existing name's body, then propagate to its dependents under a
   strategy.
 *
 * This is the gesture the whole prototype is about. The author changed the
 * code, so a new OPEN term is produced; every dependent that follows keeps its
 * own open term exactly and gets a new LINK. Both facts land in the log as
 * different event constructors, which is what makes "the author edited this"
 * and "something underneath it moved" separable afterwards without hashing
 * anything modulo names. */
let edit =
    (
      s: t,
      ~ty: option(Ty.t)=?,
      ~strategy: Propagate.strategy=Propagate.Follow,
      name: string,
      src: Term.Source.t,
    )
    : result((Bind.result_t, list(Propagate.rewrite)), string) =>
  switch (Namespace.resolve(s.ns, name)) {
  | None => Error("no such name: " ++ name)
  | Some(old_hash) =>
    Ok(
      Oplog.gesture(s.log, "edit " ++ name, () => {
        let r = Bind.bind_source(s.store, s.ns, src);
        switch (ty) {
        | Some(t) => Store.set_type(s.store, r.Bind.hash, t)
        | None =>
          switch (Typecheck.type_def(s.store, r.Bind.def)) {
          | Ok(t) => Store.set_type(s.store, r.Bind.hash, t)
          | Error(_) => ()
          }
        };
        Oplog.record(
          s.log,
          Oplog.Defined({
            def: r.Bind.hash,
            open_hash: r.Bind.open_hash,
            link_hash: r.Bind.link_hash,
          }),
        );
        Namespace.bind(s.ns, name, r.Bind.hash);
        Oplog.record(
          s.log,
          Oplog.Rebound({name, from_: old_hash, to_: r.Bind.hash}),
        );
        /* ~via=name: propagate to the sites that said THIS name. With an alias
           present that is the difference between reaching the right sites and
           guessing, and it is only available because bind recorded it. */
        let rewrites =
          Propagate.apply(
            s.store, s.ns, ~via=name, ~from_=old_hash, ~to_=r.Bind.hash, strategy);
        List.iter(
          (rw: Propagate.rewrite) => {
            Oplog.record(
              s.log,
              Oplog.Relinked({
                old_def: rw.Propagate.old_hash,
                new_def: rw.Propagate.new_hash,
                open_hash: rw.Propagate.open_hash,
                old_link: rw.Propagate.old_link,
                new_link: rw.Propagate.new_link,
              }),
            );
            List.iter(
              n =>
                Oplog.record(
                  s.log,
                  Oplog.Rebound({
                    name: n,
                    from_: rw.Propagate.old_hash,
                    to_: rw.Propagate.new_hash,
                  }),
                ),
              rw.Propagate.names,
            );
          },
          rewrites,
        );
        (r, rewrites);
      }),
    )
  };

let alias = (s: t, ~existing: string, ~new_name: string): result(unit, string) =>
  Oplog.gesture(s.log, "alias " ++ existing ++ " as " ++ new_name, () =>
    switch (Namespace.resolve(s.ns, existing)) {
    | None => Error("no such name: " ++ existing)
    | Some(h) =>
      Namespace.bind(s.ns, new_name, h);
      Oplog.record(s.log, Oplog.Bound({name: new_name, def: h}));
      Ok();
    }
  );

let rename = (s: t, ~from_: string, ~to_: string): result(unit, string) =>
  Oplog.gesture(s.log, "rename " ++ from_ ++ " to " ++ to_, () =>
    switch (Namespace.resolve(s.ns, from_)) {
    | None => Error("no such name: " ++ from_)
    | Some(h) =>
      Namespace.unbind(s.ns, from_);
      Oplog.record(s.log, Oplog.Unbound({name: from_, from_: h}));
      Namespace.bind(s.ns, to_, h);
      Oplog.record(s.log, Oplog.Bound({name: to_, def: h}));
      Ok();
    }
  );

let eval_name = (s: t, name: string): result(Eval.value, string) =>
  switch (Namespace.resolve(s.ns, name)) {
  | None => Error("no such name: " ++ name)
  | Some(h) =>
    switch (Store.get_def(s.store, h)) {
    | None => Error("no such definition")
    | Some(d) => Watch.eval_cached(s.watch, s.store, d)
    }
  };

/* ---- the three histories, for one name ---- */

type histories = {
  versions: list(Oplog.version),
  authored: list(Oplog.version),
  links: list(Oplog.version),
  names: list(Oplog.entry),
};

let histories_of = (s: t, name: string): histories => {
  let versions = Oplog.versions_of_name(s.log, name);
  {
    versions,
    authored: Oplog.authored_edits(versions),
    links: Oplog.link_moves(versions),
    names: Oplog.name_history(s.log, name),
  };
};
