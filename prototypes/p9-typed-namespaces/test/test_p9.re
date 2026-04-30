open P9_typed_namespaces_substrate;

/* ==================== Helpers ==================== */

let parse = Parse_recover.parse;

let make_substrate = () => {
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Typecheck.descriptor);
  Attachment.register_descriptor(att, Has_holes.descriptor);
  Attachment.register_descriptor(att, Eval.descriptor);
  let ns = Namespace.create();
  (store, att, ns);
};

let ingest = (~ns, ~store, ~att, src: string): result(Resolver.ingest_ok, Resolver.error) => {
  let surface = parse(src);
  Resolver.ingest(~namespace=ns, ~store, ~att, surface);
};

let must_ingest = (~ns, ~store, ~att, src) =>
  switch (ingest(~ns, ~store, ~att, src)) {
  | Ok(r) => r
  | Error(e) =>
    Alcotest.failf("must_ingest(%s) => %s", src, Resolver.error_to_string(e))
  };

let must_eval = (~store, ~att, h): Hash.t =>
  switch (Eval.eval(~store, ~att, h)) {
  | Eval.Value(h) => h
  | Eval.Stuck(h) =>
    Alcotest.failf("eval stuck at %s", Hash.short(h))
  | Eval.StepLimit(_) => Alcotest.fail("eval step limit hit")
  };

let int_value = (~store, ~att, src: string, expected: int) => {
  let (store_, att_, ns) = (store, att, Namespace.create());
  let r = must_ingest(~ns, ~store=store_, ~att=att_, src);
  let v = must_eval(~store=store_, ~att=att_, r.hash);
  switch (Store.lookup(store_, v)) {
  | Some(Node.Int_lit(n)) =>
    Alcotest.(check(int))("int " ++ src, expected, n)
  | _ =>
    Alcotest.failf("expected Int_lit(%d) for %s", expected, src)
  };
};

let bool_value = (~store, ~att, src: string, expected: bool) => {
  let r = {
    let ns = Namespace.create();
    must_ingest(~ns, ~store, ~att, src);
  };
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup(store, v)) {
  | Some(Node.Bool_lit(b)) =>
    Alcotest.(check(bool))("bool " ++ src, expected, b)
  | _ =>
    Alcotest.failf("expected Bool_lit(%b) for %s", expected, src)
  };
};

let string_value = (~store, ~att, src: string, expected: string) => {
  let r = {
    let ns = Namespace.create();
    must_ingest(~ns, ~store, ~att, src);
  };
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup(store, v)) {
  | Some(Node.String_lit(s)) =>
    Alcotest.(check(string))("string " ++ src, expected, s)
  | _ =>
    Alcotest.failf("expected String_lit(%S) for %s", expected, src)
  };
};

/* ==================== 1. Round-trip ==================== */

let test_roundtrip = () => {
  let (store, att, ns) = make_substrate();
  let src = "\\x: Int. x + 1";
  let r = must_ingest(~ns, ~store, ~att, src);
  let printed = Pretty.print_named(~namespace=ns, store, r.hash);
  let r2 = must_ingest(~ns, ~store, ~att, printed);
  Alcotest.(check(string))("roundtrip hash equal", r.hash, r2.hash);
};

let test_roundtrip_let = () => {
  let (store, att, ns) = make_substrate();
  let src = "let f = \\x: Int. x + 1 in f 41";
  let r = must_ingest(~ns, ~store, ~att, src);
  let printed = Pretty.print_named(~namespace=ns, store, r.hash);
  let r2 = must_ingest(~ns, ~store, ~att, printed);
  Alcotest.(check(string))("roundtrip let hash equal", r.hash, r2.hash);
};

/* ==================== 2. Hole hash stability ==================== */

let test_hole_hash_stable = () => {
  let (store, att, ns) = make_substrate();
  let h1 = (must_ingest(~ns, ~store, ~att, "?")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "?")).hash;
  Alcotest.(check(string))("hole hashes equal", h1, h2);
};

let test_hole_under_lambda_different_types = () => {
  let (store, att, ns) = make_substrate();
  let h1 = (must_ingest(~ns, ~store, ~att, "\\x: Int. ?")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "\\y: Bool. ?")).hash;
  Alcotest.(check(neg(string)))(
    "lambda over hole differs by annotation",
    h1,
    h2,
  );
};

let test_alpha_equivalent_lambdas = () => {
  let (store, att, ns) = make_substrate();
  let h1 = (must_ingest(~ns, ~store, ~att, "\\x: Int. x")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "\\y: Int. y")).hash;
  Alcotest.(check(string))("alpha-equivalent lambdas share hash", h1, h2);
};

