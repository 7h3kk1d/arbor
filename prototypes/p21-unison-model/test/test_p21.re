/* p21 substrate tests.
 *
 * Every test names the statement of formalism/paper/mini-unison.tex that it
 * checks, in the form formalism/scripts/check-labels.sh looks for. The paper
 * and the prototype are meant to be a checked pair: if a statement here fails,
 * one of the two is wrong, and which one is a question rather than a bug. */

open P21_substrate;

let check = (msg, expected, actual) =>
  Alcotest.(check(bool))(msg, expected, actual);

let nat_plus = Term.Ref(Reference.Builtin("Nat.+"));
let app2 = (f, a, b) => Term.App(Term.App(f, a), b);
let nat2nat = Ty.Arrow(Ty.Nat, Ty.Nat);

let comphash = ms => {
  let (h, _, _) = Abt.hash_component(ms);
  h;
};
let storage_order = ms => {
  let (_, o, _) = Abt.hash_component(ms);
  List.map(fst, o);
};
let warnings = ms => {
  let (_, _, w) = Abt.hash_component(ms);
  w;
};

/* ---- hashing ---- */

let test_hash_determinism = () => {
  let t = Term.Lam("x", Term.Var("x"));
  check("same term, same hash", true, Hash.equal(Abt.hash_closed(t), Abt.hash_closed(t)));
  check(
    "different terms, different hashes",
    false,
    Hash.equal(Abt.hash_closed(t), Abt.hash_closed(Term.Lam("x", Term.Nat(1)))),
  );
};

/* (thm:ualpha) Hashing identifies terms up to alpha-equivalence and cycle
   permutation, and no further. */
let test_ualpha = () => {
  check(
    "\\x.x and \\y.y share a hash",
    true,
    Hash.equal(
      Abt.hash_closed(Term.Lam("x", Term.Var("x"))),
      Abt.hash_closed(Term.Lam("y", Term.Var("y"))),
    ),
  );
  /* ... and no further: observationally equal but structurally distinct terms
     do NOT collide. */
  check(
    "1 + 0 and 1 do not collide",
    false,
    Hash.equal(
      Abt.hash_closed(app2(nat_plus, Term.Nat(1), Term.Nat(0))),
      Abt.hash_closed(Term.Nat(1)),
    ),
  );
  /* Cycle permutation: listing the same component two ways gives one hash,
     provided the side condition holds. */
  let f = Term.Ann(Term.Lam("n", Term.App(Term.Var("g"), Term.Var("n"))), nat2nat);
  let g =
    Term.Ann(
      Term.Lam("n", Term.App(Term.Var("f"), app2(nat_plus, Term.Var("n"), Term.Nat(1)))),
      nat2nat,
    );
  check(
    "cycle permutation is collapsed",
    true,
    Hash.equal(comphash([("f", f), ("g", g)]), comphash([("g", g), ("f", f)])),
  );
};

/* (prop:transparent) Transparency holds exactly at position 0. */
let test_transparent = () => {
  let h = Abt.hash_closed(Term.Nat(1));
  check(
    "position 0 contributes the referent's hash literally",
    true,
    Hash.equal(Abt.hash_closed(Term.Ref(Reference.Derived(h, 0))), h),
  );
  check(
    "position 1 is tagged, and opaque",
    true,
    Hash.equal(
      Abt.hash_closed(Term.Ref(Reference.Derived(h, 1))),
      Hash.acc([Hash.Tag(1), Hash.Hashed(h), Hash.Nat(1)]),
    ),
  );
};

/* (rem:transparency-limit) The usual summary of transparency is too strong.
   A position-0 reference contributes the referent's COMPONENT hash; inlining
   would contribute its TERM hash, and def:comphash puts two accumulations
   between them -- even at k = 1. So `x = 1 + 1` and `y = x` do NOT hash the
   same, and inlining does not preserve an enclosing term's hash.

   This is the claim the paper flags as the one it would most like checked. It
   is checked here. */
let test_transparency_limit = () => {
  let bx = app2(nat_plus, Term.Nat(1), Term.Nat(1));
  let x = Term.Ann(bx, Ty.Nat);
  let hx = comphash([("x", x)]);
  check(
    "comphash(C) /= the member's term hash, even at k = 1",
    false,
    Hash.equal(hx, Abt.hash_closed(x)),
  );
  let y = Term.Ann(Term.Ref(Reference.Derived(hx, 0)), Ty.Nat);
  check(
    "`x = 1 + 1` and `y = x` do NOT hash the same",
    false,
    Hash.equal(hx, comphash([("y", y)])),
  );
  check(
    "inlining does not preserve an enclosing term's hash",
    false,
    Hash.equal(
      Abt.hash_closed(Term.App(Term.Ref(Reference.Derived(hx, 0)), Term.Nat(0))),
      Abt.hash_closed(Term.App(bx, Term.Nat(0))),
    ),
  );
};

/* (def:typeinhash) The type is inside the term hash. */
let test_typeinhash = () => {
  let body = Term.Lam("n", Term.Var("n"));
  check(
    "same body, different signature, different hash",
    false,
    Hash.equal(
      Abt.hash_closed(Term.Ann(body, nat2nat)),
      Abt.hash_closed(Term.Ann(body, Ty.Arrow(Ty.Text, Ty.Text))),
    ),
  );
};

/* (def:tokens) The stream is tagged and length-prefixed, and every stream
   begins with a version token. Length-prefixing is what stops two adjacent
   payloads from being confusable. */
let test_tokens = () => {
  check(
    "text is length-prefixed, so \"ab\"+\"c\" /= \"a\"+\"bc\"",
    false,
    Hash.equal(
      Hash.acc([Hash.Text("ab"), Hash.Text("c")]),
      Hash.acc([Hash.Text("a"), Hash.Text("bc")]),
    ),
  );
  check(
    "layer tags separate a type from a term",
    false,
    Hash.equal(Ty.hash(Ty.Nat), Abt.hash_closed(Term.Nat(0))),
  );
};

