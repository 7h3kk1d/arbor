/* Update. (def:uscope, def:update, rem:step5, rem:residual, prop:follow-only)
 *
 * The five steps of (def:update). The operation that makes this a model of
 * Unison rather than a content-
 * addressed store with a namespace on top. Propagating an edit to the code that
 * depends on it is not a rename but a REBUILD, and the rebuild goes through
 * surface syntax:
 *
 *   render the dependents to text -> parse the text -> resolve the names
 *   against the NEW namespace -> typecheck -> re-ingest
 *
 * Retargeting happens at the resolution step, because a printed NAME resolves
 * against the new namespace. A dependent's source text is character-for-
 * character what it was, and its hash changes anyway. The implementation's own
 * comment for this is "the world's weirdest implementation of AST
 * substitution", and it is correct only if printing inverts parsing
 * (prop:print-parse) -- an obligation Unison does not state and maintains with
 * a golden-file corpus.
 *
 * There is exactly ONE propagation strategy and no parameter controlling it
 * (prop:follow-only). Every transitive dependent with a name gets rebuilt;
 * there is no way to ask that some callers be left pointing at the old
 * definition. arbor's pin / follow / explicit framing has no counterpart here,
 * which is worth knowing before treating that framing as descriptive of prior
 * art. */

type outcome =
  | Succeeded({
      bound: list((string, Reference.t)),
      rebuilt: list(string), /* dependents that were rebuilt */
      /* Watch expressions: hashed in the same call as the definitions, never
         registered and never named, results cached against their references
         (rem:watchkey). */
      watched: list((Reference.t, Ty.t, string)),
    })
  | Conflicted(string) /* a conflicted name; update refuses to start */
  | Failed({
      /* Step 5. Every name the file bound and every dependent that was
         rendered has left the namespace, and this text is their only home. */
      evicted: list(string),
      buffer: string,
      message: string,
    });

/* tdeps(Q). (def:uscope)
 *
 *   tdeps(Q) = (dependents+(Q) intersect S) \ F
 *
 * S is the set of references named in N -- and it is well defined only because
 * update has already demanded that N be unconflicted as a whole (def:unconf has
 * no way to take the unconflicted PART of a namespace). F is what the incoming
 * file shadows: anything you have checked out and fixed yourself is not
 * something the system will try to fix. Naming is what puts a definition in
 * scope: an unnamed definition is not a dependent to be updated even if it
 * references the target. */
let tdeps =
    (st: Store.t, ns: Names.t, ~shadowed: list(string), q: list(Reference.t))
    : list((string, Reference.t)) => {
  let closure = Store.transitive_dependents(st, q);
  List.filter_map(
    r =>
      switch (Names.names_of(ns, Reference.Ref(r))) {
      | [] => None /* unnamed: out of scope */
      | [n, ..._] => List.mem(n, shadowed) ? None : Some((n, r))
      },
    closure,
  );
};

/* Render the dependents to source. (def:update step 2) */
let render =
    (st: Store.t, ppe: Ppe.t, ds: list((string, Reference.t))): string =>
  String.concat(
    "\n\n",
    List.filter_map(
      ((n, r)) =>
        switch (Store.lookup(st, r), Store.type_of(st, r)) {
        | (Some(tm), Some(t)) => Some(Pretty.definition(ppe, n, tm, t))
        | _ => None
        },
      ds,
    ),
  );

let banner = "-- The definitions below no longer typecheck with the changes above.\n-- Please fix the errors and try again.";

/* The whole operation. `text` is the buffer's live region, already parsed into
   `file`; it is passed through so the failure path can hand it back. */
