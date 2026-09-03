/* Merge: reconciling two independently edited namespaces. (def:umerge)
 *
 * update rebuilds a namespace after one person edits it; merge reconciles two
 * that were edited apart. They share a mechanism -- both render stored
 * definitions to source and parse them back -- and therefore share
 * prop:print-parse's obligation. What merge adds is a decision update never has
 * to make: given that a definition differs on both branches, was it changed by
 * a HUMAN, or did it change because something underneath it did? That decision
 * is Synhash's, and everything below is organized around it.
 *
 * THE STORE NEVER MERGES (prop:merge-store). Two stores grown independently
 * from a common ancestor union without reconciliation: the union is a well
 * defined partial function, because equal hashes mean equal components up to
 * the identification comphash makes (thm:ualpha), and it satisfies the
 * no-dangling condition because each side is closed under its references and
 * closure survives union. So this module takes ONE store and three namespaces.
 *
 * That property is the formal content of "content-addressed codebases have no
 * merge conflicts", it is true, and it is close to vacuous (rem:merge-store):
 * nothing anyone calls a conflict lives in the store. It all lives in N, which
 * is a relation over names people chose. */

/* Roles. Alice is the branch being merged INTO, Bob the branch merged FROM.
   The asymmetry is not cosmetic: the naming function prefers Alice and so does
   the tie-break in `combine`, and prop:merge-asym is the consequence. */

/* ---- preconditions (def:mergepre) ------------------------------------------
 *
 * Three, and the third is what makes the rest well posed: everything below
 * indexes by NAME, so a definition must not be updatable at one of its names
 * and not at another. There is no lib in this model, so (ii) is absent. */
type precondition =
  | Conflicted_namespace(string) /* which branch, which name */
  | Conflicted_alias(string, string); /* two names, aliases in the LCA, now apart */

let precondition_to_string =
  fun
  | Conflicted_namespace(m) => "conflicted name: " ++ m
  | Conflicted_alias(x, y) =>
    "conflicted alias: " ++ x ++ " and " ++ y ++ " were the same definition";

/* ---- the diff (def:mergediff) -----------------------------------------------
 *
 * Per name, one of five. Only add / del / upd enter the diff: `Nil` is dropped
 * before any hashing (equal referents trivially have equal syntactic hashes, so
 * the case is settled on the content hash alone -- the narrowing step), and
 * `Prop` is separated out and set aside as an automatic propagation.
 *
 * The distinction between Prop and Upd is the ONLY place a content hash is
 * consulted at all, and even there it is consulted second: the comparison that
 * decides it is between SYNTACTIC hashes, and the type that carries a
 * definition through the diff upstream has an equality instance that looks at
 * nothing else (rem:syn-only). Once two syntactic hashes differ, nothing
 * downstream asks what the definitions actually are. A content-addressed system
 * decides its merges by name. */
type delta =
  | Nil
  | Add(Reference.referent)
  | Del(Reference.referent)
  | Prop(Reference.referent, Reference.referent) /* old, new -- rebuilt, not authored */
  | Upd(Reference.referent, Reference.referent); /* old, new -- renders differently */

let delta_tag =
  fun
  | Nil => "nil"
  | Add(_) => "add"
  | Del(_) => "del"
  | Prop(_, _) => "prop"
  | Upd(_, _) => "upd";

let delta_at =
    (st: Store.t, nu: Synhash.t, ~lca: Names.t, ~branch: Names.t, x: string)
    : delta => {
  switch (Names.resolve(lca, x), Names.resolve(branch, x)) {
  | (None, None) => Nil
  | (None, Some(r)) => Add(r)
  | (Some(r), None) => Del(r)
  /* Prop is the case that carries every guarantee in this file
     (cor:noedit): a definition whose binding changed only because update
     rebuilt it renders identically, so it never enters the diff and can never
     be conflicted. The hypothesis is nu(r) = nu(r') for each retargeted
     reference -- true when the edited dependency kept its name, false the
     moment it moved. */
  | (Some(o), Some(n)) =>
    if (Reference.referent_equal(o, n)) {
      Nil;
    } else {
      switch (Synhash.of_referent(st, nu, o), Synhash.of_referent(st, nu, n)) {
      | (Some(a), Some(b)) => Hash.equal(a, b) ? Prop(o, n) : Upd(o, n)
      | _ => Upd(o, n)
      };
    }
  };
};