/* ---- component hashing ---- */

/* (def:comphash), (rem:twoorders) The algorithm computes two distinct
   orderings: pi fixes de Bruijn indices while hashing bodies, sigma fixes the
   stored positions. sigma sorts by a hash of the pi-index, so they coincide
   only by accident -- and when they differ, which member is referenced
   transparently is not the member the user wrote first. */
let test_twoorders = () => {
  let f = Term.Ann(Term.Lam("n", Term.App(Term.Var("g"), Term.Var("n"))), nat2nat);
  let g =
    Term.Ann(
      Term.Lam("n", Term.App(Term.Var("f"), app2(nat_plus, Term.Var("n"), Term.Nat(1)))),
      nat2nat,
    );
  let order = storage_order([("f", f), ("g", g)]);
  check("both members are placed", true, List.length(order) == 2);
  check("no ordering warning: the bodies differ", true, warnings([("f", f), ("g", g)]) == []);
};

/* (thm:ordering)(a) When the permutation hash is injective,
   presentation-independence holds: the component hash, the member hashes and
   each member's Pos are all independent of how the component was listed. */
let test_ordering_a = () => {
  let f = Term.Ann(Term.Lam("n", Term.App(Term.Var("g"), Term.Var("n"))), nat2nat);
  let g =
    Term.Ann(
      Term.Lam("n", Term.App(Term.Var("f"), app2(nat_plus, Term.Var("n"), Term.Nat(1)))),
      nat2nat,
    );
  let h1 = Term.Ann(Term.Lam("n", app2(nat_plus, Term.Var("n"), Term.Nat(7))), nat2nat);
  let listings = [
    [("f", f), ("g", g), ("h", h1)],
    [("g", g), ("f", f), ("h", h1)],
    [("h", h1), ("g", g), ("f", f)],
  ];
  let hashes = List.map(comphash, listings);
  let orders = List.map(storage_order, listings);
  check(
    "comphash is listing-independent",
    true,
    List.for_all(h => Hash.equal(h, List.hd(hashes)), hashes),
  );
  check(
    "and so is the storage order, hence every Pos",
    true,
    List.for_all(o => o == List.hd(orders), orders),
  );
};

/* (thm:ordering)(b1) When the permutation hash is NOT injective, which member
   receives which Pos depends on the members' names -- always. Renaming can
   change what Derived(h, 0) denotes, and hence which member of the cycle is
   referenced transparently.

   This is the half the paper originally stated too strongly: for ping/pong the
   component hash is UNCHANGED by the rename. Only the assignment moves. */
let test_ordering_b1 = () => {
  let mk = other =>
    Term.Ann(Term.Lam("n", Term.App(Term.Var(other), Term.Var("n"))), nat2nat);
  let before = [("ping", mk("pong")), ("pong", mk("ping"))];
  let after = [("pong", mk("zing")), ("zing", mk("pong"))]; /* ping renamed to zing */
  check("the tie is detected", true, List.length(warnings(before)) == 1);
  check(
    "comphash is UNCHANGED by the rename",
    true,
    Hash.equal(comphash(before), comphash(after)),
  );
  check(
    "but position 0 moves: ping held it, pong holds it after",
    true,
    List.hd(storage_order(before)) == "ping"
    && List.hd(storage_order(after)) == "pong",
  );
};

/* (thm:ordering)(b2) And in general the component hash itself depends on the
   names. It takes a tie whose members mention the cycle asymmetrically: tied
   under the cycle frame, distinguishable once each has its own index. */
let test_ordering_b2 = () => {
  let body = (a, b) =>
    Term.Ann(
      app2(nat_plus, Term.App(Term.Var(a), Term.Nat(0)), Term.App(Term.Var(b), Term.Nat(0))),
      Ty.Nat,
    );
  let before = [("bar", body("foo", "bar")), ("foo", body("foo", "bar"))];
  let after = [("foo", body("foo", "zar")), ("zar", body("foo", "zar"))]; /* bar -> zar */
  check("the tie is detected", true, List.length(warnings(before)) == 1);
  check(
    "comphash DOES change with the names",
    false,
    Hash.equal(comphash(before), comphash(after)),
  );
};

/* (cor:detect) The side condition is decidable, and is checked: injectivity is
   decided by grouping the members by permutation hash. */
let test_detect = () => {
  let mk = other =>
    Term.Ann(Term.Lam("n", Term.App(Term.Var(other), Term.Var("n"))), nat2nat);
  switch (warnings([("ping", mk("pong")), ("pong", mk("ping"))])) {
  | [Abt.Incomplete_element_ordering(groups)] =>
    check("the structurally equivalent set is reported", true, groups == [["ping", "pong"]])
  | _ => Alcotest.fail("expected exactly one ordering warning")
  };
};

/* (rem:notpartial) hashComponent is TOTAL -- it returns a hash on the
   ambiguous input. What fails is presentation-independence, and the refusal
   belongs to the caller. Here the caller is Ingest, and it aborts. */
let test_notpartial = () => {
  let mk = other =>
    Term.Ann(Term.Lam("n", Term.App(Term.Var(other), Term.Var("n"))), nat2nat);
  let ms = [("ping", mk("pong")), ("pong", mk("ping"))];
  check("hashing still produced a hash", true, String.length(comphash(ms)) > 0);
  let st = Store.create();
  let group = [
    Ingest.definition(~name="ping", ~term=mk("pong"), ~ty=nat2nat),
    Ingest.definition(~name="pong", ~term=mk("ping"), ~ty=nat2nat),
  ];
  switch (Ingest.ingest(st, group)) {
  | Error(Ingest.Ordering_ambiguous(_)) =>
    check("and the store stayed empty", true, Store.all_hashes(st) == [])
  | Error(e) => Alcotest.fail("wrong error: " ++ Ingest.error_to_string(e))
  | Ok(_) => Alcotest.fail("ingest should have aborted")
  };
};


