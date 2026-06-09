/* p17 substrate tests: primitive layer (Hash, Mint) + the design/12 Counter
   worked example built through the editing context. */

open P17_substrate;

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

/* ---- p17: translucent modules ---- */

/* Shared setup: declare CounterSig (minting its labels), give back parsers. */
let mk_module_env = () => {
  let st = Store.create();
  let ns = Namespace.create();
  let ms = Mint.make_source();
  let res_ty = s =>
    switch (Parse.parse_ty(s)) {
    | Ok(t) => unwrap(Resolver.resolve_ty(~ns, ~st, ~mint=Some(ms), t))
    | Error(m) => Alcotest.fail("type parse: " ++ m)
    };
  let res = s =>
    switch (Parse.parse_expr(s)) {
    | Ok(e) => unwrap(Resolver.resolve(~ctx=[], ~mint=Some(ms), ~ns, ~st, e))
    | Error(m) => Alcotest.fail("expr parse: " ++ m)
    };
  let try_res = s =>
    switch (Parse.parse_expr(s)) {
    | Ok(e) => Resolver.resolve(~ctx=[], ~mint=Some(ms), ~ns, ~st, e)
    | Error(m) => Error("parse: " ++ m)
    };
  (st, ns, ms, res_ty, res, try_res);
};

let counter_sig = "sig { type t, empty: t, incr: t -> t, get: t -> Int }";
let counter_struct = "struct { type t = Int, empty = 0, incr = \\x: t. x + 1, get = \\x: t. x }";

/* Sig canonicalization: binding is by sorted label-hash rank, not declaration
   position, so component order carries no information — both declaration
   orders of the same two-opaque sig hash identically (the rank rule's acid
   test: whichever way the two labels sort, one of these declares them in the
   "wrong" textual order). */
let test_sig_rank_rule = () => {
  let (st, ns, ms, _, _, _) = mk_module_env();
  let res_ty = s =>
    switch (Parse.parse_ty(s)) {
    | Ok(t) => unwrap(Resolver.resolve_ty(~ns, ~st, ~mint=Some(ms), t))
    | Error(m) => Alcotest.fail("type parse: " ++ m)
    };
  let a = res_ty("sig { type date, type span, shift: date -> span -> date }");
  let b = res_ty("sig { type span, type date, shift: date -> span -> date }");
  Alcotest.(check(bool))("opaque declaration order is immaterial", true, Hash.equal(a, b));
  let c = res_ty("sig { shift: date -> span -> date, type date, type span }");
  Alcotest.(check(bool))("value-vs-type interleaving is immaterial", true, Hash.equal(a, c));
  /* swapping which name is opaque is NOT the same sig */
  let d = res_ty("sig { type date, type span, shift: span -> date -> span }");
  Alcotest.(check(bool))("the payload still distinguishes the roles", false, Hash.equal(a, d));
};

/* Ascription `M :> S` — the new pack. Witnessing, manifest matching, width
   (private members), depth rejection, and structural (mint-free) identity. */
