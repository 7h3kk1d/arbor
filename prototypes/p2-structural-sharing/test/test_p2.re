open P2_structural_sharing;

let parse = (s: string): Ast.t => {
  let lexbuf = Lexing.from_string(s);
  Parser.main(Lexer.token, lexbuf);
};

let ast_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Ast.show(t)),
    Ast.equal,
  );

/* Result equality for eval_testable: Value/Stuck discriminated, hash-equal payloads. */
let eval_result_testable =
  Alcotest.testable(
    (fmt, r) =>
      switch (r) {
      | Eval.Value(h) => Format.fprintf(fmt, "Value(%s)", Hash.short(h))
      | Eval.Stuck(h) => Format.fprintf(fmt, "Stuck(%s)", Hash.short(h))
      },
    (a, b) =>
      switch (a, b) {
      | (Eval.Value(x), Eval.Value(y)) => Hash.equal(x, y)
      | (Eval.Stuck(x), Eval.Stuck(y)) => Hash.equal(x, y)
      | _ => false
      },
  );

let fresh_world = () => {
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Eval.descriptor);
  (store, att);
};

/* ===== Parser (carried from p1) ===== */

let test_parse_atoms = () => {
  Alcotest.check(ast_testable, "true", Ast.True, parse("true"));
  Alcotest.check(ast_testable, "false", Ast.False, parse("false"));
  Alcotest.check(ast_testable, "0", Ast.Zero, parse("0"));
};

let test_parse_nested = () =>
  Alcotest.check(
    ast_testable,
    "succ (succ 0)",
    Ast.Succ(Ast.Succ(Ast.Zero)),
    parse("succ (succ 0)"),
  );

let test_parse_if = () =>
  Alcotest.check(
    ast_testable,
    "if iszero 0 then succ 0 else 0",
    Ast.If(Ast.IsZero(Ast.Zero), Ast.Succ(Ast.Zero), Ast.Zero),
    parse("if iszero 0 then succ 0 else 0"),
  );

/* ===== qcheck generator ===== */

let rec arith_ast_gen = (depth): QCheck.Gen.t(Ast.t) => {
  open QCheck.Gen;
  let leaves = oneof([return(Ast.True), return(Ast.False), return(Ast.Zero)]);
  if (depth <= 0) {
    leaves;
  } else {
    let sub = arith_ast_gen(depth - 1);
    oneof_weighted([
      (3, leaves),
      (2, map(a => Ast.Succ(a), sub)),
      (2, map(a => Ast.Pred(a), sub)),
      (2, map(a => Ast.IsZero(a), sub)),
      (2, map3((c, t, e) => Ast.If(c, t, e), sub, sub, sub)),
    ]);
  };
};

let arith_ast_arb = QCheck.make(~print=Ast.show, arith_ast_gen(4));

/* ===== Ingest / reconstruct ===== */

let prop_ingest_reconstruct_roundtrip =
  QCheck.Test.make(
    ~count=200,
    ~name="reconstruct(ingest(t)) = Some(t)",
    arith_ast_arb,
    t => {
      let store = Store.create();
      let h = Store.ingest(store, t);
      switch (Store.reconstruct(store, h)) {
      | Some(t') => Ast.equal(t, t')
      | None => false
      };
    },
  );

let prop_ingest_determinism =
  QCheck.Test.make(
    ~count=200,
    ~name="ingest yields the same hash each time",
    arith_ast_arb,
    t => {
      let s1 = Store.create();
      let s2 = Store.create();
      Hash.equal(Store.ingest(s1, t), Store.ingest(s2, t));
    },
  );

let prop_ingest_structural_equality =
  QCheck.Test.make(
    ~count=200,
    ~name="equal ASTs ingest to equal hashes",
    QCheck.pair(arith_ast_arb, arith_ast_arb),
    ((a, b)) =>
      if (Ast.equal(a, b)) {
        let s1 = Store.create();
        let s2 = Store.create();
        Hash.equal(Store.ingest(s1, a), Store.ingest(s2, b));
      } else {
        true;
      },
  );

/* ===== Structural sharing ===== */

let test_subterm_sharing = () => {
  let store = Store.create();
  let _ = Store.ingest(store, parse("pred (succ (succ 0))"));
  let size_before = Store.size(store);
  /* Ingesting succ 0 (a subterm of the above) should not add new entries. */
  let _ = Store.ingest(store, parse("succ 0"));
  Alcotest.(check(int))(
    "no new entries when re-ingesting a subterm",
    size_before,
    Store.size(store),
  );
};