/* ---- store, namespace, cache ---- */

/* (def:ustore, rem:storetype) The store maps a hash to a COMPONENT, and a
   definition is a pair. A non-recursive definition is position 0 of a
   singleton; a cycle is one hash with several positions. */
let test_ustore = () => {
  let st = Store.create();
  let inc =
    Term.Lam("n", app2(nat_plus, Term.Var("n"), Term.Nat(1)));
  switch (Ingest.ingest(st, [Ingest.definition(~name="inc", ~term=inc, ~ty=nat2nat)])) {
  | Error(e) => Alcotest.fail(Ingest.error_to_string(e))
  | Ok(o) =>
    switch (o.bindings) {
    | [("inc", Reference.Derived(h, i))] =>
      check("a singleton component", true, Store.component_size(st, h) == 1);
      check("at position 0", true, i == 0);
    | _ => Alcotest.fail("expected one binding")
    }
  };
  /* a cycle: one hash, two positions */
  let f = Term.Lam("n", Term.App(Term.Var("g"), Term.Var("n")));
  let g = Term.Lam("n", Term.App(Term.Var("f"), app2(nat_plus, Term.Var("n"), Term.Nat(1))));
  switch (
    Ingest.ingest(
      st,
      [
        Ingest.definition(~name="f", ~term=f, ~ty=nat2nat),
        Ingest.definition(~name="g", ~term=g, ~ty=nat2nat),
      ],
    )
  ) {
  | Error(e) => Alcotest.fail(Ingest.error_to_string(e))
  | Ok(o) =>
    switch (o.components) {
    | [(h, ms)] =>
      check("one component holding both members", true, List.length(ms) == 2);
      check("size two", true, Store.component_size(st, h) == 2);
      let positions =
        List.map(
          ((_, r)) =>
            switch (r) {
            | Reference.Derived(_, i) => i
            | _ => (-1)
            },
          o.bindings,
        );
      check("positions 0 and 1", true, List.sort(compare, positions) == [0, 1]);
    | _ => Alcotest.fail("expected exactly one component")
    }
  };
};

/* (def:uingest) SCCs are hashed in dependency order, and references to
   already-hashed components are substituted before the next is hashed. */
let test_uingest = () => {
  let st = Store.create();
  let base = Term.Ann(Term.Nat(2), Ty.Nat);
  let user = Term.Ann(app2(nat_plus, Term.Var("base"), Term.Nat(1)), Ty.Nat);
  switch (
    Ingest.ingest(
      st,
      [
        Ingest.definition(~name="user", ~term=user, ~ty=Ty.Nat),
        Ingest.definition(~name="base", ~term=base, ~ty=Ty.Nat),
      ],
    )
  ) {
  | Error(e) => Alcotest.fail(Ingest.error_to_string(e))
  | Ok(o) =>
    check("two separate components, not one", true, List.length(o.components) == 2);
    let base_ref = List.assoc("base", o.bindings);
    let user_ref = List.assoc("user", o.bindings);
    check(
      "user's stored body references base by hash",
      true,
      switch (Store.lookup(st, user_ref)) {
      | Some(tm) => List.exists(Reference.equal(base_ref), Term.refs(tm))
      | None => false
      },
    );
    check("and base is a dependency of user", true, Store.dependents(st, base_ref) != []);
  };
};

/* (def:unames, rem:conflict) The namespace is a relation: conflict is
   representable, and it is the operations that demand its absence. */
let test_unames = () => {
  let ns = Names.create();
  let a = Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(1)]), 0));
  let b = Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(2)]), 0));
  Names.add(ns, "foo", a);
  Names.add(ns, "foo", b);
  check("one name, two referents, and that is legal", true, List.length(Names.lookup(ns, "foo")) == 2);
  check("resolution declines", true, Names.resolve(ns, "foo") == None);
  /* (def:unconf) all-or-nothing: there is no unconflicted PART of a namespace */
  Names.add(ns, "bar", a);
  check(
    "as_unconflicted fails wholesale, naming the offender",
    true,
    Names.as_unconflicted(ns) == Error("foo"),
  );
  Names.remove_name(ns, "foo");
  check("and succeeds once the conflict is gone", true, Result.is_ok(Names.as_unconflicted(ns)));
  /* one referent may have several names */
  Names.add(ns, "baz", a);
  check("aliases are ordinary", true, Names.names_of(ns, a) == ["bar", "baz"]);
};

/* Names.shadowing is the scratch buffer's whole overlay semantics: a name bound
   in the buffer wins over the same name in the codebase, so everything you are
   editing is visible to everything else you are editing with no gesture. */
let test_shadowing = () => {
  let codebase = Names.create();
  let buffer = Names.create();
  let old_ = Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(1)]), 0));
  let new_ = Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(2)]), 0));
  Names.add(codebase, "f", old_);
  Names.add(codebase, "g", old_);
  Names.add(buffer, "f", new_);
  let overlay = Names.shadowing(buffer, codebase);
  check("the buffer's f wins", true, Names.resolve(overlay, "f") == Some(new_));
  check("and the codebase's g survives", true, Names.resolve(overlay, "g") == Some(old_));
};

/* (thm:wstable, cor:wnoinval) A cache entry, once written, is valid in every
   future store. No invalidation is required -- and there is no invalidation
   function to call, which is the point. */