let test_ascription = () => {
  let (st, _ns, _ms, res_ty, res, _) = mk_module_env();
  let csig = res_ty(counter_sig);
  let sealed = res("(" ++ counter_struct ++ ") :> " ++ counter_sig);
  let h = unwrap(Store.ingest_term(st, sealed));
  Alcotest.(check(bool))("sealed module : CounterSig", true, Hash.equal(tyof_in(st, h), csig));

  /* width: an extra (private) member is ignored by the ascription */
  let wide =
    "struct { type t = Int, empty = 0, incr = \\x: t. x + 1, get = \\x: t. x, raw = 99 }";
  let h2 = unwrap(Store.ingest_term(st, res("(" ++ wide ++ ") :> " ++ counter_sig)));
  Alcotest.(check(bool))("private member: still : CounterSig", true, Hash.equal(tyof_in(st, h2), csig));

  /* missing member rejected */
  let narrow = "struct { type t = Int, empty = 0, incr = \\x: t. x + 1 }";
  Alcotest.(check(bool))(
    "missing value member rejected",
    true,
    is_error(Store.ingest_term(st, res("(" ++ narrow ++ ") :> " ++ counter_sig))),
  );

  /* depth: a member at the wrong type rejected */
  let wrong = "struct { type t = Int, empty = 0, incr = \\x: t. x + 1, get = \\x: t. true }";
  Alcotest.(check(bool))(
    "wrong member type rejected (no depth subtyping)",
    true,
    is_error(Store.ingest_term(st, res("(" ++ wrong ++ ") :> " ++ counter_sig))),
  );

  /* manifest components: must match exactly */
  let vsig = res_ty("sig { type scalar = Int, one: scalar }");
  let vok = res("(struct { type scalar = Int, one = 1 }) :> sig { type scalar = Int, one: scalar }");
  let vh = unwrap(Store.ingest_term(st, vok));
  Alcotest.(check(bool))("manifest equation matches", true, Hash.equal(tyof_in(st, vh), vsig));
  Alcotest.(check(bool))(
    "manifest equation mismatch rejected",
    true,
    is_error(
      Store.ingest_term(
        st,
        res("(struct { type scalar = Bool, one = 1 }) :> sig { type scalar = Int, one: scalar }"),
      ),
    ),
  );

  /* ascription is structural, not minted: the same ascription re-authored is
     the same definition, and two branches unify at the one sig type */
  let again = unwrap(Store.ingest_term(st, res("(" ++ counter_struct ++ ") :> " ++ counter_sig)));
  Alcotest.(check(bool))("re-authoring shares the hash (no mint)", true, Hash.equal(h, again));
  let factory =
    "\\fast: Bool. if fast then (" ++ counter_struct ++ ") :> " ++ counter_sig
    ++ " else (struct { type t = Int * Int, empty = (0, 0), incr = \\p: t. (fst p + 1, snd p), get = \\p: t. fst p }) :> "
    ++ counter_sig;
  let fh = unwrap(Store.ingest_term(st, res(factory)));
  switch (Store.find(st, tyof_in(st, fh))) {
  | Some(Definition.Type(Tnode.Arrow(_, cod))) =>
    Alcotest.(check(bool))("two witnesses unify at one sig", true, Hash.equal(cod, csig))
  | _ => Alcotest.fail("factory is not an arrow")
  };

  /* forgetting: re-ascribe an already-sealed module to a NARROWER sig (drops
     `get`); the witness for the target opaque is the impl's own opaque (the
     k_i > 0 retarget path) */
  let smaller = "sig { type t, empty: t, incr: t -> t }";
  let ssig = res_ty(smaller);
  let fh2 = unwrap(Store.ingest_term(st, res("((" ++ counter_struct ++ ") :> " ++ counter_sig ++ ") :> " ++ smaller)));
  Alcotest.(check(bool))("sealed module re-ascribed to a narrower sig", true, Hash.equal(tyof_in(st, fh2), ssig));
};

/* n-ary top-level open: ONE Open node mints one Abstract per opaque component
   of a two-opaque calendar sig; field names (and the hidden types' names!)
   are recovered from the labels; generativity = two opens are incompatible. */
