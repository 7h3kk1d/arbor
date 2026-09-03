/* Turning a parsed buffer into a binding group, and classifying it against the
 * namespace. (def:bindgroup)
 *
 * Unison calls the classification "slurping". It is a typed diff between the
 * editing context and the namespace, and it is what decides whether a name in
 * the buffer is new, unchanged, or a redefinition. */

type status =
  | New /* the namespace has no such name */
  | Redefines(Reference.t) /* the namespace has it; this changes it */
  | Duplicate(Reference.t); /* the namespace has it, byte-identical */

type entry = {
  name: string,
  ty: Ty.t,
  source: Term.Source.t,
  status,
};

type problem =
  | Missing_signature(string)
  | Missing_binding(string)
  | Duplicate_name(string);

let problem_to_string = (p: problem): string =>
  switch (p) {
  | Missing_signature(n) => n ++ ": needs a signature (`" ++ n ++ " : T`)"
  | Missing_binding(n) => n ++ ": has a signature but no definition"
  | Duplicate_name(n) => n ++ ": defined twice in the buffer"
  };

/* Pair signatures with bindings, in the order the bindings appear. */
let entries =
    (file: Surface.file): (list((string, Ty.t, Term.Source.t)), list(problem)) => {
  let sigs =
    List.filter_map(
      s =>
        switch (s) {
        | Surface.Signature(n, t) => Some((n, t))
        | _ => None
        },
      file.stanzas,
    );
  let binds =
    List.filter_map(
      s =>
        switch (s) {
        | Surface.Binding(n, e) => Some((n, e))
        | _ => None
        },
      file.stanzas,
    );
  let problems = ref([]);
  let seen = Hashtbl.create(8);
  let out =
    List.filter_map(
      ((n, e)) =>
        if (Hashtbl.mem(seen, n)) {
          problems := [Duplicate_name(n), ...problems^];
          None;
        } else {
          Hashtbl.replace(seen, n, true);
          switch (List.assoc_opt(n, sigs)) {
          | Some(t) => Some((n, t, e))
          | None =>
            problems := [Missing_signature(n), ...problems^];
            None;
          };
        },
      binds,
    );
  List.iter(
    ((n, _)) =>
      if (!List.mem_assoc(n, binds)) {
        problems := [Missing_binding(n), ...problems^];
      },
    sigs,
  );
  (out, List.rev(problems^));
};

let watches = (file: Surface.file): list(Term.Source.t) =>
  List.filter_map(
    s =>
      switch (s) {
      | Surface.Watch(e) => Some(e)
      | _ => None
      },
    file.stanzas,
  );

/* Classify each entry against the namespace. */
let classify =
    (ns: Names.t, es: list((string, Ty.t, Term.Source.t))): list(entry) =>
  List.map(
    ((name, ty, source)) => {
      let status =
        switch (Names.resolve(ns, name)) {
        | None => New
        | Some(r) => Redefines(Reference.referent_ref(r))
        };
      {name, ty, source, status};
    },
    es,
  );

/* Watches get internal names so they can sit in the binding group alongside the
   definitions. `#` is not in the identifier charset, so these cannot collide
   with anything a user wrote -- and the name is invisible to the hash anyway,
   since Abt discards binder names and a watch is always a singleton component
   whose sole de Bruijn index is 0. */
let watch_name = (i: int): string => "watch#" ++ string_of_int(i);

let is_watch_name = (n: string): bool =>
  String.length(n) > 6 && String.sub(n, 0, 6) == "watch#";

/* Resolve the whole buffer into a binding group. Every buffer name shadows the
   namespace, which is what makes co-dependent edits work with no gesture: if
   `f` calls `g` and both are in the buffer, `f`'s reference to `g` stays a
   variable and is discharged by ingest against the NEW `g`.

   Watch expressions are members of the same group. That is not a convenience:
   Unison puts them in the same `hashTermComponents` call as the top-level
   bindings (`allTerms = join tlcs ++ join (snd <$> watches)`), which is why a
   watch may mention a definition the buffer has not committed yet, and why it
   comes back with a real reference id.

   A watch has no declared type -- the language has no inference, but a watch is
   an expression rather than a definition, so synthesis is enough. Synthesis runs
   in the context of the group's declared types, so `> double 5` typechecks
   against an uncommitted `double`. */
let to_group =
    (~st: Store.t, ns: Names.t, es: list(entry), ws: list(Term.Source.t))
    : result(list(Ingest.binding), string) => {
  let group = List.map(e => e.name, es);
  let defs =
    List.map(
      e =>
        Ingest.definition(
          ~name=e.name,
          ~term=Resolve.term(~group, ~ns, e.source),
          ~ty=e.ty,
        ),
      es,
    );
  let env = Store.build_env(st);
  let ctx = List.map(e => (e.name, e.ty), es);
  let rec go = (i, acc, ws) =>
    switch (ws) {
    | [] => Ok(List.rev(acc))
    | [src, ...rest] =>
      let tm = Resolve.term(~group, ~ns, src);
      switch (
        try(Ok(Typecheck.synth(env, ctx, tm))) {
        | Typecheck.Type_error(m) => Error(m)
        }
      ) {
      | Error(m) => Error("watch " ++ string_of_int(i + 1) ++ ": " ++ m)
      | Ok(ty) =>
        go(
          i + 1,
          [Ingest.watch(~name=watch_name(i), ~term=tm, ~ty), ...acc],
          rest,
        )
      };
    };
  switch (go(0, [], ws)) {
  | Error(m) => Error(m)
  | Ok(watch_bindings) => Ok(defs @ watch_bindings)
  };
};
