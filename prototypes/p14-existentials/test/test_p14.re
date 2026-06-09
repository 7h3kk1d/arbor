/* p14 substrate tests: primitive layer (Hash, Mint) + the design/12 Counter
   worked example built through the editing context. */

open P14_substrate;

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

let tyof_in = (st, h) =>
  switch (Store.type_of(st, h)) {
  | Some(t) => t
  | None => Alcotest.fail("no Type_of")
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

  /* unsealing set is derived: empty, incr, get, decr (not bump2/Math.inc) */
  let iset = Store.unsealers(st, counter_t);
  Alcotest.(check(int))("4 definitions unseal the type", 4, List.length(iset));

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

  /* evaluation: abstraction is erased — abstract values reduce to their rep */
  let assert_int = (msg, expected, node) =>
    switch (Eval.eval_top(st, node)) {
    | Ok(Eval.VInt(n)) => Alcotest.(check(int))(msg, expected, n)
    | Ok(_) => Alcotest.fail(msg ++ ": not an int")
    | Error(m) => Alcotest.fail(msg ++ ": stuck: " ++ m)
    };
  let app = (f, x) => Node.App(Node.Ref(f), x);
  assert_int("incr empty = 1", 1, app(incr_h, Node.Ref(empty_h)));
  assert_int("bump2 empty = 2", 2, app(bump2_h, Node.Ref(empty_h)));
  /* get (incr (incr empty)) = 2 — sealed ops compose and erase to Int */
  let two = app(incr_h, app(incr_h, Node.Ref(empty_h)));
  assert_int("get (incr (incr empty)) = 2", 2, app(get_h, two));
};

/* Representation change with a FRESH mint (no edit-of / mint-reuse). Soundness
   rides witness-in-hash, not the mark: a value of the old representation cannot
   feed an op of the new representation. */
