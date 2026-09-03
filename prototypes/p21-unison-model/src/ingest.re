/* Ingest. (def:uingest, def:bindgroup)
 *
 * hashComponents(B) decomposes a binding group into strongly connected
 * components in dependency order and folds def:comphash over them,
 * substituting Derived(h, i) for the names of already-hashed components before
 * hashing the next.
 *
 * Two things happen here that are the whole difference between p21 and every
 * other arbor prototype.
 *
 * First, ingest takes a GROUP, not a term. There is no recursive term former;
 * mutual recursion is several bindings that mention one another, so the unit
 * that can be given identities is the group (rem:mutrec).
 *
 * Second, NOTHING IS STORED UNTIL THE WHOLE GROUP TYPECHECKS. Identity is
 * conferred by typechecking: until then a binding is name-keyed syntax and
 * nothing else. arbor's own settled position is the opposite -- ingest
 * everything, and let what is uncommitted be the NAME. p21 implements Unison's
 * because that is what the model is. */

type binding = {
  name: string,
  term: Term.t, /* resolved, but still mentioning group members as variables */
  ty: Ty.t,
  /* A watch expression (`> e`) is a member of the binding group like any other
     -- Unison puts them in the SAME hashTermComponents call as the top-level
     definitions, and they come back with real reference ids. What differs is
     what happens afterwards: a watch is not registered in the store ("it is not
     added merely by watching it without a name") and never enters the
     namespace. Only its RESULT is kept, cached against its reference. That is
     the whole content of rem:watchkey -- there is no "is a definition" status
     attached to stored data that the cache has to respect. */
  is_watch: bool,
};

/* The consequence is (prop:watchdom): ingest mints an identity for a watch and
   then declines to store it, so the cache ends up keyed by a hash that denotes
   nothing in the store. Nothing has to special-case that, because nothing ever
   asked whether a hash "is a definition". */

let definition = (~name, ~term, ~ty) => {name, term, ty, is_watch: false};
let watch = (~name, ~term, ~ty) => {name, term, ty, is_watch: true};

type error =
  | Ill_typed(string, string) /* name, message */
  | Ordering_ambiguous(Abt.warning)
  | Escaped_variable(string);

let error_to_string = (e: error): string =>
  switch (e) {
  | Ill_typed(n, m) => n ++ ": " ++ m
  | Ordering_ambiguous(w) => Abt.warning_to_string(w)
  | Escaped_variable(x) => "free variable escaped ingest: " ++ x
  };

/* The result of a successful ingest: what each name now denotes, and the
   components that were created. */
type outcome = {
  /* Named definitions: what the caller should bind. */
  bindings: list((string, Reference.t)),
  /* The same definitions' stored forms, paired with their references. At save
     time nothing is in the store yet, so this doubles as the code lookup a
     watch needs in order to mention a definition the buffer has not committed
     -- Unison hands the runtime a CodeLookup over the file for the same
     reason. */
  defs: list((Reference.t, Term.t)),
  /* Watches: hashed, unregistered, unnamed. The caller evaluates and caches
     these; nothing else may refer to them, because nothing can name them. */
  watches: list((Reference.t, Term.t, Ty.t)),
  components: list((Hash.t, list(string))),
};

/* Typecheck the whole group before hashing any of it.
 *
 * Group members refer to each other by variable, and those variables have no
 * references yet -- so the context is seeded with each member's DECLARED type.
 * That is what makes a recursive group checkable at all, and it is why every
 * binding needs a signature in this language: with no inference there is
 * nowhere else for a recursive occurrence's type to come from. */
let typecheck_group =
    (env: Typecheck.env, group: list(binding)): result(unit, error) => {
  /* Every member's declared type seeds the context, which is what makes a
     recursive group checkable. A watch's "declared" type is the one synthesized
     for it; it is in the context too, harmlessly, since nothing can name it. */
  let ctx = List.map(b => (b.name, b.ty), group);
  let rec go = bs =>
    switch (bs) {
    | [] => Ok()
    | [b, ...rest] =>
      switch (
        try(Ok(Typecheck.check(env, ctx, b.term, b.ty))) {
        | Typecheck.Type_error(m) => Error(m)
        }
      ) {
      | Error(m) => Error(Ill_typed(b.name, m))
      | Ok () => go(rest)
      }
    };
  go(group);
};

/* hashComponents. Assumes the group typechecks. */
/* hashComponents. Assumes the group typechecks.
 *
 * `~register` is the whole difference between saving and committing, and it is
 * deliberately the ONLY difference: the hashes are identical either way. In
 * Unison, saving a scratch file runs `hashTermComponents` over the file and
 * hands the result to the runtime; the definitions do not enter the codebase
 * until `add`/`update`. So identity is conferred by TYPECHECKING, and storing
 * and naming are a separate, later gesture -- which is a sharper statement than
 * "identity is conferred at commit", and the one the model actually makes. */
