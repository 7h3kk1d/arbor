/* The operation log, and the three histories derived from it.
 *
 * One append-only log of typed events, with a grouping id so that a single
 * gesture spanning several objects stays one act. Everything the design doc
 * calls a "history" is a filter over it:
 *
 *   authored edits   where the OPEN hash changed     "the author edited this"
 *   link moves       where the LINK hash changed     "a dependency moved"
 *   name history     bind / rebind / unbind          "what this name meant"
 *
 * Git and Unison answer all three with one commit, which is why Unison has to
 * work out after the fact whether a definition changed because someone edited
 * it or because something underneath it moved. Its answer is synhash: hash the
 * definition again with names substituted for references, quotienting out the
 * dependencies' identities, and see whether what remains is the same. That is a
 * reconstruction, and its own source says it "cannot handle renames very well",
 * because the quotient is by name.
 *
 * Here the distinction is not reconstructed. A definition is a pair, the two
 * halves have separate hashes, and which half moved is a comparison. p11's mint
 * threads are the other prior answer and need an asserted mark to survive
 * edits; this needs nothing asserted, and survives renames by construction
 * because no name is involved in either hash.
 *
 * The grouping id is what fine granularity costs back. Histories this fine can
 * take a single gesture apart -- an edit touching two mutually recursive
 * definitions writes several events -- so undo needs to know they were one act.
 * That is also the unit a user wants to undo, which is a good sign it is the
 * right object. (Jujutsu's operation log is the model: recording continuously
 * rather than at checkpoints, so recovery never depends on having committed at
 * the right moment.) */

type event =
  | Defined({
      def: Hash.t,
      open_hash: Hash.t,
      link_hash: Hash.t,
    })
  | Relinked({
      old_def: Hash.t,
      new_def: Hash.t,
      open_hash: Hash.t, /* the same on both sides, by construction */
      old_link: Hash.t,
      new_link: Hash.t,
    })
  | Bound({
      name: string,
      def: Hash.t,
    })
  | Rebound({
      name: string,
      from_: Hash.t,
      to_: Hash.t,
    })
  | Unbound({
      name: string,
      from_: Hash.t,
    });

type entry = {
  seq: int,
  group: int,
  label: string, /* what the user asked for, for display */
  event,
};

type t = {
  mutable entries: list(entry), /* newest first */
  mutable next_seq: int,
  mutable next_group: int,
  mutable current_group: option(int),
  mutable current_label: string,
};

let create = (): t => {
  entries: [],
  next_seq: 0,
  next_group: 0,
  current_group: None,
  current_label: "",
};

/* Run `f` as one act. Everything recorded inside shares a group id, so undo
   takes it back as a unit. */