let test_open_module = () => {
  let (st, _ns, ms, _res_ty, res, _) = mk_module_env();
  let cal_sig = "sig { type date, type span, origin: date, after: Int -> span, shift: date -> span -> date, between: date -> date -> span, length_of: span -> Int }";
  let cal_struct = "struct { type date = Int, type span = Int, origin = 0, after = \\n: Int. n, shift = \\d: date. \\s: span. d + s, between = \\a: date. \\b: date. b - a, length_of = \\s: span. s }";
  let pkg = res("(" ++ cal_struct ++ ") :> " ++ cal_sig);

  switch (Open_module.open_package(st, ms, pkg)) {
  | Error(m) => Alcotest.fail("open: " ++ m)
  | Ok({Open_module.type_bindings, module_hash, fields}) =>
    Alcotest.(check(int))("two hidden types extracted in one open", 2, List.length(type_bindings));
    switch (List.map(snd, type_bindings)) {
    | [a, b] =>
      Alcotest.(check(bool))("date and span are distinct Abstracts", false, Hash.equal(a, b));
      switch (Store.find(st, a)) {
      | Some(Definition.Type(Tnode.Abstract(_))) => ()
      | _ => Alcotest.fail("extracted type is not an Abstract")
      };
    | _ => Alcotest.fail("expected two type bindings")
    };
    /* the module value is ONE n-ary Open node (no nesting) */
    switch (Store.find(st, module_hash)) {
    | Some(Definition.Term(Node.Open({mints, _}))) =>
      Alcotest.(check(int))("a single Open node carries both mints", 2, List.length(mints))
    | _ => Alcotest.fail("module is not an Open node")
    };
    Alcotest.(check(int))("five labeled fields projected", 5, List.length(fields));
    /* labels make the fields addressable without positional guessing */
    let field = name => {
      let lh =
        switch (Namespace.resolve(_ns, name)) {
        | Some(l) => l
        | None => Alcotest.fail("label not bound: " ++ name)
        };
      switch (List.find_opt(((l, _)) => Hash.equal(l, lh), fields)) {
      | Some((_, fh)) => fh
      | None => Alcotest.fail("field not recovered: " ++ name)
      };
    };
    let r = h => Node.Ref(h);
    let app = (f, x) => Node.App(f, x);
    /* length_of (between origin (shift origin (after 30))) = 30 */
    let d30 = app(app(r(field("shift")), r(field("origin"))), app(r(field("after")), Node.Lit(30)));
    let len = app(r(field("length_of")), app(app(r(field("between")), r(field("origin"))), d30));
    let h = unwrap(Store.ingest_term(st, len));
    switch (Eval.eval_top(st, Node.Ref(h))) {
    | Ok(Eval.VInt(30)) => Alcotest.(check(bool))("calendar round trip = 30", true, true)
    | Ok(v) => Alcotest.fail("unexpected: " ++ Eval.to_string(v))
    | Error(m) => Alcotest.fail("stuck: " ++ m)
    };
    /* date != span: shifting a date by a date is ill-typed */
    let bad = app(app(r(field("shift")), r(field("origin"))), r(field("origin")));
    Alcotest.(check(bool))(
      "shift date-by-date rejected (date != span)",
      true,
      is_error(Store.ingest_term(st, bad)),
    );
    /* a struct member may reference an enclosing binder (the seeded
       mk_calendar factory: `\base. struct { origin = base, ... } :> CalSig`) */
    let factory =
      "\\base: Int. (struct { type date = Int, type span = Int, origin = base, after = \\n: Int. n, shift = \\d: date. \\s: span. d + s, between = \\a: date. \\b: date. b - a, length_of = \\s: span. s }) :> "
      ++ cal_sig;
    let fnode = res(factory);
    ignore(unwrap(Store.ingest_term(st, fnode)));
    switch (Open_module.open_package(st, ms, Node.App(fnode, Node.Lit(7)))) {
    | Error(m) => Alcotest.fail("open factory: " ++ m)
    | Ok(_) =>
      Alcotest.(check(bool))("parameterized factory opens", true, true)
    };
    /* generativity: a second open of the SAME package is a different abstraction */
    switch (Open_module.open_package(st, ms, pkg)) {
    | Error(m) => Alcotest.fail("re-open: " ++ m)
    | Ok({Open_module.type_bindings: tb2, fields: fields2, _}) =>
      Alcotest.(check(bool))(
        "re-opening mints fresh types",
        false,
        Hash.equal(snd(List.hd(type_bindings)), snd(List.hd(tb2))),
      );
      /* mixing the two opens is rejected */
      let lh =
        switch (Namespace.resolve(_ns, "shift")) {
        | Some(l) => l
        | None => Alcotest.fail("label not bound: shift")
        };
      let shift2 =
        switch (List.find_opt(((l, _)) => Hash.equal(l, lh), fields2)) {
        | Some((_, fh)) => fh
        | None => Alcotest.fail("shift not in second open")
        };
      let mixed = app(app(r(shift2), r(field("origin"))), app(r(field("after")), Node.Lit(1)));
      Alcotest.(check(bool))(
        "values from two opens do not mix",
        true,
        is_error(Store.ingest_term(st, mixed)),
      );
    };
  };
};