let hash_components =
    (~register: bool=true, st: Store.t, group: list(binding))
    : result(outcome, error) => {
  let by_name = List.map(b => (b.name, b), group);
  let group_names = List.map(b => b.name, group);
  let graph =
    List.map(
      b =>
        (
          b.name,
          List.filter(v => List.mem(v, group_names), Term.free_vars(b.term)),
        ),
      group,
    );
  let sccs = Scc.components(graph);

  /* Names discharged so far, and what they now denote. */
  let resolved: ref(list((string, Reference.t))) = ref([]);
  let defs: ref(list((Reference.t, Term.t))) = ref([]);
  let watched: ref(list((Reference.t, Term.t, Ty.t))) = ref([]);
  let comps: ref(list((Hash.t, list(string)))) = ref([]);
  let err = ref(None);

  List.iter(
    comp_names =>
      if (err^ == None) {
        /* def:typeinhash -- wrap each member in its type before hashing,
           replacing an existing top-level annotation if there is one. The hash
           of a definition is therefore a function of its type as well as its
           body, and a change of signature is a change of identity. */
        let members =
          List.map(
            name => {
              let b = List.assoc(name, by_name);
              let body =
                switch (b.term) {
                | Term.Ann(inner, _) => inner
                | other => other
                };
              /* Discharge references to earlier components. */
              let body =
                List.fold_left(
                  (acc, (n, r)) => Term.subst_ref(n, r, acc),
                  body,
                  resolved^,
                );
              (name, Term.Ann(body, b.ty));
            },
            comp_names,
          );

        switch (
          try(Ok(Abt.hash_component(members))) {
          | Abt.Free_variable(x) => Error(Escaped_variable(x))
          }
        ) {
        | Error(e) => err := Some(e)
        | Ok((h, storage_order, warnings)) =>
          switch (warnings) {
          /* rem:notpartial: hashing is TOTAL and returns a hash here. What
             fails is presentation-independence. The refusal is the caller's,
             and this is the caller: p21 aborts rather than store a
             name-dependent identity (cor:detect). */
          | [w, ..._] => err := Some(Ordering_ambiguous(w))
          | [] =>
            /* The stored form substitutes this component's own freshly computed
               hash for the intra-cycle variables (rem:unhash, "in memory"). */
            let positions =
              List.mapi((i, (n, _)) => (n, Reference.Derived(h, i)), storage_order);
            let stored =
              List.map(
                ((n, tm)) => {
                  let tm =
                    List.fold_left(
                      (acc, (n2, r)) => Term.subst_ref(n2, r, acc),
                      tm,
                      positions,
                    );
                  (n, tm, List.assoc(n, by_name).ty);
                },
                storage_order,
              );
            /* A watch is always its own component -- nothing can reference it,
               since nothing can name it -- so this partition is never a split
               of a real cycle. */
            let is_watch = n => List.assoc(n, by_name).is_watch;
            if (!List.exists(((n, _, _)) => is_watch(n), stored)) {
              if (register) {
                Store.register(st, h, List.map(((_, tm, ty)) => (tm, ty), stored));
              };
              resolved := resolved^ @ positions;
              defs :=
                defs^
                @ List.map(((n, tm, _)) => (List.assoc(n, positions), tm), stored);
              comps := comps^ @ [(h, List.map(fst, storage_order))];
            } else {
              /* Hashed, and deliberately NOT registered. */
              watched :=
                watched^
                @ List.map(
                    ((n, tm, ty)) => (List.assoc(n, positions), tm, ty),
                    stored,
                  );
            };
          }
        };
      },
    sccs,
  );

  switch (err^) {
  | Some(e) => Error(e)
  | None =>
    Ok({bindings: resolved^, defs: defs^, watches: watched^, components: comps^})
  };
};

/* The whole path: typecheck the group, then hash it.
 *
 * `save` hashes without storing -- what writing the scratch file does.
 * `ingest` hashes and stores -- what `add`/`update` does. */
let save = (st: Store.t, group: list(binding)): result(outcome, error) => {
  let env = Store.build_env(st);
  switch (typecheck_group(env, group)) {
  | Error(e) => Error(e)
  | Ok () => hash_components(~register=false, st, group)
  };
};

let ingest = (st: Store.t, group: list(binding)): result(outcome, error) => {
  let env = Store.build_env(st);
  switch (typecheck_group(env, group)) {
  | Error(e) => Error(e)
  | Ok () => hash_components(~register=true, st, group)
  };
};