/* Every name mentioned by any of the three namespaces. */
let all_names = (lca, alice, bob): list(string) =>
  List.sort_uniq(
    String.compare,
    Names.names(lca) @ Names.names(alice) @ Names.names(bob),
  );

/* The CORE diff: adds, deletes and authored updates. Nil and Prop are both
   held back -- Nil because equal referents are settled without hashing at all,
   Prop because an automatic propagation is not something to ask the user about
   (def:mergediff). Upstream this is one partition returning both halves; the
   propagated half is `propagated` below, and only the core is combined. */
let diff =
    (st: Store.t, nu: Synhash.t, ~lca: Names.t, ~branch: Names.t)
    : list((string, delta)) =>
  List.filter_map(
    x =>
      switch (delta_at(st, nu, ~lca, ~branch, x)) {
      | Nil
      | Prop(_, _) => None
      | d => Some((x, d))
      },
    all_names(lca, branch, branch),
  );

/* The other half of the partition: names whose binding changed but whose
   rendering did not. These are exactly the definitions update rebuilt and no
   human touched, and holding them back here is what makes cor:noedit true. */
let propagated =
    (st: Store.t, nu: Synhash.t, ~lca: Names.t, ~branch: Names.t)
    : list((string, delta)) =>
  List.filter_map(
    x =>
      switch (delta_at(st, nu, ~lca, ~branch, x)) {
      | Prop(_, _) as d => Some((x, d))
      | _ => None
      },
    all_names(lca, branch, branch),
  );

/* ---- combining the two diffs (def:mergecombine) ----------------------------
 *
 * A name is conflicted iff both sides add, or both sides update, AND the two
 * results render differently. Everything else resolves.
 *
 * The (Del, Upd) row is a deliberate choice upstream -- "These two are not a
 * conflicts, perhaps only temporarily, because it's easier to implement. We
 * just ignore these deletes and keep the updates" -- and it matters more than
 * it looks (rem:updel), because there is no rename operation at the level of
 * this diff. Moving foo to bar appears as Del at foo and Add at bar. So a
 * rename racing an edit of the renamed definition keeps BOTH halves: the new
 * name from the rename, the old name from the edit. See the merge_dup test. */
/* A conflict requires TWO syntactic changes (thm:noconflict): if either side's
   delta is Nil, Prop or Del, the name cannot be conflicted. Nil and Prop never
   reach `combine` at all, and Del pairs only as (Del,Del) or (Del,Upd), neither
   of which conflicts. So both parties must have changed how the definition
   RENDERS before anyone is asked to choose.
 *
 * Restated over what people did (def:namestable) (cor:noedit-user): if neither
 * branch runs move, alias or delete, and neither binds a name to a definition
 * that already has one, then no name authored on neither branch is conflicted.
 * Contrapositively -- the only user action that can put someone in front of a
 * conflict in code they never opened is a NAME MOVING. Names.move / alias /
 * delete are the gestures that break it, and test_merge_rename is the witness
 * (prop:rename-conflict).
 *
 * Nothing here computes the rename relation that would discharge the condition,
 * and that is faithful rather than lazy (cor:unchecked): upstream computes it
 * and passes it to a debug printer. */
exception Impossible_diff_pair;

type combined =
  | Bind(Reference.referent)
  | Unbind
  | Conflict(Reference.referent, Reference.referent); /* alice, bob */