/* Translucency through open: a manifest component stays transparent — the
   opened module binds it to its equation, and consumers use it concretely. */
let test_translucent_open = () => {
  let (st, ns, ms, _res_ty, res, _) = mk_module_env();
  let vec_sig = "sig { type scalar = Int, type v, zero: v, add: v -> v -> v, smul: scalar -> v -> v, norm1: v -> scalar }";
  let vec_struct = "struct { type scalar = Int, type v = Int * Int, zero = (0, 0), add = \\a: v. \\b: v. (fst a + fst b, snd a + snd b), smul = \\k: scalar. \\a: v. (k mul fst a, k mul snd a), norm1 = \\a: v. fst a + snd a }";
  let pkg = res("(" ++ vec_struct ++ ") :> " ++ vec_sig);
  switch (Open_module.open_package(st, ms, pkg)) {
  | Error(m) => Alcotest.fail("open: " ++ m)
  | Ok({Open_module.type_bindings, fields, _}) =>
    /* scalar (manifest) binds to Int itself; v (opaque) binds to an Abstract */
    let binding = name => {
      let lh =
        switch (Namespace.resolve(ns, name)) {
        | Some(l) => l
        | None => Alcotest.fail("label not bound: " ++ name)
        };
      switch (List.find_opt(((l, _)) => Hash.equal(l, lh), type_bindings)) {
      | Some((_, h)) => h
      | None => Alcotest.fail("type binding not recovered: " ++ name)
      };
    };
    Alcotest.(check(bool))(
      "manifest member binds to its equation (Int)",
      true,
      Hash.equal(binding("scalar"), Store.int_type(st)),
    );
    switch (Store.find(st, binding("v"))) {
    | Some(Definition.Type(Tnode.Abstract(_))) => ()
    | _ => Alcotest.fail("opaque member did not become an Abstract")
    };
    /* the translucency payoff: a literal 3 feeds smul because scalar = Int */
    let field = name => {
      let lh =
        switch (Namespace.resolve(ns, name)) {
        | Some(l) => l
        | None => Alcotest.fail("label not bound: " ++ name)
        };
      switch (List.find_opt(((l, _)) => Hash.equal(l, lh), fields)) {
      | Some((_, fh)) => fh
      | None => Alcotest.fail("field not recovered: " ++ name)
      };
    };
    let app = (f, x) => Node.App(f, x);
    let r = h => Node.Ref(h);
    let one1 = app(app(r(field("smul")), Node.Lit(3)), app(app(r(field("add")), r(field("zero"))), r(field("zero"))));
    let n = app(r(field("norm1")), one1);
    let h = unwrap(Store.ingest_term(st, n));
    switch (Eval.eval_top(st, Node.Ref(h))) {
    | Ok(Eval.VInt(0)) => Alcotest.(check(bool))("norm1 (smul 3 zero+zero) = 0", true, true)
    | Ok(v) => Alcotest.fail("unexpected: " ++ Eval.to_string(v))
    | Error(m) => Alcotest.fail("stuck: " ++ m)
    };
  };
};

/* The p15 open-question answered: `open e as M in body` INSIDE a function.
   Scoped like unpack (no mint, alpha-equivalent re-authorings share a hash),
   avoidance-checked, with `M#field`, `M.field`, and `M.t`-annotations all
   resolving in the body. */
