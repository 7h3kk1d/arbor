/* follow-clean:v1 derived aspect — per-(h_old, h_new) cached
   predicate over the dry-run cascade of an edit.

   "Clean" means: under the substitution (h_old → h_new), all reachable
   callers can be rebuilt by structural substitution and still typecheck.

   Short-circuit: if both `h_old` and `h_new` (named hashes) carry the
   same `Type_of` aspect, a structural substitution is *guaranteed* to
   preserve types at every site — every position that had a value of
   the old type now has a value of the new type, and those are the
   same type. We return `true` without running the cascade.

   Without that short-circuit, the cascade is run to first failure.
   Currently a thin substrate primitive: heavy lifting (re-canonicalize,
   re-typecheck) is delegated to Typecheck on the substituted body.

   Keyed by the pair-key encoding (BLAKE2B(h_old || h_new)) under
   procedure id "follow-clean:v1". */

let aspect_id = "follow-clean:v1";
let procedure_id = "follow-clean:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["p11"],
};

let type_of_named =
    (~store, ~att, h: Hash.t): option(Hash.t) => {
  let target = Store.unwrap_named(store, h);
  switch (
    Attachment.peek(
      att,
      ~target,
      ~aspect=Typecheck.aspect_id,
      ~procedure=Typecheck.procedure_id,
    )
  ) {
  | Some(Attachment.Type_of(ty_h)) => Some(ty_h)
  | Some(Attachment.Type_with_holes(ty_h)) => Some(ty_h)
  | _ => None
  };
};

/* Walk the substructure body of `h_old_named`'s body, substituting
   any occurrence of `old_body_h` for `new_body_h`. Returns the new
   substructure hash. */
let rec substitute_in_term =
        (store: Store.t, ~mapping: Hashtbl.t(Hash.t, Hash.t), h: Hash.t)
        : Hash.t =>
  switch (Hashtbl.find_opt(mapping, h)) {
  | Some(new_h) => new_h
  | None =>
    switch (Store.lookup_term(store, h)) {
    | None => h
    | Some(node) =>
      let map_one = c => substitute_in_term(store, ~mapping, c);
      let node' =
        switch (node) {
        | Node.Var(_)
        | Node.Int_lit(_)
        | Node.Bool_lit(_)
        | Node.String_lit(_)
        | Node.Hole => node
        | Node.Lam(ty, body) => Node.Lam(ty, map_one(body))
        | Node.App(a, b) => Node.App(map_one(a), map_one(b))
        | Node.Let(a, b) => Node.Let(map_one(a), map_one(b))
        | Node.If(c, t, e) =>
          Node.If(map_one(c), map_one(t), map_one(e))
        | Node.Pair(a, b) => Node.Pair(map_one(a), map_one(b))
        | Node.Fst(a) => Node.Fst(map_one(a))
        | Node.Snd(a) => Node.Snd(map_one(a))
        | Node.Tuple(items) => Node.Tuple(List.map(map_one, items))
        | Node.List_lit(items) => Node.List_lit(List.map(map_one, items))
        | Node.Record_lit(fields) =>
          Node.Record_lit(
            List.map(((l, v)) => (l, map_one(v)), fields),
          )
        | Node.Record_update(target, fields) =>
          Node.Record_update(
            map_one(target),
            List.map(((l, v)) => (l, map_one(v)), fields),
          )
        | Node.Project_field(target, label) =>
          Node.Project_field(map_one(target), label)
        | Node.Project_index(target, i) =>
          Node.Project_index(map_one(target), i)
        | Node.Prim(op, args) => Node.Prim(op, List.map(map_one, args))
        | Node.Prim_call(id, args) =>
          Node.Prim_call(id, List.map(map_one, args))
        };
      if (Node.equal(node', node)) {
        h;
      } else {
        Store.register_term(store, node');
      };
    }
  };

/* True if (h_old, h_new) is type-preserving (same Type_of). */
let same_type =
    (~store, ~att, ~h_old: Hash.t, ~h_new: Hash.t): bool =>
  switch (type_of_named(~store, ~att, h_old), type_of_named(~store, ~att, h_new)) {
  | (Some(t1), Some(t2)) => t1 == t2
  | _ => false
  };

/* Try a dry-run cascade. Returns true if every Named caller of the
   old substructure body can be substituted and still typechecks. */
let dry_run =
    (~store: Store.t, ~att as _: Attachment.t, ~h_old: Hash.t, ~h_new: Hash.t)
    : bool => {
  let body_old = Store.unwrap_named(store, h_old);
  let body_new = Store.unwrap_named(store, h_new);
  if (body_old == body_new) {
    /* Vacuous: same body. */
    true;
  } else {
    let mapping = Hashtbl.create(4);
    Hashtbl.add(mapping, body_old, body_new);
    let candidates = Store.callers_closure(store, body_old);
    /* For each candidate that is a Named_term body root, rebuild +
       typecheck. We only run typecheck on full Named bodies (top-level
       terms) — intermediate substructure children carry no top-level
       type and are typechecked transitively through their Named
       parents. */
    let ok = ref(true);
    List.iter(
      caller_h =>
        if (ok^) {
          switch (Store.lookup(store, caller_h)) {
          | Some(Definition.Named_term(_, body)) =>
            let new_body =
              substitute_in_term(store, ~mapping, body);
            switch (Store.reconstruct(store, new_body)) {
            | None => ok := false
            | Some(ast) =>
              let canonical = Canonicalize.canonicalize(ast);
              switch (Typecheck.check_top(canonical)) {
              | Typecheck.Ill_typed(_) => ok := false
              | _ => ()
              };
            };
          | _ => ()
          };
        },
      candidates,
    );
    ok^;
  };
};

/* Compute follow-clean:v1 for (h_old, h_new). Short-circuits on
   same-Type_of; otherwise runs the dry-run; caches the result. */
let compute =
    (~store, ~att, ~h_old: Hash.t, ~h_new: Hash.t): bool => {
  let key = Attachment.pair_key(~h_old, ~h_new);
  switch (
    Attachment.peek(att, ~target=key, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(Attachment.Follow_clean(b)) => b
  | _ =>
    let result =
      if (same_type(~store, ~att, ~h_old, ~h_new)) {
        true;
      } else {
        dry_run(~store, ~att, ~h_old, ~h_new);
      };
    Attachment.attach(
      att,
      ~target=key,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
      Attachment.Follow_clean(result),
    );
    result;
  };
};