let test_alpha_equivalent_lets = () => {
  let (store, att, ns) = make_substrate();
  let h1 = (must_ingest(~ns, ~store, ~att, "let x = 1 in x")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "let y = 1 in y")).hash;
  Alcotest.(check(string))("alpha-equivalent lets share hash", h1, h2);
};

/* ==================== 3-5. Suffix resolution ==================== */

let test_suffix_resolves = () => {
  let (store, att, ns) = make_substrate();
  let added = (must_ingest(~ns, ~store, ~att, "\\x: Int. \\y: Int. x + y")).hash;
  Namespace.bind(ns, ~name="math.add", added);
  let ingested = must_ingest(~ns, ~store, ~att, "add 1 2");
  let v = must_eval(~store, ~att, ingested.hash);
  switch (Store.lookup(store, v)) {
  | Some(Node.Int_lit(3)) => ()
  | _ => Alcotest.fail("expected eval to 3")
  };
};

let test_suffix_ambiguous = () => {
  let (store, att, ns) = make_substrate();
  let h = (must_ingest(~ns, ~store, ~att, "\\x: Int. \\y: Int. x + y")).hash;
  Namespace.bind(ns, ~name="math.add", h);
  Namespace.bind(ns, ~name="vector.add", h);
  let surface = parse("add 1 2");
  switch (Resolver.ingest(~namespace=ns, ~store, ~att, surface)) {
  | Error(Resolver.Ambiguous_name("add", candidates)) =>
    Alcotest.(check(int))(
      "two candidates",
      2,
      List.length(candidates),
    )
  | Error(e) =>
    Alcotest.failf(
      "expected Ambiguous, got %s",
      Resolver.error_to_string(e),
    )
  | Ok(_) => Alcotest.fail("expected Ambiguous, got Ok")
  };
};

let test_suffix_segment_bounded = () => {
  let (_, _, ns) = make_substrate();
  let dummy = "";
  Namespace.bind(ns, ~name="add", dummy);
  switch (Namespace.resolve_query(ns, "dd")) {
  | Error(Namespace.Unbound) => ()
  | _ => Alcotest.fail("expected Unbound for partial-segment match")
  };
};

let test_full_path_resolves = () => {
  let (store, att, ns) = make_substrate();
  let h = (must_ingest(~ns, ~store, ~att, "\\x: Int. x + 1")).hash;
  Namespace.bind(ns, ~name="math.inc", h);
  let r = must_ingest(~ns, ~store, ~att, "math.inc 41");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup(store, v)) {
  | Some(Node.Int_lit(42)) => ()
  | _ => Alcotest.fail("expected eval to 42")
  };
};

/* ==================== 6-7. Has-holes ==================== */

let test_has_holes_root = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int. ?");
  Alcotest.(check(bool))("has holes", true, r.has_holes);
};

let test_has_holes_no_holes = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int. x + 1");
  Alcotest.(check(bool))("no holes", false, r.has_holes);
};

let test_has_holes_cached = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int. ?");
  Attachment.reset_counters(att);
  let _: bool = Has_holes.compute(~store, ~att, r.hash);
  /* Cached path: peek_cache has no get/miss accounting, so we verify
     by checking that the second compute returns same answer immediately
     without raising. The peek_cache(...) Some path is the cache hit. */
  let again = Has_holes.compute(~store, ~att, r.hash);
  Alcotest.(check(bool))("cached has-holes consistent", true, again);
};

/* ==================== 8-10. Typecheck ==================== */

let test_permissive_hole_in_arith = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int. ? + x");
  switch (r.type_result) {
  | Typecheck.Well_typed_with_holes(Ty.Arrow(Ty.Int, Ty.Int)) => ()
  | other =>
    Alcotest.failf(
      "expected Well_typed_with_holes(Int -> Int), got something else (%b)",
      switch (other) {
      | Typecheck.Well_typed(_) => true
      | _ => false
      },
    )
  };
};

let test_strict_reject_prim_mismatch = () => {
  let (store, att, ns) = make_substrate();
  let surface = parse("1 + true");
  switch (Resolver.ingest(~namespace=ns, ~store, ~att, surface)) {
  | Error(Resolver.Type_error(_)) => ()
  | _ => Alcotest.fail("expected Type_error for 1 + true")
  };
};

let test_strict_reject_app_mismatch = () => {
  let (store, att, ns) = make_substrate();
  let surface = parse("(\\x: Int. x) \"abc\"");
  switch (Resolver.ingest(~namespace=ns, ~store, ~att, surface)) {
  | Error(Resolver.Type_error(_)) => ()
  | _ =>
    Alcotest.fail("expected Type_error for (\\x:Int. x) \"abc\"")
  };
};

/* ==================== 11. Pair eval ==================== */

let test_pair_fst = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "fst (1, 2)");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup(store, v)) {
  | Some(Node.Int_lit(1)) => ()
  | _ => Alcotest.fail("expected fst (1,2) = 1")
  };
};

