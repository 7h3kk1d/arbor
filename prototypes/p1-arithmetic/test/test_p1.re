open P1_arithmetic;

let parse = (s: string): Ast.t => {
  let lexbuf = Lexing.from_string(s);
  Parser.main(Lexer.token, lexbuf);
};

/* Alcotest testable for Ast.t. */
let ast_testable =
  Alcotest.testable((fmt, t) => Format.fprintf(fmt, "%s", Pretty.print(t)), Ast.equal);

let eval_testable =
  Alcotest.testable(
    (fmt, r) => Format.fprintf(fmt, "%s", Eval.print_result(r)),
    (a, b) =>
      switch (a, b) {
      | (Eval.Value(x), Eval.Value(y)) => Ast.equal(x, y)
      | (Eval.Stuck(x), Eval.Stuck(y)) => Ast.equal(x, y)
      | _ => false
      },
  );

/* ===== Parser ===== */

let test_parse_atoms = () => {
  Alcotest.check(ast_testable, "true", Ast.True, parse("true"));
  Alcotest.check(ast_testable, "false", Ast.False, parse("false"));
  Alcotest.check(ast_testable, "0", Ast.Zero, parse("0"));
};

let test_parse_succ_nested = () =>
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

let test_parse_whitespace = () =>
  Alcotest.check(
    ast_testable,
    "extra whitespace tolerated",
    Ast.Succ(Ast.Zero),
    parse("  succ   0  "),
  );

/* ===== Pretty-printer roundtrip ===== */

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
      (
        2,
        map3((c, t, e) => Ast.If(c, t, e), sub, sub, sub),
      ),
    ]);
  };
};

let arith_ast_arb = QCheck.make(~print=Pretty.print, arith_ast_gen(4));

let prop_pretty_parse_roundtrip =
  QCheck.Test.make(
    ~count=200,
    ~name="pretty-print then parse recovers the AST",
    arith_ast_arb,
    t => Ast.equal(t, parse(Pretty.print(t))),
  );

/* ===== Hash determinism ===== */

let prop_hash_determinism =
  QCheck.Test.make(
    ~count=200,
    ~name="same AST hashes to the same digest",
    arith_ast_arb,
    t => Hash.equal(Hash.of_ast(t), Hash.of_ast(t)),
  );

let prop_hash_structural_equality =
  QCheck.Test.make(
    ~count=200,
    ~name="equal ASTs have equal hashes",
    QCheck.pair(arith_ast_arb, arith_ast_arb),
    ((a, b)) =>
      if (Ast.equal(a, b)) {
        Hash.equal(Hash.of_ast(a), Hash.of_ast(b));
      } else {
        true;
      },
  );

let test_hash_distinguishes_simple_pairs = () => {
  let distinct_pairs = [
    (Ast.True, Ast.False),
    (Ast.Zero, Ast.Succ(Ast.Zero)),
    (Ast.Succ(Ast.Zero), Ast.Succ(Ast.Succ(Ast.Zero))),
    (Ast.IsZero(Ast.Zero), Ast.IsZero(Ast.Succ(Ast.Zero))),
  ];
  List.iter(
    ((a, b)) =>
      Alcotest.(check(bool))(
        Printf.sprintf("%s ≠ %s", Pretty.print(a), Pretty.print(b)),
        false,
        Hash.equal(Hash.of_ast(a), Hash.of_ast(b)),
      ),
    distinct_pairs,
  );
};

/* ===== Evaluator: per-constructor correctness ===== */

let v = x => Eval.Value(x);

let test_eval_booleans = () => {
  Alcotest.check(eval_testable, "true", v(Ast.True), Eval.eval(Ast.True));
  Alcotest.check(eval_testable, "false", v(Ast.False), Eval.eval(Ast.False));
};

let test_eval_zero = () =>
  Alcotest.check(eval_testable, "0", v(Ast.Zero), Eval.eval(Ast.Zero));

let test_eval_succ_numeric = () =>
  Alcotest.check(
    eval_testable,
    "succ (succ 0) ⇒ succ (succ 0)",
    v(Ast.Succ(Ast.Succ(Ast.Zero))),
    Eval.eval(parse("succ (succ 0)")),
  );

let test_eval_pred = () => {
  Alcotest.check(eval_testable, "pred 0 ⇒ 0", v(Ast.Zero), Eval.eval(parse("pred 0")));
  Alcotest.check(
    eval_testable,
    "pred (succ (succ 0)) ⇒ succ 0",
    v(Ast.Succ(Ast.Zero)),
    Eval.eval(parse("pred (succ (succ 0))")),
  );
};