let test_cache = () => {
  let st = Store.create();
  let w = Watch.create();
  let x = Term.Ann(app2(nat_plus, Term.Nat(1), Term.Nat(1)), Ty.Nat);
  let r =
    switch (Ingest.ingest(st, [Ingest.definition(~name="x", ~term=x, ~ty=Ty.Nat)])) {
    | Ok(o) => List.assoc("x", o.bindings)
    | Error(e) => Alcotest.fail(Ingest.error_to_string(e))
    };
  let v1 = Eval.eval_ref(st, w, r);
  check("evaluated", true, v1 == Ok(Term.Nat(2)));
  check("and cached", true, Watch.get(w, r) == Some(Term.Nat(2)));
  /* grow the store; the entry stays sound because the key names exactly the
     expression that was evaluated, and that expression cannot have changed */
  ignore(
    Ingest.ingest(st, [Ingest.definition(~name="y", ~term=Term.Ann(Term.Nat(9), Ty.Nat), ~ty=Ty.Nat)]),
  );
  check("still sound after the store grows", true, Eval.eval_ref(st, w, r) == Ok(Term.Nat(2)));
};

/* (rem:watchkey) The cache key need not be a definition.
   A watch expression goes through the SAME hashing call as the buffer's
   definitions -- Unison's `allTerms = join tlcs ++ join (snd <$> watches)` --
   so it comes back with a real reference. What it does NOT get is an entry in
   the store ("it is not added merely by watching it without a name") or a name
   in the namespace. Only its result is kept.

   So the cache ends up holding an entry keyed by a hash that names nothing, and
   nothing anywhere has to decide whether that hash "is a definition". */
let test_watchkey = () => {
  let st = Store.create();
  let ns = Names.create();
  let w = Watch.create();
  let text = "answer : Nat\nanswer = 42\n\n> Nat.+ 2 3\n\n> answer";
  let (file, errs) = Parse.file(text);
  check("parses", true, errs == []);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Succeeded(o) =>
    check("one definition bound", true, List.length(o.bound) == 1);
    check("two watches hashed", true, List.length(o.watched) == 2);
    check("both results cached", true, Watch.size(w) == 2);
    /* the watches are hashed but NOT stored, and NOT named */
    List.iter(
      ((r, _, _)) => {
        check(
          "a watch is not in the store",
          true,
          Store.lookup(st, r) == None,
        );
        check(
          "and has no name",
          true,
          Names.names_of(ns, Reference.Ref(r)) == [],
        );
        check("but the cache has it", true, Watch.get(w, r) != None);
      },
      o.watched,
    );
    /* (prop:watchdom) the cache's domain is not contained in the store's --
       every one of those keys denotes nothing in Sigma, permanently, because an
       unnamed reference is unreachable by tdeps and unprintable by render. */
    check(
      "dom(W) is not contained in dom(Sigma)",
      true,
      List.for_all(((r, _, _)) => Store.lookup(st, r) == None, o.watched)
      && Watch.size(w) == 2,
    );
    /* the values are right, and `> answer` saw the definition committed in the
       same buffer -- watches are in the same binding group */
    let values =
      List.sort(compare, List.map(((_, _, v)) => v, o.watched));
    check("values are 42 and 5", true, values == ["42", "5"]);
  | Update.Conflicted(n) => Alcotest.fail("conflicted: " ++ n)
  | Update.Failed(f) => Alcotest.fail(f.message)
  };
};

/* A watch mentioning a definition the buffer commits in the same breath. This
   is only possible because watches are members of the binding group rather
   than a separate pass over the codebase. */
let test_watch_sees_group = () => {
  let st = Store.create();
  let ns = Names.create();
  let w = Watch.create();
  let text = "twice : Nat -> Nat\ntwice = \\n. Nat.* n 2\n\n> twice 21";
  let (file, _) = Parse.file(text);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Succeeded(o) =>
    switch (o.watched) {
    | [(_, ty, v)] =>
      check("the watch saw the uncommitted definition", true, v == "42");
      check("and synthesized its type", true, Ty.equal(ty, Ty.Nat));
    | _ => Alcotest.fail("expected exactly one watch")
    }
  | _ => Alcotest.fail("should have succeeded")
  };
};

/* ---- resolution ---- */

/* (def:uresolution, rem:staging) Resolution is deliberately partial. Three of
   the four cases leave a variable in place. */
let test_uresolution = () => {
  let ns = Names.create();
  let r = Reference.Derived(Hash.acc([Hash.Nat(1)]), 0);
  Names.add(ns, "known", Reference.Ref(r));
  Names.add(ns, "ambiguous", Reference.Ref(r));
  Names.add(ns, "ambiguous", Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(2)]), 0)));
  let res = e => Resolve.term(~group=["mine"], ~ns, e);
  check("a group member stays a variable", true, res(Term.Source.Var("mine")) == Term.Var("mine"));
  check("a known name becomes a reference", true, res(Term.Source.Var("known")) == Term.Ref(r));
  check("an unknown name stays free", true, res(Term.Source.Var("nope")) == Term.Var("nope"));
  check(
    "an ambiguous name stays free, on purpose",
    true,
    res(Term.Source.Var("ambiguous")) == Term.Var("ambiguous"),
  );
  /* ... and a free variable is what turns into a type error, with a type to
     report, rather than a resolution error */
  let env = Store.build_env(Store.create());
  check(
    "the free variable fails at typechecking",
    true,
    Result.is_error(Typecheck.synth_top(env, res(Term.Source.Var("nope")))),
  );
};


/* ---- update ---- */

/* A fixture: `scale` and a `double` that calls it, both bound. */
let scale_and_double = () => {
  let st = Store.create();
  let ns = Names.create();
  let w = Watch.create();
  let text = "scale : Nat -> Nat\nscale = \\n. Nat.* n 2\n\ndouble : Nat -> Nat\ndouble = \\n. scale n";
  let (file, errs) = Parse.file(text);
  check("the fixture parses", true, errs == []);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Succeeded(_) => ()
  | Update.Conflicted(n) => Alcotest.fail("conflicted: " ++ n)
  | Update.Failed(f) => Alcotest.fail("setup failed: " ++ f.message)
  };
  (st, ns, w);
};

/* (ex:update-ok, def:update steps 1-4) An update that succeeds. The dependent
   is rebuilt: its source text is character-for-character what it was, and its
   hash changes anyway, because the printed NAME resolves against the new
   namespace. */