let test_pair_snd = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "snd (true, \"x\")");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup(store, v)) {
  | Some(Node.String_lit("x")) => ()
  | _ => Alcotest.fail("expected snd (true, \"x\") = \"x\"")
  };
};

/* ==================== 12. Prim eval ==================== */

let test_prim_arith = () => {
  let (store, att, _) = make_substrate();
  int_value(~store, ~att, "1 + 2", 3);
  int_value(~store, ~att, "10 - 7", 3);
  int_value(~store, ~att, "3 mul 4", 12);
  int_value(~store, ~att, "10 / 3", 3);
  int_value(~store, ~att, "10 mod 3", 1);
};

let test_prim_bool = () => {
  let (store, att, _) = make_substrate();
  bool_value(~store, ~att, "true && false", false);
  bool_value(~store, ~att, "true || false", true);
  bool_value(~store, ~att, "not true", false);
};

let test_prim_concat = () => {
  let (store, att, _) = make_substrate();
  string_value(~store, ~att, "\"a\" ++ \"b\"", "ab");
};

let test_prim_eq = () => {
  let (store, att, _) = make_substrate();
  bool_value(~store, ~att, "1 == 1", true);
  bool_value(~store, ~att, "1 == 2", false);
  bool_value(~store, ~att, "\"hi\" == \"hi\"", true);
};

/* ==================== 13. Let eval ==================== */

let test_let = () => {
  let (store, att, _) = make_substrate();
  int_value(~store, ~att, "let x = 1 + 1 in x mul x", 4);
};

let test_let_pair = () => {
  let (store, att, _) = make_substrate();
  int_value(~store, ~att, "let p = (1, 2) in fst p", 1);
};

/* ==================== 14. Parser totality ==================== */

let total_parse_printable = () =>
  QCheck.Test.make(
    ~count=1000,
    ~name="parse total on printable strings",
    QCheck.string_printable,
    s => {
      let _: Surface_ast.t = parse(s);
      true;
    },
  );

let total_parse_arbitrary = () =>
  QCheck.Test.make(
    ~count=500,
    ~name="parse total on arbitrary bytes",
    QCheck.string,
    s => {
      let _: Surface_ast.t = parse(s);
      true;
    },
  );

/* ==================== 15. Print/parse idempotence on clean inputs ==================== */

let test_print_parse_idempotent = () => {
  let cases = [
    "1",
    "true",
    "\"hello\"",
    "1 + 2",
    "\\x: Int. x + 1",
    "let x = 1 in x",
    "if true then 1 else 2",
    "(1, 2)",
    "fst (1, 2)",
    "1 == 2",
    "\"a\" ++ \"b\"",
  ];
  let (store, att, ns) = make_substrate();
  List.iter(
    src => {
      let r = must_ingest(~ns, ~store, ~att, src);
      let printed = Pretty.print_named(~namespace=ns, store, r.hash);
      let r2 = must_ingest(~ns, ~store, ~att, printed);
      Alcotest.(check(string))(
        "idempotent: " ++ src ++ " => " ++ printed,
        r.hash,
        r2.hash,
      );
    },
    cases,
  );
};

/* ==================== 16. Reserved keywords cannot bind ==================== */

let test_reserved_keyword_bind_fails = () => {
  let (_, _, ns) = make_substrate();
  Alcotest.check_raises(
    "binding 'let' raises",
    Namespace.Name_reserved("let"),
    () => Namespace.bind(ns, ~name="let", ""),
  );
  Alcotest.check_raises(
    "binding 'Bool' raises",
    Namespace.Name_reserved("Bool"),
    () => Namespace.bind(ns, ~name="Bool", ""),
  );
  Alcotest.check_raises(
    "dotted segment 'foo.if' raises",
    Namespace.Name_reserved("foo.if"),
    () => Namespace.bind(ns, ~name="foo.if", ""),
  );
};

/* ==================== 17. Dotted bind/rebind/unbind ==================== */

let test_dotted_bind_rebind_unbind = () => {
  let (store, att, ns) = make_substrate();
  let h1 = (must_ingest(~ns, ~store, ~att, "1")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "2")).hash;
  Namespace.bind(ns, ~name="data.x", h1);
  Alcotest.(check(option(string)))(
    "bound to h1",
    Some(h1),
    Namespace.resolve(ns, "data.x"),
  );
  Namespace.rebind(ns, ~name="data.x", h2);
  Alcotest.(check(option(string)))(
    "rebound to h2",
    Some(h2),
    Namespace.resolve(ns, "data.x"),
  );
  Alcotest.(check(bool))(
    "unbind succeeds",
    true,
    Namespace.unbind(ns, ~name="data.x"),
  );
  Alcotest.(check(option(string)))(
    "unbound now",
    None,
    Namespace.resolve(ns, "data.x"),
  );
};

