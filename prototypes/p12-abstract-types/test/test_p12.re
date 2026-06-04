/* p12 substrate tests: primitive layer (Hash, Mint) + the design/12 Counter
   worked example built through the editing context. */

/* ---- primitives ---- */

let test_hash_determinism = () => {
  let h1 = Hash.digest_string("hello");
  let h2 = Hash.digest_string("hello");
  let h3 = Hash.digest_string("world");
  Alcotest.(check(bool))("same bytes hash equal", true, Hash.equal(h1, h2));
  Alcotest.(check(bool))("different bytes differ", false, Hash.equal(h1, h3));
};

let test_mint_distinct = () => {
  let src = Mint.make_source();
  let m1 = Mint.fresh(src);
  let m2 = Mint.fresh(src);
  Alcotest.(check(bool))("fresh mints differ", false, Mint.equal(m1, m2));
};

let test_mint_reproducible = () => {
  let a = Mint.make_source();
  let b = Mint.make_source();
  let a0 = Mint.fresh(a);
  let _ = Mint.fresh(a);
  let b0 = Mint.fresh(b);
  Alcotest.(check(bool))("first draw matches across sources", true, Mint.equal(a0, b0));
};

/* ---- helpers ---- */

let unwrap = r =>
  switch (r) {
  | Ok(v) => v
  | Error(m) => Alcotest.fail("unexpected error: " ++ m)
  };

let is_error = r =>
  switch (r) {
  | Error(_) => true
  | Ok(_) => false
  };

let is_sealed = (k: Editing_context.kind) =>
  switch (k) {
  | `Sealed => true
  | `Normal => false
  };

/* ---- the Counter worked example ---- */