let test_shared_across_registrations = () => {
  let store = Store.create();
  let h_pred = Store.ingest(store, parse("pred (succ 0)"));
  let h_iszero = Store.ingest(store, parse("iszero (succ 0)"));
  /* Both share the `succ 0` subterm. Lookup the contents, extract the child
     hash, and confirm it's the same. */
  switch (Store.lookup(store, h_pred), Store.lookup(store, h_iszero)) {
  | (Some(Node.Pred(h1)), Some(Node.IsZero(h2))) =>
    Alcotest.(check(bool))(
      "succ 0 subterm is shared by hash",
      true,
      Hash.equal(h1, h2),
    )
  | _ => Alcotest.fail("unexpected top-level shapes")
  };
};

/* ===== Evaluator: per-constructor correctness ===== */

let eval_ast = (src) => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse(src));
  (store, att, h, Eval.eval(~store, ~att, h));
};

let test_eval_booleans = () => {
  let (store, att) = fresh_world();
  let h_true = Store.ingest(store, Ast.True);
  Alcotest.check(eval_result_testable, "true", Eval.Value(h_true), Eval.eval(~store, ~att, h_true));
  let h_false = Store.ingest(store, Ast.False);
  Alcotest.check(eval_result_testable, "false", Eval.Value(h_false), Eval.eval(~store, ~att, h_false));
};

let test_eval_zero = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, Ast.Zero);
  Alcotest.check(eval_result_testable, "0", Eval.Value(h), Eval.eval(~store, ~att, h));
};

let test_eval_succ = () => {
  let (_, _, h, result) = eval_ast("succ (succ 0)");
  Alcotest.check(eval_result_testable, "succ (succ 0) ⇒ itself", Eval.Value(h), result);
};

let test_eval_pred = () => {
  let (store, _, _, result) = eval_ast("pred (succ (succ 0))");
  let h_succ_0 = Store.ingest(store, parse("succ 0"));
  Alcotest.check(eval_result_testable, "pred (succ (succ 0)) ⇒ succ 0", Eval.Value(h_succ_0), result);
};

let test_eval_pred_zero = () => {
  let (store, _, _, result) = eval_ast("pred 0");
  let h_zero = Store.ingest(store, Ast.Zero);
  Alcotest.check(eval_result_testable, "pred 0 ⇒ 0", Eval.Value(h_zero), result);
};

let test_eval_iszero = () => {
  let (store, _, _, r_true) = eval_ast("iszero 0");
  let h_true = Store.ingest(store, Ast.True);
  Alcotest.check(eval_result_testable, "iszero 0 ⇒ true", Eval.Value(h_true), r_true);
  let (store2, _, _, r_false) = eval_ast("iszero (succ 0)");
  let h_false = Store.ingest(store2, Ast.False);
  Alcotest.check(eval_result_testable, "iszero (succ 0) ⇒ false", Eval.Value(h_false), r_false);
};

let test_eval_if = () => {
  let (store, _, _, result) = eval_ast("if iszero (pred (succ 0)) then succ 0 else 0");
  let h_succ_0 = Store.ingest(store, parse("succ 0"));
  Alcotest.check(eval_result_testable, "if branch picks succ 0", Eval.Value(h_succ_0), result);
};

/* ===== Stuck terms ===== */

let is_stuck =
  fun
  | Eval.Stuck(_) => true
  | _ => false;

let test_stuck_terms = () => {
  let cases = ["succ true", "pred false", "iszero true", "if 0 then true else false"];
  List.iter(
    src => {
      let (_, _, _, r) = eval_ast(src);
      Alcotest.(check(bool))(src ++ " stuck", true, is_stuck(r));
    },
    cases,
  );
};

let test_stuck_cached = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse("succ true"));
  let r1 = Eval.eval(~store, ~att, h);
  Alcotest.(check(bool))("first eval stuck", true, is_stuck(r1));
  Attachment.reset_counters(att);
  let r2 = Eval.eval(~store, ~att, h);
  Alcotest.(check(bool))("second eval still stuck", true, is_stuck(r2));
  let s = Attachment.stats(att);
  Alcotest.(check(int))("stuck result cached (>=1 hit)", 1, s.hits);
};

/* ===== Cache hit semantics ===== */

let test_repeated_eval_hits_cache = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse("succ (succ 0)"));
  let _ = Eval.eval(~store, ~att, h);
  Attachment.reset_counters(att);
  let _ = Eval.eval(~store, ~att, h);
  let s = Attachment.stats(att);
  Alcotest.(check(int))("second eval is a single cache hit", 1, s.hits);
  Alcotest.(check(int))("no additional misses", 0, s.misses);
};