let combine =
    (st: Store.t, nu: Synhash.t, a: option(delta), b: option(delta))
    : option(combined) => {
  let syn = r =>
    switch (Synhash.of_referent(st, nu, r)) {
    | Some(h) => Some(h)
    | None => None
    };
  let renders_same = (x, y) =>
    switch (syn(x), syn(y)) {
    | (Some(p), Some(q)) => Hash.equal(p, q)
    | _ => Reference.referent_equal(x, y)
    };
  switch (a, b) {
  | (None, None) => None
  /* one side only */
  | (Some(Add(r)), None)
  | (None, Some(Add(r))) => Some(Bind(r))
  | (Some(Upd(_, n)), None)
  | (None, Some(Upd(_, n))) => Some(Bind(n))
  | (Some(Del(_)), None)
  | (None, Some(Del(_))) => Some(Unbind)
  /* both sides */
  | (Some(Add(x)), Some(Add(y))) =>
    /* The tie-break takes ALICE's referent, and the naming function prefers
       Alice too, so merging bob-into-alice and alice-into-bob are different
       operations (prop:merge-asym): the same two branches can merge cleanly one
       way and fail the other, with no conflict reported either way. */
    renders_same(x, y) ? Some(Bind(x)) : Some(Conflict(x, y))
  | (Some(Upd(_, x)), Some(Upd(_, y))) =>
    renders_same(x, y) ? Some(Bind(x)) : Some(Conflict(x, y))
  | (Some(Del(_)), Some(Del(_))) => Some(Unbind)
  /* a delete never wins against an update (rem:updel) */
  | (Some(Del(_)), Some(Upd(_, n))) => Some(Bind(n))
  | (Some(Upd(_, n)), Some(Del(_))) => Some(Bind(n))
  /* Unreachable: `diff` holds Nil and Prop back. Kept total rather than
     raising, since nothing downstream depends on the distinction. */
  | (Some(Nil | Prop(_, _)), _)
  | (_, Some(Nil | Prop(_, _))) => None
  /* Add against Del or Upd cannot happen: one side says the name was absent
     from the LCA and the other says it was present. Upstream this is literally
     `error "impossible"`; keeping it an exception rather than a silent case
     preserves that, and a test asserts it is unreachable on the examples. */
  | (Some(Add(_)), Some(Del(_) | Upd(_, _)))
  | (Some(Del(_) | Upd(_, _)), Some(Add(_))) => raise(Impossible_diff_pair)
  };
};

/* ---- preconditions, checked (def:mergepre) --------------------------------- */

let conflicted_alias =
    (st: Store.t, nu: Synhash.t, ~lca: Names.t, ~branch: Names.t)
    : option((string, string)) => {
  /* Two names that denoted ONE referent in the LCA and different ones now. */
  let rec go = names =>
    switch (names) {
    | [] => None
    | [x, ...rest] =>
      switch (Names.resolve(lca, x)) {
      | None => go(rest)
      | Some(rx) =>
        let aliases =
          List.filter(y => !String.equal(x, y), Names.names_of(lca, rx));
        let bad =
          List.find_opt(
            y =>
              switch (Names.resolve(branch, x), Names.resolve(branch, y)) {
              | (Some(a), Some(b)) =>
                switch (
                  Synhash.of_referent(st, nu, a),
                  Synhash.of_referent(st, nu, b),
                ) {
                | (Some(p), Some(q)) => !Hash.equal(p, q)
                | _ => !Reference.referent_equal(a, b)
                }
              /* if one was deleted that is fine -- upstream says so explicitly */
              | _ => false
              },
            aliases,
          );
        switch (bad) {
        | Some(y) => Some((x, y))
        | None => go(rest)
        };
      }
    };
  go(Names.names(lca));
};

let check_preconditions =
    (st: Store.t, nu: Synhash.t, ~lca, ~alice, ~bob)
    : option(precondition) =>
  switch (
    Names.as_unconflicted(lca),
    Names.as_unconflicted(alice),
    Names.as_unconflicted(bob),
  ) {
  | (Error(n), _, _) => Some(Conflicted_namespace("ancestor: " ++ n))
  | (_, Error(n), _) => Some(Conflicted_namespace("alice: " ++ n))
  | (_, _, Error(n)) => Some(Conflicted_namespace("bob: " ++ n))
  | (Ok(_), Ok(_), Ok(_)) =>
    switch (conflicted_alias(st, nu, ~lca, ~branch=alice)) {
    | Some((x, y)) => Some(Conflicted_alias(x, y))
    | None =>
      switch (conflicted_alias(st, nu, ~lca, ~branch=bob)) {
      | Some((x, y)) => Some(Conflicted_alias(x, y))
      | None => None
      }
    }
  };