let test_update_ok = () => {
  let (st, ns, w) = scale_and_double();
  let before = Names.resolve(ns, "double");
  let text = "scale : Nat -> Nat\nscale = \\n. Nat.* n 3";
  let (file, _) = Parse.file(text);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Succeeded(o) =>
    check("double was rebuilt", true, List.mem("double", o.rebuilt));
    check("both names are still bound", true, Names.resolve(ns, "scale") != None && Names.resolve(ns, "double") != None);
    check("and double now denotes a different definition", false, Names.resolve(ns, "double") == before);
    /* the old definition is untouched in the store, so anything cached for it
       remains valid (cor:wnoinval) */
    switch (before) {
    | Some(Reference.Ref(r)) => check("the old double is still in the store", true, Store.lookup(st, r) != None)
    | _ => Alcotest.fail("no prior double")
    };
    /* and it means what it meant: 5 doubled under the old scale is still 10 */
    let w = Watch.create();
    switch (before) {
    | Some(Reference.Ref(r)) =>
      switch (Store.lookup(st, r), Eval.eval_top(st, Term.App(Term.Ref(r), Term.Nat(5)))) {
      | (Some(_), Ok(v)) => check("old double 5 = 10", true, Eval.to_term(v) == Term.Nat(10))
      | _ => Alcotest.fail("could not evaluate the old double")
      }
    | _ => ()
    };
    ignore(w);
  | Update.Conflicted(n) => Alcotest.fail("conflicted: " ++ n)
  | Update.Failed(f) => Alcotest.fail("should have succeeded: " ++ f.message)
  };
};

/* (ex:update-fail, def:update step 5, rem:step5) An update that fails.
   NOTHING is ingested; BOTH names leave the namespace -- not just the broken
   dependent, and not just the definitions that failed -- and what comes back
   is F u S, the user's own file plus the dependents it broke, under a banner.

   This is the correction the audit produced: the paper originally showed
   `scale` rebound to a new hash. */
let test_update_fail = () => {
  let (st, ns, w) = scale_and_double();
  let hashes_before = List.length(Store.all_hashes(st));
  let text = "scale : Text -> Nat\nscale = \\s. Text.size s";
  let (file, _) = Parse.file(text);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Failed(f) =>
    check("double is evicted", true, Names.resolve(ns, "double") == None);
    check("and so is scale, the name the user was editing", true, Names.resolve(ns, "scale") == None);
    check("the namespace is empty", true, Names.names(ns) == []);
    check("nothing was ingested", true, List.length(Store.all_hashes(st)) == hashes_before);
    check("the residual carries the banner", true, String.length(f.buffer) > String.length(text));
    check("and the user's own file is in it", true, f.evicted != []);
    /* the residual is code: it parses */
    let (rf, rerrs) = Parse.file(f.buffer);
    check("the residual parses back", true, rerrs == []);
    check("and holds both definitions", true, List.length(fst(Slurp.entries(rf))) == 2);
  | Update.Succeeded(_) => Alcotest.fail("should have failed to typecheck")
  | Update.Conflicted(n) => Alcotest.fail("conflicted: " ++ n)
  };
};

/* (rem:conflict) update cannot proceed against a conflicted namespace, and
   asserts unconflictedness before doing anything. A representable state that
   the operations refuse is the price of the relation. */
let test_update_conflicted = () => {
  let (st, ns, w) = scale_and_double();
  Names.add(ns, "scale", Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(99)]), 0)));
  let text = "other : Nat\nother = 1";
  let (file, _) = Parse.file(text);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Conflicted(n) => check("it names the conflicted name", true, n == "scale")
  | _ => Alcotest.fail("should have refused")
  };
};

/* (def:uscope, rem:scopenaming) Naming is what puts a definition in scope. An
   unnamed definition is not a dependent to be updated, even though it does
   reference the target. */
let test_uscope_naming = () => {
  let (st, ns, w) = scale_and_double();
  /* unbind double; it still references scale, but it is now out of scope */
  Names.remove_name(ns, "double");
  let text = "scale : Nat -> Nat\nscale = \\n. Nat.* n 5";
  let (file, _) = Parse.file(text);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Succeeded(o) =>
    check("the unnamed dependent was not rebuilt", true, o.rebuilt == [])
  | Update.Conflicted(n) => Alcotest.fail("conflicted: " ++ n)
  | Update.Failed(f) => Alcotest.fail("should have succeeded: " ++ f.message)
  };
};

/* (prop:follow-only) There is one strategy and no parameter controlling it.
   The nearest thing to pinning is to choose a name nothing depends on, so that
   tdeps(Q) is empty -- abstention, not an operation. */
let test_follow_only = () => {
  let (st, ns, w) = scale_and_double();
  let text = "fresh : Nat\nfresh = 1";
  let (file, _) = Parse.file(text);
  switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
  | Update.Succeeded(o) =>
    check("a fresh name propagates to nothing", true, o.rebuilt == []);
    check("and the existing bindings are untouched", true, Names.resolve(ns, "double") != None);
  | _ => Alcotest.fail("should have succeeded")
  };
};

/* ---- the round trip ---- */

/* (prop:print-parse) update is correct only if printing inverts parsing. This
   is the obligation Unison does not state and maintains with a golden-file
   corpus named `reparses-with-same-hash.u`. Over a language this small it can
   be a property instead. */