let test_rep_change_soundness = () => {
  let st = Store.create();
  let int_h = Store.int_type(st);
  let ms = Mint.make_source();

  /* Counter v1 over Int */
  let m1 = Mint.fresh(ms);
  let c1 = Store.ingest_type(st, Tnode.Opaque({mint: m1, witness: int_h}));
  let ctx1 = Editing_context.make();
  Editing_context.open_type(ctx1, c1);
  let (empty1, _) =
    unwrap(Editing_context.commit(st, ctx1, ~term=Node.Lit(0), ~ann=c1));

  /* Counter v2: representation now Int * Int, fresh mint — a distinct type */
  let pair_h = Store.ingest_type(st, Tnode.Product(int_h, int_h));
  let m2 = Mint.fresh(ms);
  let c2 = Store.ingest_type(st, Tnode.Opaque({mint: m2, witness: pair_h}));
  Alcotest.(check(bool))(
    "representation change yields a distinct type",
    false,
    Hash.equal(c1, c2),
  );

  let ctx2 = Editing_context.make();
  Editing_context.open_type(ctx2, c2);
  let arrow_c2c2 = Store.ingest_type(st, Tnode.Arrow(c2, c2));
  /* incr2 : c2 -> c2 = \x. (fst x + 1, snd x) */
  let incr2_body =
    Node.Pair(
      Node.Prim(Node.Add, [Node.Fst(Node.Var(0)), Node.Lit(1)]),
      Node.Snd(Node.Var(0)),
    );
  let (incr2, incr2_k) =
    unwrap(Editing_context.commit(st, ctx2, ~term=Node.Lam(c2, incr2_body), ~ann=arrow_c2c2));
  let (empty2, _) =
    unwrap(Editing_context.commit(st, ctx2, ~term=Node.Pair(Node.Lit(0), Node.Lit(0)), ~ann=c2));
  Alcotest.(check(bool))("incr2 over the new rep is sealed", true, is_sealed(incr2_k));

  let env = Store.build_env(st);
  /* soundness: a v1 value must NOT type-check against a v2 op */
  switch (Typecheck.synth_top(env, [], Node.App(Node.Ref(incr2), Node.Ref(empty1)))) {
  | Error(_) =>
    Alcotest.(check(bool))("old-rep value rejected by new-rep op", true, true)
  | Ok(_) =>
    Alcotest.fail("UNSOUND: a v1 Counter value type-checked against a v2 op")
  };
  /* and the new op works on a new-rep value */
  switch (Eval.eval_top(st, Node.App(Node.Ref(incr2), Node.Ref(empty2)))) {
  | Ok(Eval.VPair(Eval.VInt(1), Eval.VInt(0))) =>
    Alcotest.(check(bool))("incr2 empty2 = (1, 0)", true, true)
  | Ok(v) => Alcotest.fail("unexpected value: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("stuck: " ++ m)
  };
};

/* An operation that opens *two* abstract types at once (Celsius -> Kelvin): it
   is sealed, lists both in its `opens`, appears in both implementation sets, and
   evaluates with abstraction erased. */
let test_multi_abstract = () => {
  let st = Store.create();
  let int_h = Store.int_type(st);
  let ms = Mint.make_source();
  let c = Store.ingest_type(st, Tnode.Opaque({mint: Mint.fresh(ms), witness: int_h}));
  let k = Store.ingest_type(st, Tnode.Opaque({mint: Mint.fresh(ms), witness: int_h}));
  Alcotest.(check(bool))("Celsius != Kelvin (distinct mints)", false, Hash.equal(c, k));

  let arrow = (a, b) => Store.ingest_type(st, Tnode.Arrow(a, b));
  let ctx = Editing_context.make();
  Editing_context.open_type(ctx, c);
  Editing_context.open_type(ctx, k);

  /* c_to_k : Celsius.t -> Kelvin.t = \x. x + 273 */
  let (c2k, kind) =
    unwrap(
      Editing_context.commit(
        st,
        ctx,
        ~term=Node.Lam(c, Node.Prim(Node.Add, [Node.Var(0), Node.Lit(273)])),
        ~ann=arrow(c, k),
      ),
    );
  Alcotest.(check(bool))("cross-type op is sealed", true, is_sealed(kind));
  Alcotest.(check(bool))("external type is Celsius -> Kelvin", true, Hash.equal(tyof_in(st, c2k), arrow(c, k)));

  switch (Store.find(st, c2k)) {
  | Some(Definition.Term(Node.Seal({opens, _}))) =>
    Alcotest.(check(bool))("seal opens Celsius", true, List.mem(c, opens));
    Alcotest.(check(bool))("seal opens Kelvin", true, List.mem(k, opens));
  | _ => Alcotest.fail("c_to_k is not a seal")
  };

  Alcotest.(check(bool))("c_to_k unseals Celsius", true, List.mem(c2k, Store.unsealers(st, c)));
  Alcotest.(check(bool))("c_to_k unseals Kelvin", true, List.mem(c2k, Store.unsealers(st, k)));

  /* 0c converts to 273k; the value erases to the underlying Int */
  let (zero_c, _) = unwrap(Editing_context.commit(st, ctx, ~term=Node.Lit(0), ~ann=c));
  switch (Eval.eval_top(st, Node.App(Node.Ref(c2k), Node.Ref(zero_c)))) {
  | Ok(Eval.VInt(273)) => Alcotest.(check(bool))("c_to_k 0c = 273", true, true)
  | Ok(v) => Alcotest.fail("unexpected value: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("stuck: " ++ m)
  };
};

/* The `test` aspect: a marker store, plus the boolean evaluation that backs
   pass/fail. */
let test_aspect = () => {
  let att = Attachment.create();
  let h = Hash.digest_string("x");
  let g = Hash.digest_string("y");
  Alcotest.(check(bool))("not marked initially", false, Attachment.has(att, ~aspect="test", h));
  Attachment.mark(att, ~aspect="test", h);
  Alcotest.(check(bool))("marked", true, Attachment.has(att, ~aspect="test", h));
  Alcotest.(check(bool))("other not marked", false, Attachment.has(att, ~aspect="test", g));
  Alcotest.(check(int))("one in marked set", 1, List.length(Attachment.marked(att, ~aspect="test")));
  Attachment.unmark(att, ~aspect="test", h);
  Alcotest.(check(bool))("unmarked", false, Attachment.has(att, ~aspect="test", h));

  /* a test passes iff it evaluates to true */
  let st = Store.create();
  let passes = node =>
    switch (Eval.eval_top(st, node)) {
    | Ok(Eval.VBool(b)) => b
    | _ => Alcotest.fail("expected a boolean")
    };
  Alcotest.(check(bool))("2 == 2 passes", true, passes(Node.Prim(Node.Eq, [Node.Lit(2), Node.Lit(2)])));
  Alcotest.(check(bool))("2 == 3 fails", false, passes(Node.Prim(Node.Eq, [Node.Lit(2), Node.Lit(3)])));
};

/* An "internal" test unseals the abstract type — it compares an abstract value
   directly to its representation, so it needs the type open. It seals (opens the
   type), keeps external type Bool, and evaluates. This exercises the mixed-impl
   path (the impl references a sealed op AND unseals its result). */
let test_internal_test = () => {
  let st = Store.create();
  let int_h = Store.int_type(st);
  let bool_h = Store.bool_type(st);
  let ms = Mint.make_source();
  let counter = Store.ingest_type(st, Tnode.Opaque({mint: Mint.fresh(ms), witness: int_h}));
  let arrow = (a, b) => Store.ingest_type(st, Tnode.Arrow(a, b));
  let ctx = Editing_context.make();
  Editing_context.open_type(ctx, counter);
  let commit = (term, ann) => Editing_context.commit(st, ctx, ~term, ~ann);
  let (empty, _) = unwrap(commit(Node.Lit(0), counter));
  let (incr, _) =
    unwrap(commit(Node.Lam(counter, Node.Prim(Node.Add, [Node.Var(0), Node.Lit(1)])), arrow(counter, counter)));
  /* incr empty == 1 : Bool — unseals counter */
  let term = Node.Prim(Node.Eq, [Node.App(Node.Ref(incr), Node.Ref(empty)), Node.Lit(1)]);
  let (t, kind) = unwrap(commit(term, bool_h));
  Alcotest.(check(bool))("internal test seals (needed the unfold)", true, is_sealed(kind));
  Alcotest.(check(bool))("external type stays Bool", true, Hash.equal(tyof_in(st, t), bool_h));
  switch (Store.find(st, t)) {
  | Some(Definition.Term(Node.Seal({opens, _}))) =>
    Alcotest.(check(bool))("opens the abstract type", true, List.mem(counter, opens))
  | _ => Alcotest.fail("expected a seal")
  };
  switch (Eval.eval_top(st, Node.Ref(t))) {
  | Ok(Eval.VBool(true)) => Alcotest.(check(bool))("evaluates true", true, true)
  | Ok(v) => Alcotest.fail("unexpected: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("stuck: " ++ m)
  };
};

/* System-F: one polymorphic `step` functor applied to two counters with
   different internal representations (Int and Int*Int). Proves type abstraction
   (forall), type application, and that the body stays parametric (it only uses
   the passed ops; it never unseals the carrier). */
let test_systemf = () => {
  let st = Store.create();
  let int_h = Store.int_type(st);
  let bool_h = Store.bool_type(st);
  let ms = Mint.make_source();
  let arrow = (a, b) => Store.ingest_type(st, Tnode.Arrow(a, b));
  let prod = (a, b) => Store.ingest_type(st, Tnode.Product(a, b));

  /* step : forall t. ((t -> t) * (t -> t)) -> t -> Bool -> t
     = /\t. \ops. \x. \b. if b then (fst ops) x else (snd ops) x
     de Bruijn in the body: b = 0, x = 1, ops = 2 */
  let tv = Store.ingest_type(st, Tnode.TVar(0));
  let ops_ty = prod(arrow(tv, tv), arrow(tv, tv));
  let body =
    Node.If(
      Node.Var(0),
      Node.App(Node.Fst(Node.Var(2)), Node.Var(1)),
      Node.App(Node.Snd(Node.Var(2)), Node.Var(1)),
    );
  let step_node =
    Node.TyLam(Node.Lam(ops_ty, Node.Lam(tv, Node.Lam(bool_h, body))));
  let step = unwrap(Store.ingest_term(st, step_node));
  switch (Store.find(st, tyof_in(st, step))) {
  | Some(Definition.Type(Tnode.Forall(_))) =>
    Alcotest.(check(bool))("step is polymorphic (forall)", true, true)
  | _ => Alcotest.fail("step did not get a forall type")
  };

  /* counter A over Int */
  let a = Store.ingest_type(st, Tnode.Opaque({mint: Mint.fresh(ms), witness: int_h}));
  let ctxa = Editing_context.make();
  Editing_context.open_type(ctxa, a);
  let coma = (term, ann) => fst(unwrap(Editing_context.commit(st, ctxa, ~term, ~ann)));
  let empty_a = coma(Node.Lit(0), a);
  let incr_a = coma(Node.Lam(a, Node.Prim(Node.Add, [Node.Var(0), Node.Lit(1)])), arrow(a, a));
  let decr_a = coma(Node.Lam(a, Node.Prim(Node.Sub, [Node.Var(0), Node.Lit(1)])), arrow(a, a));

  /* counter B over Int * Int (a distinct representation) */
  let b = Store.ingest_type(st, Tnode.Opaque({mint: Mint.fresh(ms), witness: prod(int_h, int_h)}));
  Alcotest.(check(bool))("A and B are distinct types", false, Hash.equal(a, b));
  let ctxb = Editing_context.make();
  Editing_context.open_type(ctxb, b);
  let comb = (term, ann) => fst(unwrap(Editing_context.commit(st, ctxb, ~term, ~ann)));
  let start_b = comb(Node.Pair(Node.Lit(0), Node.Lit(0)), b);
  let incr_b =
    comb(
      Node.Lam(b, Node.Pair(Node.Prim(Node.Add, [Node.Fst(Node.Var(0)), Node.Lit(1)]), Node.Snd(Node.Var(0)))),
      arrow(b, b),
    );
  let decr_b =
    comb(
      Node.Lam(b, Node.Pair(Node.Prim(Node.Sub, [Node.Fst(Node.Var(0)), Node.Lit(1)]), Node.Snd(Node.Var(0)))),
      arrow(b, b),
    );

  /* apply the SAME step to both counters */
  let app4 = (tyarg, ops_pair, x, flag) =>
    Node.App(Node.App(Node.App(Node.TyApp(Node.Ref(step), tyarg), ops_pair), x), flag);

  let use_a =
    unwrap(
      Store.ingest_term(
        st,
        app4(a, Node.Pair(Node.Ref(incr_a), Node.Ref(decr_a)), Node.Ref(empty_a), Node.BoolLit(true)),
      ),
    );
  Alcotest.(check(bool))("step [A] ... : A", true, Hash.equal(tyof_in(st, use_a), a));
  switch (Eval.eval_top(st, Node.Ref(use_a))) {
  | Ok(Eval.VInt(1)) => Alcotest.(check(bool))("step up on A = 1", true, true)
  | Ok(v) => Alcotest.fail("A unexpected: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("A stuck: " ++ m)
  };

  let use_b =
    unwrap(
      Store.ingest_term(
        st,
        app4(b, Node.Pair(Node.Ref(incr_b), Node.Ref(decr_b)), Node.Ref(start_b), Node.BoolLit(false)),
      ),
    );
  Alcotest.(check(bool))("step [B] ... : B", true, Hash.equal(tyof_in(st, use_b), b));
  switch (Eval.eval_top(st, Node.Ref(use_b))) {
  | Ok(Eval.VPair(Eval.VInt(-1), Eval.VInt(0))) =>
    Alcotest.(check(bool))("step down on B = (-1, 0)", true, true)
  | Ok(v) => Alcotest.fail("B unexpected: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("B stuck: " ++ m)
  };
};

/* Existentials: a factory that hides its representation. mkCounter returns a
   counter package over either Int or Int*Int (the two `if` branches pack
   different witnesses, unifying at the one `∃` type); the consumer unpacks and
   observes the same value either way, and cannot leak the abstract value out
   (avoidance). Uses the surface parser since pack/unpack nodes are unwieldy. */
let test_existential = () => {
  let st = Store.create();
  let ns = Namespace.create();
  let res_ty = s =>
    switch (Parse.parse_ty(s)) {
    | Ok(t) => unwrap(Resolver.resolve_ty(~ns, ~st, t))
    | Error(m) => Alcotest.fail("type parse: " ++ m)
    };
  let res = s =>
    switch (Parse.parse_expr(s)) {
    | Ok(e) => unwrap(Resolver.resolve(~ctx=[], ~ns, ~st, e))
    | Error(m) => Alcotest.fail("expr parse: " ++ m)
    };
  let bind_str = (name, ty_s, expr_s) => {
    let ann = res_ty(ty_s);
    let node = res(expr_s);
    let (h, _) = unwrap(Editing_context.commit(st, Editing_context.make(), ~term=node, ~ann));
    Namespace.rebind(ns, ~name, h);
  };
  let ex = "exists t. t * ((t -> t) * (t -> Int))";
  bind_str(
    "mkCounter",
    "Bool -> " ++ ex,
    "\\fast: Bool. if fast "
    ++ "then pack [Int] (0, (\\x: Int. x + 1, \\x: Int. x)) as "
    ++ ex
    ++ " else pack [Int * Int] ((0, 0), (\\p: Int * Int. (fst p + 1, snd p), \\p: Int * Int. fst p)) as "
    ++ ex,
  );
  /* get (incr (incr empty)) on the unpacked package */
  let use = b =>
    res(
      "unpack [t] c = mkCounter "
      ++ b
      ++ " in snd (snd c) ((fst (snd c)) ((fst (snd c)) (fst c)))",
    );
  let observe = b =>
    switch (Eval.eval_top(st, Node.Ref(unwrap(Store.ingest_term(st, use(b)))))) {
    | Ok(Eval.VInt(n)) => n
    | Ok(v) => Alcotest.fail("not an int: " ++ Eval.to_string(v))
    | Error(m) => Alcotest.fail("stuck: " ++ m)
    };
  Alcotest.(check(int))("Int-rep counter observes 2", 2, observe("true"));
  Alcotest.(check(int))("Pair-rep counter observes 2", 2, observe("false"));
  /* leaking the abstract value out of unpack is rejected (avoidance) */
  switch (Store.ingest_term(st, res("unpack [t] c = mkCounter true in fst c"))) {
  | Error(_) => Alcotest.(check(bool))("witness escape rejected", true, true)
  | Ok(_) => Alcotest.fail("UNSOUND: the witness type escaped its scope")
  };
};

let () =
  Alcotest.run(
    "p14",
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
        [
          Alcotest.test_case("counter worked example", `Quick, test_counter),
          Alcotest.test_case(
            "representation change is sound (fresh mint)",
            `Quick,
            test_rep_change_soundness,
          ),
          Alcotest.test_case(
            "operation over multiple abstract types",
            `Quick,
            test_multi_abstract,
          ),
        ],
      ),
      (
        "aspects",
        [
          Alcotest.test_case("test aspect + pass/fail", `Quick, test_aspect),
          Alcotest.test_case("internal (unsealing) test", `Quick, test_internal_test),
        ],
      ),
      (
        "system-f",
        [
          Alcotest.test_case(
            "polymorphic step over two representations",
            `Quick,
            test_systemf,
          ),
        ],
      ),
      (
        "existentials",
        [
          Alcotest.test_case(
            "factory hides its representation (pack/unpack)",
            `Quick,
            test_existential,
          ),
        ],
      ),
    ],
  );