let test_open_local = () => {
  let (st, _ns, _ms, res_ty, res, try_res) = mk_module_env();
  let csig = res_ty(counter_sig);
  ignore(csig);

  /* a consumer over ANY counter module: open the argument in the body */
  let total = "\\m: " ++ counter_sig ++ ". open m as c in c#get (c#incr (c#incr c#empty))";
  let th = unwrap(Store.ingest_term(st, res(total)));
  switch (Store.find(st, tyof_in(st, th))) {
  | Some(Definition.Type(Tnode.Arrow(_, cod))) =>
    Alcotest.(check(bool))("total : CounterSig -> Int", true, Hash.equal(cod, Store.int_type(st)))
  | _ => Alcotest.fail("total is not an arrow")
  };

  /* the factory recast (no pack): both witnesses observe 2 through the open */
  let factory =
    "\\fast: Bool. if fast then (" ++ counter_struct ++ ") :> " ++ counter_sig
    ++ " else (struct { type t = Int * Int, empty = (0, 0), incr = \\p: t. (fst p + 1, snd p), get = \\p: t. fst p }) :> "
    ++ counter_sig;
  let fh = unwrap(Store.ingest_term(st, res(factory)));
  let observe = b => {
    let use = Node.App(Node.Ref(th), Node.App(Node.Ref(fh), Node.BoolLit(b)));
    switch (Eval.eval_top(st, Node.Ref(unwrap(Store.ingest_term(st, use))))) {
    | Ok(Eval.VInt(n)) => n
    | Ok(v) => Alcotest.fail("not an int: " ++ Eval.to_string(v))
    | Error(m) => Alcotest.fail("stuck: " ++ m)
    };
  };
  Alcotest.(check(int))("Int-rep counter observes 2", 2, observe(true));
  Alcotest.(check(int))("Pair-rep counter observes 2", 2, observe(false));

  /* structural identity: re-authoring the same local open shares the hash */
  let th2 = unwrap(Store.ingest_term(st, res(total)));
  Alcotest.(check(bool))("local open is mint-free (same hash)", true, Hash.equal(th, th2));

  /* avoidance: the hidden type may not escape the body */
  switch (try_res("\\m: " ++ counter_sig ++ ". open m as c in c#empty")) {
  | Error(_) => Alcotest.(check(bool))("escape rejected at resolution", true, true)
  | Ok(node) =>
    Alcotest.(check(bool))(
      "hidden-type escape rejected",
      true,
      is_error(Store.ingest_term(st, node)),
    )
  };

  /* `M.t` in an annotation inside the body, and `M.field` as projection */
  let annotated =
    "\\m: " ++ counter_sig ++ ". open m as c in (\\x: c.t. c.get x) (c.incr c.empty)";
  let ah = unwrap(Store.ingest_term(st, res(annotated)));
  let use = Node.App(Node.Ref(ah), Node.App(Node.Ref(fh), Node.BoolLit(true)));
  switch (Eval.eval_top(st, Node.Ref(unwrap(Store.ingest_term(st, use))))) {
  | Ok(Eval.VInt(1)) => Alcotest.(check(bool))("c.t annotation + c.field = 1", true, true)
  | Ok(v) => Alcotest.fail("unexpected: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("stuck: " ++ m)
  };

  /* direct projection from an unsealed-module VALUE: hidden-type-mentioning
     members do not project (only the local open gives them meaning) */
  let direct = "\\m: " ++ counter_sig ++ ". m#get";
  switch (try_res(direct)) {
  | Error(_) => Alcotest.(check(bool))("hidden-type member projection rejected", true, true)
  | Ok(node) =>
    Alcotest.(check(bool))(
      "hidden-type member projection rejected",
      true,
      is_error(Store.ingest_term(st, node)),
    )
  };
};

/* fold is the only list eliminator: sum [1,2,3] via the explicit spine. */
let test_fold = () => {
  let st = Store.create();
  let ns = Namespace.create();
  let res = s =>
    switch (Parse.parse_expr(s)) {
    | Ok(e) => unwrap(Resolver.resolve(~ctx=[], ~ns, ~st, e))
    | Error(m) => Alcotest.fail("parse: " ++ m)
    };
  let sum =
    res("fold (cons 1 (cons 2 (cons 3 (nil [Int])))) 0 (\\x: Int. \\acc: Int. x + acc)");
  let sum_h = unwrap(Store.ingest_term(st, sum));
  switch (Eval.eval_top(st, Node.Ref(sum_h))) {
  | Ok(Eval.VInt(6)) => Alcotest.(check(bool))("fold sum [1,2,3] = 6", true, true)
  | Ok(v) => Alcotest.fail("fold unexpected: " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("fold stuck: " ++ m)
  };
};

/* `[| ... |]` is pure surface sugar: it resolves to the same canonical cons-spine
   as the explicit form (same hash), infers the element type from the head, and
   refuses when the head's type can't be synthesized in the empty context. */
let test_list_literals = () => {
  let st = Store.create();
  let ns = Namespace.create();
  let resolve = s =>
    switch (Parse.parse_expr(s)) {
    | Ok(e) => Resolver.resolve(~ctx=[], ~ns, ~st, e)
    | Error(m) => Alcotest.fail("parse: " ++ m)
    };
  let ingest = s => unwrap(Store.ingest_term(st, unwrap(resolve(s))));
  /* sugar == explicit cons-spine, by content address */
  let lit = ingest("[| 1, 2, 3 |]");
  let explicit = ingest("cons 1 (cons 2 (cons 3 (nil [Int])))");
  Alcotest.(check(bool))("literal hashes to the cons-spine", true, Hash.equal(lit, explicit));
  /* element type inferred from the head */
  Alcotest.(check(string))("inferred List Int", "List Int", Pretty.ty(~ns, ~st, tyof_in(st, lit)));
  /* p17 upgrade: the resolver now tracks binder types (tctx, added for local
     open), so a locally-bound head's element type IS inferable — p16 rejected
     this. `\x: Int. [| x |] : Int -> List Int`. */
  let under = ingest("\\x: Int. [| x |]");
  switch (Store.find(st, tyof_in(st, under))) {
  | Some(Definition.Type(Tnode.Arrow(_, cod))) =>
    Alcotest.(check(string))(
      "binder-headed literal infers List Int",
      "List Int",
      Pretty.ty(~ns, ~st, cod),
    )
  | _ => Alcotest.fail("expected an arrow")
  };
  /* heterogeneous list still rejected at ingest */
  let het = unwrap(resolve("[| 1, true |]"));
  Alcotest.(check(bool))(
    "heterogeneous list rejected",
    true,
    is_error(Store.ingest_term(st, het)),
  );
};

/* p17 stage 2 — the label sort + record substrate: labels are minted (distinct
   field identity), record types/literals are canonical by sorted label hash
   (`{x,y}` = `{y,x}`), and projection type-checks + evaluates by label. */
let test_records = () => {
  let st = Store.create();
  let ms = Mint.make_source();
  let int_h = Store.int_type(st);
  let bool_h = Store.bool_type(st);
  let mklabel = () => Store.ingest_label(st, Label.create(Mint.fresh(ms)));
  let (x, y, z) = (mklabel(), mklabel(), mklabel());
  Alcotest.(check(bool))("distinct labels are distinct", false, Hash.equal(x, y));
  let rxy = Store.ingest_type(st, Tnode.Record([(x, int_h), (y, bool_h)]));
  let ryx = Store.ingest_type(st, Tnode.Record([(y, bool_h), (x, int_h)]));
  Alcotest.(check(bool))("record type is permutation-invariant", true, Hash.equal(rxy, ryx));
  let rzy = Store.ingest_type(st, Tnode.Record([(z, int_h), (y, bool_h)]));
  Alcotest.(check(bool))("different label => different record", false, Hash.equal(rxy, rzy));
  let lit = Node.Record_lit([(x, Node.Lit(1)), (y, Node.BoolLit(true))]);
  let lh = unwrap(Store.ingest_term(st, lit));
  Alcotest.(check(bool))("{x=1,y=true} : {x:Int,y:Bool}", true, Hash.equal(tyof_in(st, lh), rxy));
  let px = unwrap(Store.ingest_term(st, Node.Project_field(Node.Ref(lh), x)));
  let py = unwrap(Store.ingest_term(st, Node.Project_field(Node.Ref(lh), y)));
  Alcotest.(check(bool))("r#x : Int", true, Hash.equal(tyof_in(st, px), int_h));
  Alcotest.(check(bool))("r#y : Bool", true, Hash.equal(tyof_in(st, py), bool_h));
  switch (Eval.eval_top(st, Node.Ref(px))) {
  | Ok(Eval.VInt(1)) => Alcotest.(check(bool))("r#x = 1", true, true)
  | Ok(v) => Alcotest.fail("r#x = " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("r#x stuck: " ++ m)
  };
  switch (Eval.eval_top(st, Node.Ref(py))) {
  | Ok(Eval.VBool(true)) => Alcotest.(check(bool))("r#y = true", true, true)
  | Ok(v) => Alcotest.fail("r#y = " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("r#y stuck: " ++ m)
  };
  let bad = Node.Project_field(Node.Ref(lh), z);
  Alcotest.(check(bool))(
    "projecting an absent field is rejected",
    true,
    is_error(Store.ingest_term(st, bad)),
  );
};

/* p17 stage 4 — surface syntax: a record-type declaration mints its field
   labels; record literals and `e#x` projection resolve field names to those
   labels; an undeclared field name in a literal is rejected. */
let test_record_surface = () => {
  let st = Store.create();
  let ns = Namespace.create();
  let ms = Mint.make_source();
  let pt =
    switch (Parse.parse_ty("{ px: Int, py: Int }")) {
    | Ok(s) => s
    | Error(m) => Alcotest.fail("parse ty: " ++ m)
    };
  /* declaring the record type mints the px/py labels (decision 1) */
  ignore(unwrap(Resolver.resolve_ty(~ns, ~st, ~mint=Some(ms), pt)));
  let res = s =>
    switch (Parse.parse_expr(s)) {
    | Ok(e) => unwrap(Resolver.resolve(~ctx=[], ~ns, ~st, e))
    | Error(m) => Alcotest.fail("parse: " ++ m)
    };
  /* a record literal projects by field name (labels resolve to the minted ones) */
  let h = unwrap(Store.ingest_term(st, res("{ px = 3, py = 4 }#px")));
  switch (Eval.eval_top(st, Node.Ref(h))) {
  | Ok(Eval.VInt(3)) => Alcotest.(check(bool))("{px=3,py=4}#px = 3", true, true)
  | Ok(v) => Alcotest.fail("got " ++ Eval.to_string(v))
  | Error(m) => Alcotest.fail("stuck: " ++ m)
  };
  /* an undeclared field name in a literal is rejected (declare the type first) */
  Alcotest.(check(bool))(
    "undeclared field label is rejected",
    true,
    switch (Parse.parse_expr("{ zz = 1 }")) {
    | Ok(e) => is_error(Resolver.resolve(~ctx=[], ~ns, ~st, e))
    | Error(_) => true
    },
  );
};

let () =
  Alcotest.run(
    "p17",
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
        "modules",
        [
          Alcotest.test_case(
            "sig rank rule: component order is immaterial",
            `Quick,
            test_sig_rank_rule,
          ),
          Alcotest.test_case(
            "ascription M :> S: witnessing, manifest, width, forgetting",
            `Quick,
            test_ascription,
          ),
          Alcotest.test_case(
            "n-ary open: one gesture, labeled hidden types, generative",
            `Quick,
            test_open_module,
          ),
          Alcotest.test_case(
            "translucency: manifest members stay transparent through open",
            `Quick,
            test_translucent_open,
          ),
          Alcotest.test_case(
            "local open inside a function (scoped, avoidance, mint-free)",
            `Quick,
            test_open_local,
          ),
        ],
      ),
      (
        "records",
        [
          Alcotest.test_case(
            "label sort + record types/literals/projection",
            `Quick,
            test_records,
          ),
          Alcotest.test_case(
            "surface: { x: T } decl mints labels; { x = e } / e#x resolve",
            `Quick,
            test_record_surface,
          ),
        ],
      ),
      (
        "lists",
        [
          Alcotest.test_case(
            "[| ... |] literal sugar + element inference",
            `Quick,
            test_list_literals,
          ),
          Alcotest.test_case("fold sums a spine", `Quick, test_fold),
        ],
      ),
    ],
  );