let gen_term: QCheck.Gen.t((Term.Source.t, Ty.t)) = {
  open QCheck.Gen;
  let rec nat_expr = (vars, n) =>
    if (n <= 0) {
      oneof(
        [map(k => Term.Source.Nat(k), int_range(0, 9))]
        @ (vars == [] ? [] : [map(v => Term.Source.Var(v), oneof_list(vars))]),
      );
    } else {
      oneof([
        map(k => Term.Source.Nat(k), int_range(0, 9)),
        map2(
          (a, b) =>
            Term.Source.App(
              Term.Source.App(Term.Source.Var("Nat.+"), a),
              b,
            ),
          nat_expr(vars, n - 1),
          nat_expr(vars, n - 1),
        ),
        map2(
          (a, b) =>
            Term.Source.App(
              Term.Source.App(Term.Source.Var("Nat.*"), a),
              b,
            ),
          nat_expr(vars, n - 1),
          nat_expr(vars, n - 1),
        ),
        map(
          s => Term.Source.App(Term.Source.Var("Text.size"), s),
          text_expr(vars, n - 1),
        ),
      ]);
    }
  and text_expr = (vars, n) =>
    if (n <= 0) {
      oneof_list([Term.Source.Text("a"), Term.Source.Text("hi"), Term.Source.Text("")]);
    } else {
      oneof([
        oneof_list([Term.Source.Text("a"), Term.Source.Text("hi")]),
        map2(
          (a, b) =>
            Term.Source.App(Term.Source.App(Term.Source.Var("Text.++"), a), b),
          text_expr(vars, n - 1),
          text_expr(vars, n - 1),
        ),
      ]);
    };
  /* Either a Nat, or a Nat -> Nat with one binder. */
  oneof([
    map(e => (e, Ty.Nat), nat_expr([], 3)),
    map(
      e => (Term.Source.Lam("n", e), Ty.Arrow(Ty.Nat, Ty.Nat)),
      nat_expr(["n"], 3),
    ),
    map(e => (e, Ty.Text), text_expr([], 3)),
  ]);
};

let rec show_src = (e: Term.Source.t): string =>
  switch (e) {
  | Term.Source.Var(x) => x
  | Term.Source.Nat(n) => string_of_int(n)
  | Term.Source.Text(s) => "\"" ++ String.escaped(s) ++ "\""
  | Term.Source.Lam(x, b) => "(\\" ++ x ++ ". " ++ show_src(b) ++ ")"
  | Term.Source.App(f, a) => "(" ++ show_src(f) ++ " " ++ show_src(a) ++ ")"
  | Term.Source.Ann(e, t) => "(" ++ show_src(e) ++ " : " ++ Ty.to_string(t) ++ ")"
  };

let prop_print_parse =
  QCheck.Test.make(
    ~count=500,
    ~name="print-then-parse preserves the hash (prop:print-parse)",
    QCheck.make(~print=((e, t)) => show_src(e) ++ " : " ++ Ty.to_string(t), gen_term),
    ((src, ty)) => {
      let st = Store.create();
      let ns = Names.create();
      switch (
        Ingest.ingest(st, [Ingest.definition(~name="d", ~term=Resolve.term(~ns, src), ~ty=ty)])
      ) {
      /* the generator can produce an ill-typed term; that is not this
         property's business */
      | Error(_) => true
      | Ok(o) =>
        let r = List.assoc("d", o.bindings);
        Names.add(ns, "d", Reference.Ref(r));
        switch (Store.lookup(st, r), Store.type_of(st, r)) {
        | (Some(tm), Some(t)) =>
          let text = Pretty.definition(Ppe.make(ns), "d", tm, t);
          let (file, errs) = Parse.file(text);
          errs == []
          && (
            switch (Slurp.entries(file)) {
            | ([(_, ty2, src2)], []) =>
              let st2 = Store.create();
              let ns2 = Names.create();
              switch (
                Ingest.ingest(
                  st2,
                  [Ingest.definition(~name="d", ~term=Resolve.term(~ns=ns2, src2), ~ty=ty2)],
                )
              ) {
              | Ok(o2) => Reference.equal(List.assoc("d", o2.bindings), r)
              | Error(_) => false
              };
            | _ => false
            }
          );
        | _ => false
        };
      };
    },
  );


/* ---- resolution decisions, and the scratch state ---- */

/* (def:uresolution, rem:staging) Resolution takes one of five routes per
   occurrence, and THREE OF THEM PRODUCE THE SAME OUTPUT NODE. That is the whole
   reason the trace exists: `Var(x)` alone cannot tell you whether x is a group
   member awaiting its hash, a name that resolves to nothing, or a name that
   resolves to too many things. */
let test_resolve_trace = () => {
  let ns = Names.create();
  let r = Reference.Derived(Hash.acc([Hash.Nat(1)]), 0);
  Names.add(ns, "known", Reference.Ref(r));
  Names.add(ns, "amb", Reference.Ref(r));
  Names.add(ns, "amb", Reference.Ref(Reference.Derived(Hash.acc([Hash.Nat(2)]), 0)));
  /* \n. Nat.+ (mine n) (known (amb (nope 0))) */
  let src =
    Term.Source.Lam(
      "n",
      Term.Source.App(
        Term.Source.App(
          Term.Source.Var("Nat.+"),
          Term.Source.App(Term.Source.Var("mine"), Term.Source.Var("n")),
        ),
        Term.Source.App(
          Term.Source.Var("known"),
          Term.Source.App(
            Term.Source.Var("amb"),
            Term.Source.App(Term.Source.Var("nope"), Term.Source.Nat(0)),
          ),
        ),
      ),
    );
  let (tm, decisions) = Resolve.term_traced(~group=["mine"], ~ns, src);
  let outcome_for = n =>
    switch (List.find_opt((d: Resolve.decision) => d.name == n, decisions)) {
    | Some(d) => Some(d.outcome)
    | None => None
    };
  check("local binder", true, outcome_for("n") == Some(Resolve.Local));
  check("group member", true, outcome_for("mine") == Some(Resolve.Group));
  check("builtin", true, outcome_for("Nat.+") == Some(Resolve.Builtin("Nat.+")));
  check("resolved", true, outcome_for("known") == Some(Resolve.Resolved(r)));
  check("not found", true, outcome_for("nope") == Some(Resolve.Unresolved));
  check("ambiguous", true, outcome_for("amb") == Some(Resolve.Ambiguous(2)));
  /* the three that stay free are indistinguishable in the term */
  let free = Term.free_vars(tm);
  check(
    "three distinct outcomes, one indistinguishable node shape",
    true,
    List.mem("mine", free) && List.mem("nope", free) && List.mem("amb", free),
  );
  check(
    "and the trace is what tells them apart",
    true,
    List.length(
      List.filter(
        (d: Resolve.decision) => Resolve.stays_free(d.outcome),
        decisions,
      ),
    )
    == 3,
  );
  /* untraced resolution agrees with traced */
  check(
    "term and term_traced agree",
    true,
    Resolve.term(~group=["mine"], ~ns, src) == tm,
  );
};