let gesture = (log: t, label: string, f: unit => 'a): 'a => {
  let g = log.next_group;
  log.next_group = g + 1;
  let prior = log.current_group;
  let prior_label = log.current_label;
  log.current_group = Some(g);
  log.current_label = label;
  let finish = () => {
    log.current_group = prior;
    log.current_label = prior_label;
  };
  switch (f()) {
  | r =>
    finish();
    r;
  | exception e =>
    finish();
    raise(e);
  };
};

/* `label` defaults to the enclosing gesture's, so individual recordings do not
   have to repeat what the user asked for. */
let record = (log: t, ~label=?, event: event): unit => {
  let label = Option.value(~default=log.current_label, label);
  let group =
    switch (log.current_group) {
    | Some(g) => g
    | None =>
      let g = log.next_group;
      log.next_group = g + 1;
      g;
    };
  log.entries = [{seq: log.next_seq, group, label, event}, ...log.entries];
  log.next_seq = log.next_seq + 1;
};

/* Oldest first, which is the order a history reads in. */
let all = (log: t): list(entry) => List.rev(log.entries);

let groups = (log: t): list((int, string, list(entry))) => {
  let tbl = Hashtbl.create(16);
  List.iter(
    e => {
      let prior = Option.value(~default=[], Hashtbl.find_opt(tbl, e.group));
      Hashtbl.replace(tbl, e.group, prior @ [e]);
    },
    all(log),
  );
  Hashtbl.fold((g, es, acc) => [(g, es), ...acc], tbl, [])
  |> List.sort(((a, _), (b, _)) => compare(a, b))
  |> List.map(((g, es)) => {
       let label =
         switch (es) {
         | [{label, _}, ..._] => label
         | [] => ""
         };
       (g, label, es);
     });
};

/* ---- the three histories ---- */

/* One version of a thing a name denoted. */
type version = {
  seq: int,
  group: int,
  label: string,
  def: Hash.t,
  open_hash: Hash.t,
  link_hash: Hash.t,
};

/* Every definition this name has denoted, oldest first. Follows the name
   through binds, rebinds, and the rebinds propagation performs. */
let versions_of_name = (log: t, name: string): list(version) => {
  let defs = ref([]);
  List.iter(
    e =>
      switch (e.event) {
      | Bound(b) when String.equal(b.name, name) => defs := defs^ @ [(e, b.def)]
      | Rebound(r) when String.equal(r.name, name) => defs := defs^ @ [(e, r.to_)]
      | _ => ()
      },
    all(log),
  );
  /* Attach each def's open/link hashes from the Defined/Relinked events. */
  let halves = Hashtbl.create(16);
  List.iter(
    e =>
      switch (e.event) {
      | Defined(d) => Hashtbl.replace(halves, d.def, (d.open_hash, d.link_hash))
      | Relinked(r) =>
        Hashtbl.replace(halves, r.new_def, (r.open_hash, r.new_link))
      | _ => ()
      },
    all(log),
  );
  List.filter_map(
    ((e: entry, def)) =>
      switch (Hashtbl.find_opt(halves, def)) {
      | Some((open_hash, link_hash)) =>
        Some({
          seq: e.seq,
          group: e.group,
          label: e.label,
          def,
          open_hash,
          link_hash,
        })
      | None => None
      },
    defs^,
  );
};

/* The subsequence where the OPEN half moved: the author edited the code. */
let authored_edits = (vs: list(version)): list(version) => {
  let rec go = (prev, acc, vs) =>
    switch (vs) {
    | [] => List.rev(acc)
    | [v, ...rest] =>
      switch (prev) {
      | Some(p) when Hash.equal(p, v.open_hash) => go(prev, acc, rest)
      | _ => go(Some(v.open_hash), [v, ...acc], rest)
      }
    };
  go(None, [], vs);
};

/* The subsequence where the LINK half moved: something underneath it did. */
let link_moves = (vs: list(version)): list(version) => {
  let rec go = (prev, acc, vs) =>
    switch (vs) {
    | [] => List.rev(acc)
    | [v, ...rest] =>
      switch (prev) {
      | Some(p) when Hash.equal(p, v.link_hash) => go(prev, acc, rest)
      | _ => go(Some(v.link_hash), [v, ...acc], rest)
      }
    };
  go(None, [], vs);
};

/* Every binding event for a name, in order. */
let name_history = (log: t, name: string): list(entry) =>
  List.filter(
    e =>
      switch (e.event) {
      | Bound(b) => String.equal(b.name, name)
      | Rebound(r) => String.equal(r.name, name)
      | Unbound(u) => String.equal(u.name, name)
      | Defined(_)
      | Relinked(_) => false
      },
    all(log),
  );

let event_to_string = (e: event): string =>
  switch (e) {
  | Defined(d) =>
    "defined " ++ Hash.short(d.def) ++ " = " ++ Hash.short(d.open_hash)
    ++ " @ " ++ Hash.short(d.link_hash)
  | Relinked(r) =>
    "relinked " ++ Hash.short(r.open_hash) ++ ": " ++ Hash.short(r.old_link)
    ++ " -> " ++ Hash.short(r.new_link)
  | Bound(b) => "bound " ++ b.name ++ " = " ++ Hash.short(b.def)
  | Rebound(r) =>
    "rebound " ++ r.name ++ ": " ++ Hash.short(r.from_) ++ " -> "
    ++ Hash.short(r.to_)
  | Unbound(u) => "unbound " ++ u.name
  };