let test_eval_iszero = () => {
  Alcotest.check(eval_testable, "iszero 0 ⇒ true", v(Ast.True), Eval.eval(parse("iszero 0")));
  Alcotest.check(
    eval_testable,
    "iszero (succ 0) ⇒ false",
    v(Ast.False),
    Eval.eval(parse("iszero (succ 0)")),
  );
};

let test_eval_if = () => {
  Alcotest.check(
    eval_testable,
    "if true then 0 else succ 0",
    v(Ast.Zero),
    Eval.eval(parse("if true then 0 else succ 0")),
  );
  Alcotest.check(
    eval_testable,
    "if iszero (pred (succ 0)) then succ 0 else 0",
    v(Ast.Succ(Ast.Zero)),
    Eval.eval(parse("if iszero (pred (succ 0)) then succ 0 else 0")),
  );
};

/* ===== Stuck terms ===== */

let is_stuck =
  fun
  | Eval.Stuck(_) => true
  | _ => false;

let test_eval_stuck = () => {
  let stuck_examples = [
    "succ true",
    "pred false",
    "iszero true",
    "if 0 then true else false",
  ];
  List.iter(
    src =>
      Alcotest.(check(bool))(
        src ++ " should be stuck",
        true,
        is_stuck(Eval.eval(parse(src))),
      ),
    stuck_examples,
  );
};

/* ===== Store ===== */

let test_store_roundtrip = () => {
  let store = Store.create();
  let ast = parse("succ (succ 0)");
  let h = Store.register(store, ast);
  Alcotest.check(
    Alcotest.option(ast_testable),
    "lookup(register(d)) = Some d",
    Some(ast),
    Store.lookup(store, h),
  );
};

let test_store_idempotent = () => {
  let store = Store.create();
  let ast = parse("iszero (pred (succ 0))");
  let h1 = Store.register(store, ast);
  let h2 = Store.register(store, ast);
  Alcotest.(check(bool))("same hash on re-register", true, Hash.equal(h1, h2));
  Alcotest.(check(int))("no duplicate entry", 1, List.length(Store.entries(store)));
};

let test_store_prefix_lookup = () => {
  let store = Store.create();
  let ast1 = parse("succ 0");
  let ast2 = parse("succ (succ 0)");
  let h1 = Store.register(store, ast1);
  let _h2 = Store.register(store, ast2);
  switch (Store.resolve_prefix(store, String.sub(h1, 0, 8))) {
  | Hash.Found(h) =>
    Alcotest.(check(bool))("prefix resolves to exact hash", true, Hash.equal(h, h1))
  | _ => Alcotest.fail("expected Found")
  };
};

/* ===== Runner ===== */

let () =
  Alcotest.run(
    "p1-arithmetic",
    [
      (
        "parser",
        [
          Alcotest.test_case("atoms", `Quick, test_parse_atoms),
          Alcotest.test_case("nested succ", `Quick, test_parse_succ_nested),
          Alcotest.test_case("if", `Quick, test_parse_if),
          Alcotest.test_case("whitespace", `Quick, test_parse_whitespace),
        ],
      ),
      (
        "roundtrip",
        List.map(QCheck_alcotest.to_alcotest, [prop_pretty_parse_roundtrip]),
      ),
      (
        "hash",
        [
          Alcotest.test_case(
            "distinguishes simple pairs",
            `Quick,
            test_hash_distinguishes_simple_pairs,
          ),
          ...List.map(
               QCheck_alcotest.to_alcotest,
               [prop_hash_determinism, prop_hash_structural_equality],
             ),
        ],
      ),
      (
        "eval",
        [
          Alcotest.test_case("booleans", `Quick, test_eval_booleans),
          Alcotest.test_case("zero", `Quick, test_eval_zero),
          Alcotest.test_case("succ", `Quick, test_eval_succ_numeric),
          Alcotest.test_case("pred", `Quick, test_eval_pred),
          Alcotest.test_case("iszero", `Quick, test_eval_iszero),
          Alcotest.test_case("if", `Quick, test_eval_if),
          Alcotest.test_case("stuck terms", `Quick, test_eval_stuck),
        ],
      ),
      (
        "store",
        [
          Alcotest.test_case("register/lookup roundtrip", `Quick, test_store_roundtrip),
          Alcotest.test_case("idempotent register", `Quick, test_store_idempotent),
          Alcotest.test_case("prefix lookup", `Quick, test_store_prefix_lookup),
        ],
      ),
    ],
  );