let run =
    (~st: Store.t, ~ns: Names.t, ~w: Watch.t, ~text: string, ~file: Surface.file)
    : outcome => {
  /* Step 0: assert unconflictedness. This is the first thing handleUpdate2
     does, and it is where the cost of a relation-valued namespace lands: a
     representable state that the operations refuse. */
  switch (Names.as_unconflicted(ns)) {
  | Error(bad) => Conflicted(bad)
  | Ok(_) =>
    let (es, problems) = Slurp.entries(file);
    if (problems != []) {
      Failed({
        evicted: [],
        buffer: text,
        message: String.concat("; ", List.map(Slurp.problem_to_string, problems)),
      });
    } else {
      let classified = Slurp.classify(ns, es);
      let file_names = List.map((e: Slurp.entry) => e.name, classified);

      /* Step 1: Q, and its transitive dependents. */
      let q =
        List.filter_map(
          (e: Slurp.entry) =>
            switch (e.status) {
            | Slurp.Redefines(r) => Some(r)
            | _ => None
            },
          classified,
        );
      let ds = tdeps(st, ns, ~shadowed=file_names, q);

      /* Step 2: render. */
      let ppe = Ppe.make(ns);
      let rendered = render(st, ppe, ds);

      /* Step 3: parse and typecheck F u S AS ONE FILE. The union is the point:
         a dependent is re-typechecked against the new definition, in the same
         binding group, which is what lets the retarget happen by resolution. */
      let combined = rendered == "" ? text : text ++ "\n\n" ++ rendered;
      let (combined_file, perrs) = Parse.file(combined);
      if (perrs != []) {
        /* A round-trip failure: what we rendered did not parse back. This is
           prop:print-parse failing, and it is the one error mode the model
           says should be impossible. */
        Failed({
          evicted: [],
          buffer: combined,
          message:
            "rendered dependents did not parse back (prop:print-parse): "
            ++ String.concat(
                 "; ",
                 List.map((e: Parse.located_error) => e.message, perrs),
               ),
        });
      } else {
        let (ces, cproblems) = Slurp.entries(combined_file);
        if (cproblems != []) {
          Failed({
            evicted: [],
            buffer: combined,
            message:
              String.concat("; ", List.map(Slurp.problem_to_string, cproblems)),
          });
        } else {
          switch (
            Slurp.to_group(
              ~st,
              ns,
              Slurp.classify(ns, ces),
              Slurp.watches(combined_file),
            )
          ) {
          | Error(m) =>
            Failed({evicted: [], buffer: combined, message: m})
          | Ok(group) =>
          switch (Ingest.ingest(st, group)) {
          /* Step 4: it typechecks. Ingest and rebind every affected name. */
          | Ok(outcome) =>
            List.iter(
              ((n, r)) => Names.rebind(ns, n, Reference.Ref(r)),
              outcome.bindings,
            );
            /* The watches. Evaluate each and write the result into the cache
               against its own reference. The expression itself is NOT put in
               the store -- "it is not added merely by watching it without a
               name" -- so the cache holds an entry keyed by a hash that names
               nothing. That is exactly the point of rem:watchkey: there is no
               "is a definition" status the cache has to respect. */
            let watched =
              List.map(
                ((r, tm, ty)) => {
                  let rendered =
                    switch (Watch.get(w, r)) {
                    | Some(v) => Pretty.term(Ppe.make(ns), v)
                    | None =>
                      switch (Eval.eval_top(st, tm)) {
                      | Ok(v) =>
                        let t = Eval.to_term(v);
                        Watch.put(w, r, t);
                        Pretty.term(Ppe.make(ns), t);
                      | Error(m) => m
                      }
                    };
                  (r, ty, rendered);
                },
                outcome.watches,
              );
            Succeeded({
              bound: outcome.bindings,
              rebuilt: List.map(fst, ds),
              watched,
            });
          /* Step 5: it does not. NOTHING IS INGESTED, and the removal is
             wider than "the offending definitions" -- every name the file
             binds and every dependent that was rendered leaves the namespace,
             whatever the typechecker's verdict on each (rem:step5). The
             namespace is left with no half-applied edit; what the user was
             trying to bind is left denoting nothing at all, rather than still
             denoting its old referent. What is delivered is F u S: the user's
             own file plus the dependents it broke. */
          | Error(e) =>
            let evicted = file_names @ List.map(fst, ds);
            Names.remove_names(ns, evicted);
            Failed({
              evicted,
              buffer:
                rendered == ""
                  ? text : text ++ "\n\n" ++ banner ++ "\n\n" ++ rendered,
              message: Ingest.error_to_string(e),
            });
          };
          };
        };
      };
    };
  };
};