let test_cross_expression_cache_hit = () => {
  let (store, att) = fresh_world();
  /* Evaluate pred (succ (succ 0)) — produces succ 0 as the value. */
  let h_outer = Store.ingest(store, parse("pred (succ (succ 0))"));
  let _ = Eval.eval(~store, ~att, h_outer);
  /* Now evaluate succ 0 directly. The eval cache should have attached
     Eval_value(h_succ_0) on h_succ_0 during the pred eval (self-eval on
     the returned value node). */
  Attachment.reset_counters(att);
  let h_succ_0 = Store.ingest(store, parse("succ 0"));
  let _ = Eval.eval(~store, ~att, h_succ_0);
  let s = Attachment.stats(att);
  Alcotest.(check(int))("succ 0 eval hits cache", 1, s.hits);
  Alcotest.(check(int))("succ 0 eval does not miss", 0, s.misses);
};

/* ===== Attachment: bidirectional query and derived immutability ===== */

let test_by_value_query = () => {
  let (store, att) = fresh_world();
  let h_outer = Store.ingest(store, parse("pred (succ (succ 0))"));
  let _ = Eval.eval(~store, ~att, h_outer);
  let h_succ_0 = Store.ingest(store, parse("succ 0"));
  let targets =
    Attachment.by_value(
      att,
      ~aspect=Eval.aspect_id,
      ~procedure=Eval.procedure_id,
      Attachment.Eval_value(h_succ_0),
    );
  Alcotest.(check(bool))(
    "h_outer evaluates to succ 0 (appears in by_value)",
    true,
    List.exists(h => Hash.equal(h, h_outer), targets),
  );
  Alcotest.(check(bool))(
    "h_succ_0 self-evaluates to itself (appears in by_value)",
    true,
    List.exists(h => Hash.equal(h, h_succ_0), targets),
  );
};

let test_derived_immutability = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, Ast.Zero);
  let other = Store.ingest(store, Ast.True);
  Attachment.attach(
    att,
    ~target=h,
    ~aspect=Eval.aspect_id,
    ~procedure=Eval.procedure_id,
    Attachment.Eval_value(h),
  );
  /* Second attach with a different value is a no-op for derived aspects. */
  Attachment.attach(
    att,
    ~target=h,
    ~aspect=Eval.aspect_id,
    ~procedure=Eval.procedure_id,
    Attachment.Eval_value(other),
  );
  let v =
    Attachment.peek(att, ~target=h, ~aspect=Eval.aspect_id, ~procedure=Eval.procedure_id);
  switch (v) {
  | Some(Attachment.Eval_value(got)) =>
    Alcotest.(check(bool))("first write wins", true, Hash.equal(got, h))
  | _ => Alcotest.fail("expected Eval_value")
  };
};

/* ===== Runner ===== */

let () =
  Alcotest.run(
    "p2-structural-sharing",
    [
      (
        "parser",
        [
          Alcotest.test_case("atoms", `Quick, test_parse_atoms),
          Alcotest.test_case("nested", `Quick, test_parse_nested),
          Alcotest.test_case("if", `Quick, test_parse_if),
        ],
      ),
      (
        "ingest",
        List.map(
          QCheck_alcotest.to_alcotest,
          [
            prop_ingest_reconstruct_roundtrip,
            prop_ingest_determinism,
            prop_ingest_structural_equality,
          ],
        ),
      ),
      (
        "sharing",
        [
          Alcotest.test_case("subterm sharing", `Quick, test_subterm_sharing),
          Alcotest.test_case("shared across registrations", `Quick, test_shared_across_registrations),
        ],
      ),
      (
        "eval",
        [
          Alcotest.test_case("booleans", `Quick, test_eval_booleans),
          Alcotest.test_case("zero", `Quick, test_eval_zero),
          Alcotest.test_case("succ", `Quick, test_eval_succ),
          Alcotest.test_case("pred", `Quick, test_eval_pred),
          Alcotest.test_case("pred 0", `Quick, test_eval_pred_zero),
          Alcotest.test_case("iszero", `Quick, test_eval_iszero),
          Alcotest.test_case("if", `Quick, test_eval_if),
          Alcotest.test_case("stuck terms", `Quick, test_stuck_terms),
          Alcotest.test_case("stuck cached", `Quick, test_stuck_cached),
        ],
      ),
      (
        "cache",
        [
          Alcotest.test_case("repeated eval hits", `Quick, test_repeated_eval_hits_cache),
          Alcotest.test_case("cross-expression hit", `Quick, test_cross_expression_cache_hit),
        ],
      ),
      (
        "attachment",
        [
          Alcotest.test_case("by_value query", `Quick, test_by_value_query),
          Alcotest.test_case("derived immutability", `Quick, test_derived_immutability),
        ],
      ),
    ],
  );