let test_counter = () => {
  let st = Store.create();
  let int_h = Store.int_type(st);
  let bool_h = Store.bool_type(st);

  /* distinctness + witness-in-hash on the opaque type */
  let ms = Mint.make_source();
  let m1 = Mint.fresh(ms);
  let m2 = Mint.fresh(ms);
  let counter_t = Store.ingest_type(st, Tnode.Opaque({mint: m1, witness: int_h}));
  let counter_t' = Store.ingest_type(st, Tnode.Opaque({mint: m1, witness: int_h}));
  let celsius_t = Store.ingest_type(st, Tnode.Opaque({mint: m2, witness: int_h}));
  Alcotest.(check(bool))(
    "same mint+witness -> same hash",
    true,
    Hash.equal(counter_t, counter_t'),
  );
  Alcotest.(check(bool))(
    "Counter != Celsius over same witness (mint distinctness)",
    false,
    Hash.equal(counter_t, celsius_t),
  );
  let pair_h = Store.ingest_type(st, Tnode.Product(int_h, int_h));
  let counter_pair = Store.ingest_type(st, Tnode.Opaque({mint: m1, witness: pair_h}));
  Alcotest.(check(bool))(
    "witness in hash: representation change moves the type",
    false,
    Hash.equal(counter_t, counter_pair),
  );

  /* opening context: author the ops implicitly opened */
  let ctx = Editing_context.make();
  Editing_context.open_type(ctx, counter_t);
  let arrow_tt = Store.ingest_type(st, Tnode.Arrow(counter_t, counter_t));
  let arrow_tint = Store.ingest_type(st, Tnode.Arrow(counter_t, int_h));

  let commit = (term, ann) => Editing_context.commit(st, ctx, ~term, ~ann);
  let tyof = h =>
    switch (Store.type_of(st, h)) {
    | Some(t) => t
    | None => Alcotest.fail("no Type_of")
    };

  /* empty = 0 : t */
  let (empty_h, empty_k) = unwrap(commit(Node.Lit(0), counter_t));
  /* incr = \x:t. x + 1 : t -> t */
  let incr_body = Node.Prim(Node.Add, [Node.Var(0), Node.Lit(1)]);
  let (incr_h, incr_k) = unwrap(commit(Node.Lam(counter_t, incr_body), arrow_tt));
  /* get = \x:t. x : t -> Int */
  let (get_h, get_k) = unwrap(commit(Node.Lam(counter_t, Node.Var(0)), arrow_tint));
  /* decr = \x:t. x - 1 : t -> t */
  let decr_body = Node.Prim(Node.Sub, [Node.Var(0), Node.Lit(1)]);
  let (decr_h, _) = unwrap(commit(Node.Lam(counter_t, decr_body), arrow_tt));

  Alcotest.(check(bool))("empty sealed", true, is_sealed(empty_k));
  Alcotest.(check(bool))("incr sealed", true, is_sealed(incr_k));
  Alcotest.(check(bool))("get sealed", true, is_sealed(get_k));
  Alcotest.(check(bool))("empty : t", true, Hash.equal(tyof(empty_h), counter_t));
  Alcotest.(check(bool))("incr : t -> t", true, Hash.equal(tyof(incr_h), arrow_tt));
  Alcotest.(check(bool))("get : t -> Int", true, Hash.equal(tyof(get_h), arrow_tint));

  /* raw-body sharing: incr's normalized impl == \x:Int. x+1 (Math.inc) */
  let math_inc =
    Node.Lam(int_h, Node.Prim(Node.Add, [Node.Var(0), Node.Lit(1)]));
  let math_inc_h = unwrap(Store.ingest_term(st, math_inc));
  let incr_impl_h =
    switch (Store.find(st, incr_h)) {
    | Some(Definition.Term(Node.Seal({impl, _}))) => impl
    | _ => Alcotest.fail("incr is not a seal")
    };
  Alcotest.(check(bool))(
    "sealed incr's raw body is shared with Math.inc",
    true,
    Hash.equal(incr_impl_h, math_inc_h),
  );

  /* consumer in the DEFAULT context: bump2 = \c:t. incr (incr c) -> ordinary term */
  let ctx0 = Editing_context.make();
  let bump2_term =
    Node.Lam(
      counter_t,
      Node.App(Node.Ref(incr_h), Node.App(Node.Ref(incr_h), Node.Var(0))),
    );
  let (bump2_h, bump2_k) =
    unwrap(Editing_context.commit(st, ctx0, ~term=bump2_term, ~ann=arrow_tt));
  Alcotest.(check(bool))("bump2 is a normal term, not sealed", false, is_sealed(bump2_k));
  Alcotest.(check(bool))("bump2 : t -> t", true, Hash.equal(tyof(bump2_h), arrow_tt));

  /* opacity: a default-context consumer cannot touch the representation */
  let bad =
    Editing_context.commit(
      st,
      ctx0,
      ~term=Node.Lam(counter_t, Node.Prim(Node.Add, [Node.Var(0), Node.Lit(1)])),
      ~ann=arrow_tt,
    );
  Alcotest.(check(bool))("consumer cannot open the abstract type", true, is_error(bad));

  /* a hand-written bogus seal (impl Int->Int as t->Bool) is rejected at ingest */
  let arrow_tbool = Store.ingest_type(st, Tnode.Arrow(counter_t, bool_h));
  let bogus =
    Store.ingest_term(
      st,
      Node.Seal({opens: [counter_t], ty: arrow_tbool, impl: incr_impl_h}),
    );
  Alcotest.(check(bool))("bogus seal rejected at ingest", true, is_error(bogus));

  /* implementation set is derived: empty, incr, get, decr (not bump2/Math.inc) */
  let iset = Store.impl_set(st, counter_t);
  Alcotest.(check(int))("implementation set has 4 sealed ops", 4, List.length(iset));

  /* criterion 4: editing decr leaves bump2 byte-identical */
  let decr2_body = Node.Prim(Node.Sub, [Node.Var(0), Node.Lit(2)]);
  let (decr2_h, _) = unwrap(commit(Node.Lam(counter_t, decr2_body), arrow_tt));
  Alcotest.(check(bool))("decr edit changes decr's hash", false, Hash.equal(decr_h, decr2_h));
  let bump2_again = Definition.hash(Definition.Term(bump2_term));
  Alcotest.(check(bool))(
    "bump2 byte-identical across the decr edit",
    true,
    Hash.equal(bump2_h, bump2_again),
  );
};

let () =
  Alcotest.run(
    "p12",
    [
      (
        "primitives",
        [
          Alcotest.test_case("hash determinism", `Quick, test_hash_determinism),
          Alcotest.test_case("mint distinctness", `Quick, test_mint_distinct),
          Alcotest.test_case("mint reproducible", `Quick, test_mint_reproducible),
        ],
      ),
      (
        "abstract-types",
        [Alcotest.test_case("counter worked example", `Quick, test_counter)],
      ),
    ],
  );