/* ---- the operation (def:umerge) --------------------------------------------
 *
 * Five steps. Step 5 covers two situations the implementation reports
 * identically (rem:merge-twofail): either a name is conflicted, in which case
 * the file holds two definitions under one name and cannot parse by
 * construction; or nothing is conflicted and the file STILL fails, because
 * rendering and re-parsing did not round-trip (prop:merge-print-parse). The
 * second is the interesting one and rem:merge-gap says why it is reachable:
 * the render environment prefers Alice's names, while the merged namespace has
 * applied BOTH branches' deletes and adds, and nothing requires a name the
 * renderer emits to still exist in the namespace it will be parsed against. */
type outcome =
  | Refused(precondition)
  | Merged({
      bound: list((string, Reference.referent)),
      unbound: list(string),
      rebuilt: list(string),
    })
  | Unmerged({
      conflicts: list(string),
      merged: list((string, Reference.referent)),
      file: string,
      message: string,
    });

let run =
    (~st: Store.t, ~lca: Names.t, ~alice: Names.t, ~bob: Names.t): outcome => {
  let nu = Synhash.make(~lca, ~alice, ~bob);
  switch (check_preconditions(st, nu, ~lca, ~alice, ~bob)) {
  | Some(p) => Refused(p)
  | None =>
    /* Step 1: the two diffs, combined. */
    let da = diff(st, nu, ~lca, ~branch=alice);
    let db = diff(st, nu, ~lca, ~branch=bob);
    let names = all_names(lca, alice, bob);
    let ops =
      List.filter_map(
        x => {
          let a = List.assoc_opt(x, da);
          let b = List.assoc_opt(x, db);
          switch (combine(st, nu, a, b)) {
          | None => None
          | Some(c) => Some((x, c))
          };
        },
        names,
      );
    let conflicts =
      List.filter_map(
        ((x, c)) =>
          switch (c) {
          | Conflict(_, _) => Some(x)
          | _ => None
          },
        ops,
      );

    /* The merged namespace: the LCA with the PROPAGATED updates applied first,
       then the resolved core operations on top. Conflicted names are bound to
       nothing here -- they go into the file.

       Applying the propagated half is not optional. It is held back from the
       core diff because nobody should be asked about it (cor:noedit), not
       because it should be discarded: a definition that update rebuilt is still
       the version that branch has. Dropping it would leave the merged namespace
       holding the ancestor's copy of everything downstream of any edit. */
    let merged = Names.copy(lca);
    List.iter(
      ((x, d)) =>
        switch (d) {
        | Prop(_, n) => Names.rebind(merged, x, n)
        | _ => ()
        },
      propagated(st, nu, ~lca, ~branch=bob) @ propagated(st, nu, ~lca, ~branch=alice),
    );
    List.iter(
      ((x, c)) =>
        switch (c) {
        | Bind(r) => Names.rebind(merged, x, r)
        | Unbind => Names.remove_name(merged, x)
        | Conflict(_, _) => Names.remove_name(merged, x)
        },
      ops,
    );

    /* Step 2: what has to be re-typechecked.
     *
     * NOT just the conflicts and their dependents. A definition that neither
     * branch touched can still be pointing at a version of a dependency that
     * the merge has moved out from under it, and if it is not rebuilt the
     * merged namespace keeps a stale reference nobody asked for.
     *
     * Unison computes this downwards, from a core dependency set with three
     * sources -- one's own updates and conflicts, and *the other* party's
     * changed names resolved on this branch ("if Bob updates the name foo, we
     * go find the thing Alice calls foo, no matter what its hash is"). The same
     * set is easier to characterize upwards, and that is what this does:
     *
     *   a referent is SUPERSEDED if the ancestor bound some name to it and the
     *   merged namespace does not bind that name to it any more,
     *
     * and D is every name the merged namespace binds whose definition reaches a
     * superseded referent. A rename supersedes (the old name is gone), an
     * update supersedes (the name means something else), and a definition whose
     * dependencies all still mean what they meant is left alone. */
    let superseded = (r: Reference.t): bool =>
      List.exists(
        x =>
          switch (Names.resolve(lca, x)) {
          | Some(Reference.Ref(lr)) when Reference.equal(lr, r) =>
            switch (Names.resolve(merged, x)) {
            | Some(Reference.Ref(mr)) => !Reference.equal(mr, r)
            | _ => true
            }
          | _ => false
          },
        Names.names(lca),
      );

    let rec reaches_superseded = (seen, r: Reference.t): bool =>
      if (List.exists(Reference.equal(r), seen)) {
        false;
      } else if (superseded(r)) {
        true;
      } else {
        List.exists(reaches_superseded([r, ...seen]), Store.deps(st, r));
      };

    let d =
      List.filter_map(
        n =>
          List.mem(n, conflicts)
            ? None
            : (
              switch (Names.resolve(merged, n)) {
              | Some(Reference.Ref(r)) =>
                List.exists(reaches_superseded([r]), Store.deps(st, r))
                  ? Some((n, r)) : None
              | _ => None
              }
            ),
        Names.names(merged),
      );
    /* A dependent is rendered and re-ingested rather than bound directly, so it
       leaves the merged namespace first -- exactly as update treats what it
       rebuilds. Conflicted names are already gone. */
    List.iter(((n, _)) => Names.remove_name(merged, n), d);

    /* Step 3: render, under a namer built exactly like nu. In the real system
       the render environment is suffixified and nu is not; names are atomic
       here, so the two coincide -- which does not weaken rem:merge-gap, since
       the gap comes from the ALICE BIAS and not from suffixification. */
    let rho = Ppe.make(Synhash.make(~lca, ~alice, ~bob));
    let rendered =
      String.concat(
        "\n\n",
        List.filter_map(
          ((n, r)) =>
            switch (Store.lookup(st, r), Store.type_of(st, r)) {
            | (Some(tm), Some(t)) => Some(Pretty.definition(rho, n, tm, t))
            | _ => None
            },
          d,
        ),
      );

    if (conflicts != []) {
      Unmerged({
        conflicts,
        merged: Names.rows(merged),
        file: rendered,
        message:
          "conflicted: " ++ String.concat(", ", conflicts),
      });
    } else if (rendered == "") {
      /* Nothing to re-typecheck: the resolved operations are the whole merge. */
      Merged({
        bound:
          List.filter_map(
            ((x, c)) =>
              switch (c) {
              | Bind(r) => Some((x, r))
              | _ => None
              },
            ops,
          ),
        unbound:
          List.filter_map(
            ((x, c)) =>
              switch (c) {
              | Unbind => Some(x)
              | _ => None
              },
            ops,
          ),
        rebuilt: [],
      });
    } else {
      /* Step 4: parse and typecheck the rendered file against the MERGED
         namespace -- which is neither party's (prop:merge-print-parse). */
      let (file, perrs) = Parse.file(rendered);
      if (perrs != []) {
        Unmerged({
          conflicts: [],
          merged: Names.rows(merged),
          file: rendered,
          message:
            "the rendered file does not parse against the merged namespace "
            ++ "(prop:merge-print-parse): "
            ++ String.concat(
                 "; ",
                 List.map((e: Parse.located_error) => e.message, perrs),
               ),
        });
      } else {
        let (es, problems) = Slurp.entries(file);
        if (problems != []) {
          Unmerged({
            conflicts: [],
            merged: Names.rows(merged),
            file: rendered,
            message:
              String.concat("; ", List.map(Slurp.problem_to_string, problems)),
          });
        } else {
          switch (
            Slurp.to_group(
              ~st,
              merged,
              Slurp.classify(merged, es),
              Slurp.watches(file),
            )
          ) {
          | Error(m) =>
            Unmerged({
              conflicts: [],
              merged: Names.rows(merged),
              file: rendered,
              message: m,
            })
          | Ok(group) =>
            switch (Ingest.ingest(st, group)) {
            | Error(e) =>
              Unmerged({
                conflicts: [],
                merged: Names.rows(merged),
                file: rendered,
                message: Ingest.error_to_string(e),
              })
            | Ok(o) =>
              List.iter(
                ((n, r)) => Names.rebind(merged, n, Reference.Ref(r)),
                o.bindings,
              );
              Merged({
                bound: Names.rows(merged),
                unbound:
                  List.filter_map(
                    ((x, c)) =>
                      switch (c) {
                      | Unbind => Some(x)
                      | _ => None
                      },
                    ops,
                  ),
                rebuilt: List.map(fst, o.bindings),
              });
            }
          };
        };
      };
    };
  };
};