/* ==================== Recovered AST output ==================== */

let test_recovery_truncated_let = () => {
  let surface = parse("let x =");
  switch (surface) {
  | Surface_ast.Let("x", Surface_ast.Hole, Surface_ast.Hole) => ()
  | _ =>
    Alcotest.failf(
      "expected Let(x, Hole, Hole), got %s",
      Pretty.print_surface(surface),
    )
  };
};

let test_recovery_truncated_if = () => {
  let surface = parse("if 1 == 1");
  switch (surface) {
  | Surface_ast.If(_, Surface_ast.Hole, Surface_ast.Hole) => ()
  | _ =>
    Alcotest.failf(
      "expected If(_, Hole, Hole), got %s",
      Pretty.print_surface(surface),
    )
  };
};

let test_recovery_truncated_lambda = () => {
  let surface = parse("\\x: Int.");
  switch (surface) {
  | Surface_ast.Lam("x", Ty.Int, Surface_ast.Hole) => ()
  | _ =>
    Alcotest.failf(
      "expected Lam(x, Int, Hole), got %s",
      Pretty.print_surface(surface),
    )
  };
};

let test_recovery_garbage_to_hole = () => {
  let surface = parse("}}}");
  /* Garbage tokens should at least not raise; the surface may be Hole
     or a partially-recovered shape. The contract is "doesn't raise". */
  let _: int = Surface_ast.count_holes(surface);
  ();
};

/* ==================== Test registration ==================== */

let () =
  Alcotest.run(
    "p9-typed-namespaces",
    [
      (
        "roundtrip",
        [
          Alcotest.test_case("simple", `Quick, test_roundtrip),
          Alcotest.test_case("let", `Quick, test_roundtrip_let),
          Alcotest.test_case("print/parse idempotent", `Quick, test_print_parse_idempotent),
        ],
      ),
      (
        "holes",
        [
          Alcotest.test_case("hole hash stable", `Quick, test_hole_hash_stable),
          Alcotest.test_case("hole annot differs", `Quick, test_hole_under_lambda_different_types),
          Alcotest.test_case("alpha lams share hash", `Quick, test_alpha_equivalent_lambdas),
          Alcotest.test_case("alpha lets share hash", `Quick, test_alpha_equivalent_lets),
        ],
      ),
      (
        "namespace",
        [
          Alcotest.test_case("suffix resolves", `Quick, test_suffix_resolves),
          Alcotest.test_case("suffix ambiguous", `Quick, test_suffix_ambiguous),
          Alcotest.test_case("suffix segment-bounded", `Quick, test_suffix_segment_bounded),
          Alcotest.test_case("full path resolves", `Quick, test_full_path_resolves),
          Alcotest.test_case("reserved keyword fails", `Quick, test_reserved_keyword_bind_fails),
          Alcotest.test_case("dotted bind/rebind/unbind", `Quick, test_dotted_bind_rebind_unbind),
        ],
      ),
      (
        "has-holes",
        [
          Alcotest.test_case("root flagged", `Quick, test_has_holes_root),
          Alcotest.test_case("no holes flagged false", `Quick, test_has_holes_no_holes),
          Alcotest.test_case("cached", `Quick, test_has_holes_cached),
        ],
      ),
      (
        "typecheck",
        [
          Alcotest.test_case("permissive hole in arith", `Quick, test_permissive_hole_in_arith),
          Alcotest.test_case("strict reject prim", `Quick, test_strict_reject_prim_mismatch),
          Alcotest.test_case("strict reject app", `Quick, test_strict_reject_app_mismatch),
        ],
      ),
      (
        "eval",
        [
          Alcotest.test_case("pair fst", `Quick, test_pair_fst),
          Alcotest.test_case("pair snd", `Quick, test_pair_snd),
          Alcotest.test_case("arith", `Quick, test_prim_arith),
          Alcotest.test_case("bool", `Quick, test_prim_bool),
          Alcotest.test_case("concat", `Quick, test_prim_concat),
          Alcotest.test_case("eq", `Quick, test_prim_eq),
          Alcotest.test_case("let", `Quick, test_let),
          Alcotest.test_case("let with pair", `Quick, test_let_pair),
        ],
      ),
      (
        "recovery",
        [
          Alcotest.test_case("truncated let", `Quick, test_recovery_truncated_let),
          Alcotest.test_case("truncated if", `Quick, test_recovery_truncated_if),
          Alcotest.test_case("truncated lambda", `Quick, test_recovery_truncated_lambda),
          Alcotest.test_case("garbage to hole", `Quick, test_recovery_garbage_to_hole),
          QCheck_alcotest.to_alcotest(total_parse_printable()),
          QCheck_alcotest.to_alcotest(total_parse_arbitrary()),
        ],
      ),
    ],
  );