/* The scratch state is a cache, not a store: it holds no hashes, it is
   recomputable from the text, and analyzing a buffer mutates nothing. */
let test_scratch_holds_no_hashes = () => {
  let st = Store.create();
  let ns = Names.create();
  let text = "f : Nat -> Nat\nf = \\n. Nat.+ (g n) 1\n\ng : Nat -> Nat\ng = \\n. f n";
  let before = List.length(Store.all_hashes(st));
  let sc = Scratch.analyze(~st, ~ns, text);
  check("analysis stored nothing", true, List.length(Store.all_hashes(st)) == before);
  check("and bound nothing", true, Names.names(ns) == []);
  check("the buffer typechecks", true, sc.stage == Scratch.Typechecked);
  check("committable", true, Scratch.is_committable(sc));
  /* the SCC preview shows what ingest WOULD do, before it does it */
  switch (sc.components) {
  | [c] =>
    check("one cycle", true, c.recursive);
    check("of both members", true, List.sort(compare, c.members) == ["f", "g"]);
  | cs => Alcotest.fail("expected one component, got " ++ string_of_int(List.length(cs)))
  };
  /* recomputable: same text, same answer */
  let sc2 = Scratch.analyze(~st, ~ns, text);
  check("deterministic", true, sc2.stage == sc.stage);
};

/* The pipeline stops at the first stage that fails, which is what the stage rail
   in the UI is reading. */
let test_scratch_stages = () => {
  let st = Store.create();
  let ns = Names.create();
  let at = t => Scratch.analyze(~st, ~ns, t).stage;
  check("unparsed", true, at("f = \\n.") == Scratch.Unparsed);
  check("parsed but no signature", true, at("f = 1") == Scratch.Parsed);
  check(
    "resolved but ill-typed",
    true,
    at("f : Nat\nf = nope 1") == Scratch.Resolved,
  );
  check("typechecked", true, at("f : Nat\nf = 1") == Scratch.Typechecked);
};


/* ---- save is not commit ---- */

/* Unison's save path (`loadUnisonFile`) typechecks, hashes the whole file
   including its watches, evaluates them against a code lookup over the file,
   and writes each result to the cache -- `only when (not isHit)` -- with no
   `addDefsToCodebase` anywhere. So a save confers identity and fills the cache;
   it stores and names nothing. */
let test_save_is_not_commit = () => {
  let st = Store.create();
  let ns = Names.create();
  let w = Watch.create();
  let text = "twice : Nat -> Nat\ntwice = \\n. Nat.* n 2\n\n> twice 21";
  let sc = Scratch.save(~st, ~ns, ~w, text);
  check("the save hashed", true, sc.stage == Scratch.Hashed);
  /* identity is conferred, by typechecking */
  switch (sc.bindings) {
  | [b] => check("the definition has a reference", true, b.reference != None)
  | _ => Alcotest.fail("expected one binding")
  };
  /* ... and yet nothing is stored and nothing is named */
  check("nothing stored", true, Store.all_hashes(st) == []);
  check("nothing named", true, Names.names(ns) == []);
  /* the watch ran, against an overlay carrying the uncommitted definition */
  switch (sc.watches) {
  | [wa] =>
    check("the watch has a reference", true, wa.reference != None);
    check("it evaluated", false, wa.cache_hit);
    check("over an uncommitted definition", true, wa.value == Some(Term.Nat(42)));
  | _ => Alcotest.fail("expected one watch")
  };
  check("and the result is cached", true, Watch.size(w) == 1);
};

/* The observable the UI shows: re-saving does not re-evaluate, and reverting an
   edit hits an entry that was never invalidated (cor:wnoinval). */
let test_save_does_not_reevaluate = () => {
  let st = Store.create();
  let ns = Names.create();
  let w = Watch.create();
  let a = "n : Nat\nn = 1\n\n> Nat.* 7 6";
  let b = "n : Nat\nn = 1\n-- an unrelated comment\n\n> Nat.* 7 6";
  let c = "n : Nat\nn = 1\n\n> Nat.* 7 7";

  let s1 = Scratch.save(~st, ~ns, ~w, a);
  check("first save evaluates", true, s1.evaluated == 1);
  check("and misses", false, List.hd(s1.watches).cache_hit);

  let s2 = Scratch.save(~st, ~ns, ~w, b);
  check("an unrelated edit evaluates nothing", true, s2.evaluated == 0);
  check("the watch hits", true, List.hd(s2.watches).cache_hit);
  check(
    "because its reference is unchanged",
    true,
    List.hd(s2.watches).reference == List.hd(s1.watches).reference,
  );

  let s3 = Scratch.save(~st, ~ns, ~w, c);
  check("changing the watch evaluates", true, s3.evaluated == 1);
  check(
    "under a different reference",
    false,
    List.hd(s3.watches).reference == List.hd(s1.watches).reference,
  );

  /* revert: the first entry is still there. Nothing invalidated it, and nothing
     could have -- the store only grows and the key is the expression. */
  let s4 = Scratch.save(~st, ~ns, ~w, a);
  check("reverting hits", true, List.hd(s4.watches).cache_hit);
  check("and evaluates nothing", true, s4.evaluated == 0);
  check("both entries survive", true, Watch.size(w) == 2);
  check("still nothing stored", true, Store.all_hashes(st) == []);
};

