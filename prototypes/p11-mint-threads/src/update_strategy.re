/* Update strategies — Pin / Follow / Migrate as substrate operations
   on top of three primitives: `Store.callers_of`, atomic
   `Namespace.multi_rebind`, and the `follow-clean:v1` aspect.

   Each strategy is invoked AFTER ingesting a new body (substructure
   hash) for the user's edit, together with the prior named hash that
   the user is editing. The new named hash is freshly wrapped in
   `apply_edit` so the mint mark inherits from the prior named hash
   (preserving the mint thread).

   - `Pin`: rebind `name` to the new named hash. Old hash becomes
     orphan; binding history records the new event.
   - `Follow(att)`: also cascade through every reachable Named caller
     of the old substructure body. Each caller is rebuilt by structural
     substitution (Follow_clean.substitute_in_term), wrapped with its
     own prior mint, and rebound under the same name(s) — atomically.
     Aborts if the dry-run cascade typechecks fail at any site.
   - `Migrate(sites, att)`: same as Follow but restricted to a chosen
     subset of caller named-hashes; unselected callers stay pinned to
     the old hash. */

type strategy =
  | Pin
  | Follow
  | Migrate(list(Hash.t)); /* selected caller named hashes */

type ok = {
  new_named: Hash.t,
  rebinds: list((string, Hash.t)),
};

type error =
  | Cascade_typecheck_failed(Hash.t /* offending old caller hash */, string)
  | Namespace_failed(Namespace.multi_rebind_error)
  | Edit_sort_mismatch(Definition.kind, Definition.kind);

let error_to_string =
  fun
  | Cascade_typecheck_failed(h, msg) =>
    "cascade typecheck failed at " ++ Hash.short(h) ++ ": " ++ msg
  | Namespace_failed(Namespace.Mr_reserved(n)) =>
    "namespace: reserved name " ++ n
  | Namespace_failed(Namespace.Mr_target_missing(n, h)) =>
    "namespace: missing target " ++ Hash.short(h) ++ " for " ++ n
  | Edit_sort_mismatch(expected, got) =>
    "edit-of sort mismatch: expected "
    ++ Definition.kind_to_string(expected)
    ++ ", got "
    ++ Definition.kind_to_string(got);

/* Build the set of (old_caller_named_hash, new_caller_named_hash)
   pairs the cascade would produce. The mapping is built incrementally:
   substructure substitutions accumulate so a caller whose body
   includes both `old_body` and another already-substituted child gets
   the latest substitution. */
let cascade_pairs =
    (
      ~store: Store.t,
      ~old_body: Hash.t,
      ~new_body: Hash.t,
      ~filter: option(Hash.t => bool),
    )
    : result(list((Hash.t, Hash.t, Hash.t)), error) => {
  /* (old_named, new_named, new_substructure_body) triples */
  let mapping = Hashtbl.create(8);
  Hashtbl.add(mapping, old_body, new_body);
  let candidates = Store.callers_closure(store, old_body);
  let results = ref([]);
  let error = ref(None);
  List.iter(
    caller_h =>
      if (error^ == None) {
        switch (Store.lookup(store, caller_h)) {
        | Some(Definition.Named_term(mint, body))
        | Some(Definition.Named_type(mint, body)) =>
          let include_ =
            switch (filter) {
            | None => true
            | Some(p) => p(caller_h)
            };
          if (include_) {
            let new_body' =
              Follow_clean.substitute_in_term(store, ~mapping, body);
            if (new_body' != body) {
              switch (Store.reconstruct(store, new_body')) {
              | None =>
                error :=
                  Some(
                    Cascade_typecheck_failed(
                      caller_h,
                      "could not reconstruct",
                    ),
                  )
              | Some(ast) =>
                let canonical = Canonicalize.canonicalize(ast);
                switch (Typecheck.check_top(canonical)) {
                | Typecheck.Ill_typed(msg) =>
                  error :=
                    Some(Cascade_typecheck_failed(caller_h, msg))
                | _ =>
                  let new_named =
                    switch (Store.lookup(store, caller_h)) {
                    | Some(Definition.Named_term(_, _)) =>
                      Store.register_named_term_with_mint(
                        store,
                        ~mint,
                        new_body',
                      )
                    | _ =>
                      Store.register_named_type_with_mint(
                        store,
                        ~mint,
                        new_body',
                      )
                    };
                  Hashtbl.replace(mapping, caller_h, new_named);
                  Hashtbl.replace(mapping, body, new_body');
                  results := [(caller_h, new_named, new_body'), ...results^];
                };
              };
            };
          };
        | _ => ()
        };
      },
    candidates,
  );
  switch (error^) {
  | Some(e) => Error(e)
  | None => Ok(List.rev(results^))
  };
};

/* Apply an edit. The caller has already:
   - ingested the new body (substructure hash) via `Store.ingest`
   - identified the prior named hash they were editing.

   We:
   - mint-preserve-wrap the new body using the prior named's mint,
     yielding the new top-level named hash.
   - cascade per strategy (Pin → no cascade; Follow → all callers;
     Migrate → selected callers).
   - bundle up all (name, new_named) rebinds and commit atomically. */
let apply_with_named =
    (
      ~ns,
      ~store,
      ~name: string,
      ~old_named: Hash.t,
      ~new_named: Hash.t,
      ~strategy: strategy,
    )
    : result(ok, error) => {
  let old_body = Store.unwrap_named(store, old_named);
  let new_body = Store.unwrap_named(store, new_named);
  let filter =
    switch (strategy) {
    | Pin => Some(_ => false)
    | Follow => None
    | Migrate(sites) =>
      Some(h => List.exists(s => s == h, sites))
    };
  switch (cascade_pairs(~store, ~old_body, ~new_body, ~filter)) {
  | Error(_) as e => e
  | Ok(triples) =>
    let rebinds = ref([(name, new_named)]);
    List.iter(
      ((old_caller, new_caller, _)) => {
        let names = Namespace.names_of(ns, old_caller);
        List.iter(
          n =>
            if (!List.exists(((nm, _)) => nm == n, rebinds^)) {
              rebinds := [(n, new_caller), ...rebinds^];
            },
          names,
        );
      },
      triples,
    );
    let updates = List.rev(rebinds^);
    let target_exists = h => Store.has(store, h);
    switch (Namespace.multi_rebind(ns, ~target_exists, updates)) {
    | Error(e) => Error(Namespace_failed(e))
    | Ok () => Ok({new_named, rebinds: updates})
    };
  };
};

let apply_edit =
    (
      ~ns: Namespace.t,
      ~store: Store.t,
      ~att as _: Attachment.t,
      ~name: string,
      ~old_named: Hash.t,
      ~new_body: Hash.t,
      ~strategy: strategy,
    )
    : result(ok, error) =>
  switch (Store.lookup(store, old_named)) {
  | Some(Definition.Named_term(mint, _)) =>
    let new_named =
      Store.register_named_term_with_mint(store, ~mint, new_body);
    apply_with_named(~ns, ~store, ~name, ~old_named, ~new_named, ~strategy);
  | Some(Definition.Named_type(mint, _)) =>
    let new_named =
      Store.register_named_type_with_mint(store, ~mint, new_body);
    apply_with_named(~ns, ~store, ~name, ~old_named, ~new_named, ~strategy);
  | Some(d) =>
    Error(Edit_sort_mismatch(Definition.Term_kind, Definition.kind(d)))
  | None =>
    Error(Edit_sort_mismatch(Definition.Term_kind, Definition.Term_kind))
  };