/* Saving and committing produce the SAME hashes -- `~register` is the only
   difference between them. If this failed, "identity is conferred at save"
   would be a lie: the commit would be re-deciding it. */
let test_save_and_commit_agree = () => {
  let text = "twice : Nat -> Nat\ntwice = \\n. Nat.* n 2";
  let saved = {
    let st = Store.create();
    let ns = Names.create();
    let w = Watch.create();
    let sc = Scratch.save(~st, ~ns, ~w, text);
    List.hd(sc.bindings).reference;
  };
  let committed = {
    let st = Store.create();
    let ns = Names.create();
    let w = Watch.create();
    let (file, _) = Parse.file(text);
    switch (Update.run(~st, ~ns, ~w, ~text, ~file)) {
    | Update.Succeeded(o) => Some(List.assoc("twice", o.bound))
    | _ => None
    };
  };
  check("the save's hash is the commit's hash", true, saved == committed);
  check("and it is a real reference", true, saved != None);
};

/* Analysis alone touches nothing -- the distinction between analyze and save is
   itself worth a test, since only one of them writes. */
let test_analyze_is_inert = () => {
  let st = Store.create();
  let ns = Names.create();
  let w = Watch.create();
  let text = "n : Nat\nn = 1\n\n> Nat.* 9 9";
  let sc = Scratch.analyze(~st, ~ns, text);
  check("analysis stops at typechecked", true, sc.stage == Scratch.Typechecked);
  check("confers no identity", true, List.hd(sc.bindings).reference == None);
  check("and writes no cache entry", true, Watch.size(w) == 0);
};

let () =
  Alcotest.run(
    "p21 substrate",
    [
      (
        "hashing",
        [
          Alcotest.test_case("determinism", `Quick, test_hash_determinism),
          Alcotest.test_case("the token stream (def:tokens)", `Quick, test_tokens),
          Alcotest.test_case("alpha-equivalence and no further (thm:ualpha)", `Quick, test_ualpha),
          Alcotest.test_case("transparency at position 0 (prop:transparent)", `Quick, test_transparent),
          Alcotest.test_case("transparency is narrower than usually said (rem:transparency-limit)", `Quick, test_transparency_limit),
          Alcotest.test_case("the type is inside the hash (def:typeinhash)", `Quick, test_typeinhash),
        ],
      ),
      (
        "components",
        [
          Alcotest.test_case("two orderings, not one (def:comphash, rem:twoorders)", `Quick, test_twoorders),
          Alcotest.test_case("presentation-independence, when injective (thm:ordering a)", `Quick, test_ordering_a),
          Alcotest.test_case("a tie makes Pos name-dependent (thm:ordering b1)", `Quick, test_ordering_b1),
          Alcotest.test_case("and usually the hash too (thm:ordering b2)", `Quick, test_ordering_b2),
          Alcotest.test_case("the condition is decidable and checked (cor:detect)", `Quick, test_detect),
          Alcotest.test_case("total function, aborting caller (rem:notpartial)", `Quick, test_notpartial),
        ],
      ),
      (
        "store",
        [
          Alcotest.test_case("hash to component, definition to pair (def:ustore, rem:storetype)", `Quick, test_ustore),
          Alcotest.test_case("SCCs in dependency order (def:uingest)", `Quick, test_uingest),
        ],
      ),
      (
        "namespace",
        [
          Alcotest.test_case("a relation, conflict representable (def:unames, rem:conflict, def:unconf)", `Quick, test_unames),
          Alcotest.test_case("the buffer shadows the codebase", `Quick, test_shadowing),
          Alcotest.test_case("resolution is deliberately partial (def:uresolution, rem:staging)", `Quick, test_uresolution),
        ],
      ),
      (
        "cache",
        [
          Alcotest.test_case("sound under store growth (thm:wstable, cor:wnoinval)", `Quick, test_cache),
          Alcotest.test_case("the key need not be a definition (rem:watchkey)", `Quick, test_watchkey),
          Alcotest.test_case("a watch is in the same binding group", `Quick, test_watch_sees_group),
        ],
      ),
      (
        "update",
        [
          Alcotest.test_case("an update that succeeds (ex:update-ok)", `Quick, test_update_ok),
          Alcotest.test_case("an update that fails, and what it evicts (ex:update-fail, rem:step5, rem:residual)", `Quick, test_update_fail),
          Alcotest.test_case("it refuses a conflicted namespace (rem:conflict)", `Quick, test_update_conflicted),
          Alcotest.test_case("naming is what puts a definition in scope (def:uscope, rem:scopenaming)", `Quick, test_uscope_naming),
          Alcotest.test_case("one strategy, no parameter (prop:follow-only)", `Quick, test_follow_only),
        ],
      ),
      (
        "resolution",
        [
          Alcotest.test_case("five routes, three indistinguishable (def:uresolution, rem:staging)", `Quick, test_resolve_trace),
        ],
      ),
      (
        "scratch",
        [
          Alcotest.test_case("holds no hashes and mutates nothing", `Quick, test_scratch_holds_no_hashes),
          Alcotest.test_case("stops at the first failing stage", `Quick, test_scratch_stages),
          Alcotest.test_case("analysis alone is inert", `Quick, test_analyze_is_inert),
        ],
      ),
      (
        "save",
        [
          Alcotest.test_case("a save confers identity but stores and names nothing", `Quick, test_save_is_not_commit),
          Alcotest.test_case("re-saving does not re-evaluate (cor:wnoinval)", `Quick, test_save_does_not_reevaluate),
          Alcotest.test_case("save and commit agree on the hash", `Quick, test_save_and_commit_agree),
        ],
      ),
      ("round-trip", [QCheck_alcotest.to_alcotest(prop_print_parse)]),
    ],
  );
