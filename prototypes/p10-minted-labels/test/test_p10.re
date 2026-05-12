open P10_minted_labels_substrate;

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
  switch (Store.lookup_term(store_, v)) {
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
  switch (Store.lookup_term(store, v)) {
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
  switch (Store.lookup_term(store, v)) {
  | Some(Node.String_lit(s)) =>
    Alcotest.(check(string))("string " ++ src, expected, s)
  | _ =>
    Alcotest.failf("expected String_lit(%S) for %s", expected, src)
  };
};

/* ==================== 1. Round-trip ==================== */

let test_roundtrip = () => {
  let (store, att, ns) = make_substrate();
  let src = "\\x: Int => x + 1";
  let r = must_ingest(~ns, ~store, ~att, src);
  let printed = Pretty.print_named(~namespace=ns, store, r.hash);
  let r2 = must_ingest(~ns, ~store, ~att, printed);
  Alcotest.(check(string))("roundtrip hash equal", r.hash, r2.hash);
};

let test_roundtrip_let = () => {
  let (store, att, ns) = make_substrate();
  let src = "let f = \\x: Int => x + 1 in f 41";
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
  let h1 = (must_ingest(~ns, ~store, ~att, "\\x: Int => ?")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "\\y: Bool => ?")).hash;
  Alcotest.(check(neg(string)))(
    "lambda over hole differs by annotation",
    h1,
    h2,
  );
};

let test_alpha_equivalent_lambdas = () => {
  let (store, att, ns) = make_substrate();
  let h1 = (must_ingest(~ns, ~store, ~att, "\\x: Int => x")).hash;
  let h2 = (must_ingest(~ns, ~store, ~att, "\\y: Int => y")).hash;
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
  let added = (must_ingest(~ns, ~store, ~att, "\\x: Int => \\y: Int => x + y")).hash;
  Namespace.bind(ns, ~name="math.add", added);
  let ingested = must_ingest(~ns, ~store, ~att, "add 1 2");
  let v = must_eval(~store, ~att, ingested.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(3)) => ()
  | _ => Alcotest.fail("expected eval to 3")
  };
};

let test_suffix_ambiguous = () => {
  let (store, att, ns) = make_substrate();
  let h = (must_ingest(~ns, ~store, ~att, "\\x: Int => \\y: Int => x + y")).hash;
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
  let h = (must_ingest(~ns, ~store, ~att, "\\x: Int => x + 1")).hash;
  Namespace.bind(ns, ~name="math.inc", h);
  let r = must_ingest(~ns, ~store, ~att, "math.inc 41");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(42)) => ()
  | _ => Alcotest.fail("expected eval to 42")
  };
};

/* ==================== 6-7. Has-holes ==================== */

let test_has_holes_root = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int => ?");
  Alcotest.(check(bool))("has holes", true, r.has_holes);
};

let test_has_holes_no_holes = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int => x + 1");
  Alcotest.(check(bool))("no holes", false, r.has_holes);
};

let test_has_holes_cached = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int => ?");
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
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int => ? + x");
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
  let surface = parse("(\\x: Int => x) \"abc\"");
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
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(1)) => ()
  | _ => Alcotest.fail("expected fst (1,2) = 1")
  };
};

let test_pair_snd = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "snd (true, \"x\")");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
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
    "\\x: Int => x + 1",
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
  let surface = parse("\\x: Int =>");
  switch (surface) {
  | Surface_ast.Lam("x", Surface_ty.Int, Surface_ast.Hole) => ()
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

/* ==================== Type bindings & aliasing ==================== */

let ingest_ty = (~ns, ~store, src: string): Resolver.ingest_ty_ok => {
  let surface_ty = Parse_recover.parse_ty(src);
  switch (Resolver.ingest_ty(~namespace=ns, ~store, surface_ty)) {
  | Ok(r) => r
  | Error(e) =>
    Alcotest.failf(
      "ingest_ty(%s) => %s",
      src,
      Resolver.error_to_string(e),
    )
  };
};

/* The aliasing identity: a Lam annotated with a named type produces
   the same Node hash as the same Lam annotated with the underlying
   structural type. This is the core demonstration that names binding
   to type hashes are aliases by content. */
let test_named_type_alias_identity = () => {
  let (store, att, ns) = make_substrate();
  let ty_r = ingest_ty(~ns, ~store, "Int -> Int");
  Namespace.bind(ns, ~name="Endo", ty_r.hash);
  let h_named = (must_ingest(~ns, ~store, ~att, "\\f: Endo => f")).hash;
  let h_struct =
    (must_ingest(~ns, ~store, ~att, "\\f: Int -> Int => f")).hash;
  Alcotest.(check(string))(
    "named alias and structural type produce identical Lam hash",
    h_named,
    h_struct,
  );
};

/* Two distinct alias names binding the same structural type share
   the type-hash, so Lams annotated by either name still match. */
let test_two_aliases_share_hash = () => {
  let (store, att, ns) = make_substrate();
  let r1 = ingest_ty(~ns, ~store, "Int -> Int");
  let r2 = ingest_ty(~ns, ~store, "Int -> Int");
  Alcotest.(check(string))(
    "structurally-equal types share hash",
    r1.hash,
    r2.hash,
  );
  Namespace.bind(ns, ~name="A", r1.hash);
  Namespace.bind(ns, ~name="B", r2.hash);
  let h_a = (must_ingest(~ns, ~store, ~att, "\\f: A => f")).hash;
  let h_b = (must_ingest(~ns, ~store, ~att, "\\f: B => f")).hash;
  Alcotest.(check(string))(
    "aliased annotations share Lam hash",
    h_a,
    h_b,
  );
};

/* Mixed-case dotted names (like the bootstrapped `alias.IntEndo`)
   must lex as a single IDENT so they can be referenced inside a
   Lam annotation. Regression: prior to this fix, the dotted-ident
   regex required lower_ident on both sides of `.`, so an upper-case
   suffix split into multiple tokens and the resolver saw an
   unbound name. */
let test_mixed_case_dotted_name_in_annotation = () => {
  let (store, att, ns) = make_substrate();
  let ty_r = ingest_ty(~ns, ~store, "Int -> Int");
  Namespace.bind(ns, ~name="alias.IntEndo", ty_r.hash);
  let h_named =
    (must_ingest(~ns, ~store, ~att, "\\f: alias.IntEndo => f")).hash;
  let h_struct =
    (must_ingest(~ns, ~store, ~att, "\\f: Int -> Int => f")).hash;
  Alcotest.(check(string))(
    "mixed-case dotted alias resolves the same as structural form",
    h_named,
    h_struct,
  );
};

/* Recursive aliasing: a named type whose body itself uses a named
   type. */
let test_recursive_alias = () => {
  let (store, att, ns) = make_substrate();
  let pair = ingest_ty(~ns, ~store, "Int * Int");
  Namespace.bind(ns, ~name="Pair", pair.hash);
  let endo_pair = ingest_ty(~ns, ~store, "Pair -> Pair");
  Namespace.bind(ns, ~name="EndoPair", endo_pair.hash);
  let h_named = (must_ingest(~ns, ~store, ~att, "\\f: EndoPair => f")).hash;
  let h_struct =
    (must_ingest(~ns, ~store, ~att, "\\f: Int * Int -> Int * Int => f")).hash;
  Alcotest.(check(string))(
    "recursive alias resolves through chain",
    h_named,
    h_struct,
  );
};

/* Kind-mismatch: a term-named binding used in type position. */
let test_term_in_type_position_errors = () => {
  let (store, att, ns) = make_substrate();
  let term = (must_ingest(~ns, ~store, ~att, "1")).hash;
  Namespace.bind(ns, ~name="ONE", term);
  let surface = parse("\\x: ONE => x");
  switch (Resolver.ingest(~namespace=ns, ~store, ~att, surface)) {
  | Error(Resolver.Kind_mismatch({name: "ONE", expected: Definition.Type_kind, got: Definition.Term_kind})) =>
    ()
  | Error(e) =>
    Alcotest.failf(
      "expected Kind_mismatch, got %s",
      Resolver.error_to_string(e),
    )
  | Ok(_) =>
    Alcotest.fail("expected Kind_mismatch when term name in type position")
  };
};

/* Kind-mismatch: a type-named binding used in term position. */
let test_type_in_term_position_errors = () => {
  let (store, att, ns) = make_substrate();
  let ty_r = ingest_ty(~ns, ~store, "Int");
  Namespace.bind(ns, ~name="MyInt", ty_r.hash);
  let surface = parse("MyInt");
  switch (Resolver.ingest(~namespace=ns, ~store, ~att, surface)) {
  | Error(Resolver.Kind_mismatch({name: "MyInt", expected: Definition.Term_kind, got: Definition.Type_kind})) =>
    ()
  | Error(e) =>
    Alcotest.failf(
      "expected Kind_mismatch, got %s",
      Resolver.error_to_string(e),
    )
  | Ok(_) =>
    Alcotest.fail("expected Kind_mismatch when type name in term position")
  };
};

/* The Type_of aspect now stores the type's hash; a typecheck
   peek_cache should reconstruct the same Ty.t out of the Store. */
let test_type_of_aspect_round_trips = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "\\x: Int => x + 1");
  switch (Typecheck.peek_cache(~store, att, r.hash)) {
  | Some(Typecheck.Well_typed(Ty.Arrow(Ty.Int, Ty.Int))) => ()
  | _ =>
    Alcotest.fail(
      "expected Well_typed(Int -> Int) reconstructed from cached type-hash",
    )
  };
};

/* Two terms with the same type share the cached type-hash in
   their respective Type_of aspect entries. */
let test_type_of_aspect_dedups = () => {
  let (store, att, ns) = make_substrate();
  let r1 = must_ingest(~ns, ~store, ~att, "\\x: Int => x + 1");
  let r2 = must_ingest(~ns, ~store, ~att, "\\y: Int => y - 1");
  let ty_h1 =
    switch (
      Attachment.peek(
        att,
        ~target=r1.hash,
        ~aspect=Typecheck.aspect_id,
        ~procedure=Typecheck.procedure_id,
      )
    ) {
    | Some(Attachment.Type_of(h)) => h
    | _ => Alcotest.fail("expected Type_of aspect on r1")
    };
  let ty_h2 =
    switch (
      Attachment.peek(
        att,
        ~target=r2.hash,
        ~aspect=Typecheck.aspect_id,
        ~procedure=Typecheck.procedure_id,
      )
    ) {
    | Some(Attachment.Type_of(h)) => h
    | _ => Alcotest.fail("expected Type_of aspect on r2")
    };
  Alcotest.(check(string))(
    "two Int->Int terms share their type hash",
    ty_h1,
    ty_h2,
  );
};

/* ==================== Primitive operations ==================== */

/* Helper substrate that also installs the canonical default primitive
   set, so tests can exercise the namespace path. */
let make_substrate_with_prims = () => {
  let (store, att, ns) = make_substrate();
  Primitives.install(~store, ~att, ~ns);
  (store, att, ns);
};

/* Calling a registered primitive by namespace name evaluates via the
   registered impl. */
let test_prim_string_length = () => {
  let (store, att, ns) = make_substrate_with_prims();
  let r = must_ingest(~ns, ~store, ~att, "string.length \"hello\"");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(5)) => ()
  | _ => Alcotest.fail("expected length 5")
  };
};

let test_prim_string_reverse = () => {
  let (store, att, ns) = make_substrate_with_prims();
  let r = must_ingest(~ns, ~store, ~att, "string.reverse \"abcd\"");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.String_lit("dcba")) => ()
  | _ => Alcotest.fail("expected reversed string \"dcba\"")
  };
};

let test_prim_substring_ternary = () => {
  let (store, att, ns) = make_substrate_with_prims();
  let r =
    must_ingest(~ns, ~store, ~att, "string.substring \"hello world\" 6 11");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.String_lit("world")) => ()
  | _ => Alcotest.fail("expected substring \"world\"")
  };
};

let test_prim_int_to_string_compose = () => {
  let (store, att, ns) = make_substrate_with_prims();
  /* Compose with the existing string.length primitive: encode an
     integer then measure its string representation. Exercises the
     primitive call inside another primitive call. */
  let r =
    must_ingest(
      ~ns,
      ~store,
      ~att,
      "string.length (int.to_string 12345)",
    );
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(5)) => ()
  | _ => Alcotest.fail("expected length 5 of \"12345\"")
  };
};

/* The wrapping Lam's hash is stable across substrate instances —
   re-installing the primitives in a fresh substrate reproduces the
   same hash for the same id+type. */
/* Under p10's mint-by-default, every namespace binding wraps the
   substructure body with a fresh Named_term mint. So the top-level
   hash for `string.reverse` differs across substrates, but the
   underlying body (the substructure Lam) is structurally identical
   and shares a hash. */
let test_prim_hash_stable = () => {
  let (store_a, att_a, ns_a) = make_substrate_with_prims();
  let (store_b, att_b, ns_b) = make_substrate_with_prims();
  let _ = (att_a, att_b);
  let h_a = Namespace.resolve(ns_a, "string.reverse");
  let h_b = Namespace.resolve(ns_b, "string.reverse");
  /* Minted wrappers must differ — that's the whole point of
     mint-by-default. */
  switch (h_a, h_b) {
  | (Some(a), Some(b)) when a == b =>
    Alcotest.fail("mint-by-default: minted wrapper hashes should differ")
  | (Some(_), Some(_)) => ()
  | _ => Alcotest.fail("string.reverse not bound in one or both substrates")
  };
  /* But unwrapping to the substructure body, the hashes must match —
     structural sharing still works at the substructure level. */
  let body_a = Option.map(Store.unwrap_named(store_a), h_a);
  let body_b = Option.map(Store.unwrap_named(store_b), h_b);
  Alcotest.(check(option(string)))(
    "unwrapped string.reverse body is identical across substrates",
    body_a,
    body_b,
  );
};

/* The typecheck aspect on a primitive's wrapping Lam reflects the
   declared type. */
let test_prim_typecheck_aspect = () => {
  let (store, att, ns) = make_substrate_with_prims();
  switch (Namespace.resolve(ns, "string.length")) {
  | None => Alcotest.fail("string.length not bound")
  | Some(h) =>
    switch (Typecheck.peek_cache(~store, att, h)) {
    | Some(Typecheck.Well_typed(Ty.Arrow(Ty.String, Ty.Int))) => ()
    | _ => Alcotest.fail("expected Well_typed(String -> Int) on string.length")
    }
  };
};

/* A user-defined wrapper that builds on a primitive type-checks and
   evaluates correctly. */
let test_prim_user_wrapper = () => {
  let (store, att, ns) = make_substrate_with_prims();
  let added =
    must_ingest(
      ~ns,
      ~store,
      ~att,
      "\\s: String => string.length s + 1",
    );
  Namespace.bind(ns, ~name="length_plus_one", added.hash);
  let r = must_ingest(~ns, ~store, ~att, "length_plus_one \"abc\"");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(4)) => ()
  | _ => Alcotest.fail("expected 4 (length 3 + 1)")
  };
};

/* ==================== Mint-by-default behavior ==================== */

/* Mint.fresh produces distinct marks call to call. */
let test_mint_fresh_distinct = () => {
  let a = Mint.fresh();
  let b = Mint.fresh();
  if (a == b) {
    Alcotest.fail("Mint.fresh produced two identical marks");
  };
};

/* The deterministic counter generator produces a stable sequence. */
let test_mint_counter_stable = () => {
  Mint.reset_counter();
  let a = Mint.from_counter();
  let b = Mint.from_counter();
  Mint.reset_counter();
  let a' = Mint.from_counter();
  let b' = Mint.from_counter();
  Alcotest.(check(string))(
    "counter mark 1 reproducible after reset",
    a,
    a',
  );
  Alcotest.(check(string))(
    "counter mark 2 reproducible after reset",
    b,
    b',
  );
  if (a == b) {
    Alcotest.fail("counter marks within one run should differ");
  };
};

/* Store.register_named_term wraps a substructure body with a fresh
   mint; two calls on the same body produce distinct named hashes,
   while the underlying body remains shared. */
let test_register_named_term_mints = () => {
  let (store, att, ns) = make_substrate();
  let body = (must_ingest(~ns, ~store, ~att, "1 + 2")).hash;
  let n1 = Store.register_named_term(store, body);
  let n2 = Store.register_named_term(store, body);
  if (n1 == n2) {
    Alcotest.fail("two register_named_term calls produced identical hashes");
  };
  /* Unwrapping both returns the same body. */
  Alcotest.(check(string))(
    "named wrappers share an unwrapped body",
    Store.unwrap_named(store, n1),
    Store.unwrap_named(store, n2),
  );
  Alcotest.(check(string))(
    "unwrapped body matches the original body",
    body,
    Store.unwrap_named(store, n1),
  );
};

/* A Label is just a fresh mint mark; two Labels mint distinct hashes
   even with no other distinguishing content. */
let test_label_mints_distinct = () => {
  let l1 = Label.fresh();
  let l2 = Label.fresh();
  if (Label.hash(l1) == Label.hash(l2)) {
    Alcotest.fail("two Label.fresh produced the same hash");
  };
};

/* ==================== Parser-level tests for new syntax ==================== */

/* `(a, b, c)` parses as Tuple([a, b, c]). */
let test_parse_tuple_3 = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "(1, 2, 3)");
  switch (Store.lookup_term(store, r.hash)) {
  | Some(Node.Tuple([_, _, _])) => ()
  | _ => Alcotest.fail("(1, 2, 3) did not parse as Tuple of 3")
  };
};

/* `[1, 2, 3]` parses as List_lit. */
let test_parse_list_lit = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "[1, 2, 3]");
  switch (Store.lookup_term(store, r.hash)) {
  | Some(Node.List_lit([_, _, _])) => ()
  | _ => Alcotest.fail("[1, 2, 3] did not parse as List_lit of 3")
  };
};

/* `[]` parses as empty list literal. */
let test_parse_empty_list = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "[]");
  switch (Store.lookup_term(store, r.hash)) {
  | Some(Node.List_lit([])) => ()
  | _ => Alcotest.fail("[] did not parse as empty List_lit")
  };
};

/* `{ x = 1, y = 2 }` parses as Record_lit. Labels are minted on
   first use through the resolver. */
let test_parse_record_lit = () => {
  let (store, att, ns) = make_substrate();
  let r = must_ingest(~ns, ~store, ~att, "{ x = 1, y = 2 }");
  switch (Store.lookup_term(store, r.hash)) {
  | Some(Node.Record_lit([_, _])) => ()
  | _ => Alcotest.fail("{ x = 1, y = 2 } did not parse as Record_lit of 2")
  };
  /* The labels should be bound in the namespace by now. */
  switch (Namespace.resolve(ns, "x"), Namespace.resolve(ns, "y")) {
  | (Some(_), Some(_)) => ()
  | _ => Alcotest.fail("record literal did not mint x and y labels")
  };
};

/* `p.x` after a record value evaluates to the field. */
let test_parse_field_projection_eval = () => {
  let (store, att, ns) = make_substrate();
  let r =
    must_ingest(
      ~ns,
      ~store,
      ~att,
      "let p = { x = 7, y = 13 } in p.x",
    );
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(7)) => ()
  | _ => Alcotest.fail("expected p.x to evaluate to 7")
  };
};

/* `p.0` on a tuple value evaluates to the indexed item. */
let test_parse_index_projection_eval = () => {
  let (store, att, ns) = make_substrate();
  let r =
    must_ingest(~ns, ~store, ~att, "let t = (10, 20, 30) in t.1");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(20)) => ()
  | _ => Alcotest.fail("expected t.1 to evaluate to 20")
  };
};

/* `{ p with x = 99 }` parses as Record_update and replaces the field. */
let test_parse_record_update_eval = () => {
  let (store, att, ns) = make_substrate();
  let r =
    must_ingest(
      ~ns,
      ~store,
      ~att,
      "let p = { x = 1, y = 2 } in (let q = { p with x = 99 } in q.x)",
    );
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(99)) => ()
  | _ => Alcotest.fail("expected updated record q.x to evaluate to 99")
  };
};

/* `List Int` parses as the List type. */
let test_parse_list_type = () => {
  let (store, _att, ns) = make_substrate();
  let r =
    Parse_recover.parse_ty("List Int") |> Resolver.ingest_ty(~namespace=ns, ~store);
  switch (r) {
  | Ok({hash, _}) =>
    switch (Store.lookup_type(store, hash)) {
    | Some(Ty.List(Ty.Int)) => ()
    | _ => Alcotest.fail("expected List Int")
    }
  | Error(_) => Alcotest.fail("type parse failed")
  };
};

/* Record type syntax: `{ x : Int, y : Int }` */
let test_parse_record_type = () => {
  let (store, _att, ns) = make_substrate();
  let r =
    Parse_recover.parse_ty("{ x : Int, y : Int }")
    |> Resolver.ingest_ty(~namespace=ns, ~store);
  switch (r) {
  | Ok({hash, _}) =>
    switch (Store.lookup_type(store, hash)) {
    | Some(Ty.Record([_, _])) => ()
    | _ => Alcotest.fail("expected Record with 2 fields")
    }
  | Error(_) => Alcotest.fail("type parse failed")
  };
};

/* Tuple round-trip through ingest + reconstruct + eval. Built
   directly as an Ast.t since the parser doesn't yet recognize tuple
   syntax. */
let test_tuple_ast_roundtrip = () => {
  let (store, _att, _ns) = make_substrate();
  let ast =
    Ast.Tuple([
      Ast.Int_lit(1),
      Ast.String_lit("hi"),
      Ast.Bool_lit(true),
    ]);
  let h = Store.ingest(store, ast);
  switch (Store.reconstruct(store, h)) {
  | Some(Ast.Tuple([
      Ast.Int_lit(1),
      Ast.String_lit("hi"),
      Ast.Bool_lit(true),
    ])) => ()
  | _ => Alcotest.fail("tuple did not round-trip")
  };
};

/* List_lit round-trip + eval. */
let test_list_ast_roundtrip = () => {
  let (store, _att, _ns) = make_substrate();
  let ast = Ast.List_lit([Ast.Int_lit(1), Ast.Int_lit(2), Ast.Int_lit(3)]);
  let h = Store.ingest(store, ast);
  switch (Store.reconstruct(store, h)) {
  | Some(Ast.List_lit([
      Ast.Int_lit(1),
      Ast.Int_lit(2),
      Ast.Int_lit(3),
    ])) => ()
  | _ => Alcotest.fail("list literal did not round-trip")
  };
};

/* Record_lit with two minted labels. The labels' identities are part
   of the canonical encoding, so two different label hashes produce
   distinct Record_lit hashes even with the same value list. */
let test_record_lit_label_identity = () => {
  let (store, _att, _ns) = make_substrate();
  let l_x = Label.fresh();
  let l_y = Label.fresh();
  let xh = Store.register_label(store, l_x);
  let yh = Store.register_label(store, l_y);
  let r1 =
    Store.ingest(
      store,
      Ast.Record_lit([(xh, Ast.Int_lit(1)), (yh, Ast.Int_lit(2))]),
    );
  let r2 =
    Store.ingest(
      store,
      Ast.Record_lit([(yh, Ast.Int_lit(2)), (xh, Ast.Int_lit(1))]),
    );
  /* Field order in the literal must not perturb the hash. */
  Alcotest.(check(string))(
    "record literal field order is canonical",
    r1,
    r2,
  );
};

/* Project_index of a tuple evaluates to the chosen element. */
let test_project_index_tuple = () => {
  let (store, att, _ns) = make_substrate();
  let ast =
    Ast.Project_index(
      Ast.Tuple([
        Ast.Int_lit(10),
        Ast.Int_lit(20),
        Ast.Int_lit(30),
      ]),
      1,
    );
  let h = Store.ingest(store, ast);
  let v = must_eval(~store, ~att, h);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(20)) => ()
  | _ => Alcotest.fail("expected Int_lit 20 from tuple index 1")
  };
};

/* Project_field on a record evaluates to the chosen field's value. */
let test_project_field_record = () => {
  let (store, att, _ns) = make_substrate();
  let l_x = Label.fresh();
  let l_y = Label.fresh();
  let xh = Store.register_label(store, l_x);
  let yh = Store.register_label(store, l_y);
  let ast =
    Ast.Project_field(
      Ast.Record_lit([(xh, Ast.Int_lit(7)), (yh, Ast.Int_lit(13))]),
      yh,
    );
  let h = Store.ingest(store, ast);
  let v = must_eval(~store, ~att, h);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(13)) => ()
  | _ => Alcotest.fail("expected Int_lit 13 from record .y projection")
  };
};

/* Record_update replaces matching label-keyed fields and preserves
   the others. */
let test_record_update = () => {
  let (store, att, _ns) = make_substrate();
  let l_x = Label.fresh();
  let l_y = Label.fresh();
  let xh = Store.register_label(store, l_x);
  let yh = Store.register_label(store, l_y);
  let original =
    Ast.Record_lit([(xh, Ast.Int_lit(1)), (yh, Ast.Int_lit(2))]);
  let updated = Ast.Record_update(original, [(xh, Ast.Int_lit(99))]);
  let proj_x = Ast.Project_field(updated, xh);
  let proj_y = Ast.Project_field(updated, yh);
  let hx = Store.ingest(store, proj_x);
  let hy = Store.ingest(store, proj_y);
  let vx = must_eval(~store, ~att, hx);
  let vy = must_eval(~store, ~att, hy);
  switch (Store.lookup_term(store, vx), Store.lookup_term(store, vy)) {
  | (Some(Node.Int_lit(99)), Some(Node.Int_lit(2))) => ()
  | _ => Alcotest.fail("record update produced wrong values")
  };
};

/* Ty.Record hash is canonicalized by sorted label hash. Two records
   with the same fields in different orders produce the same hash. */
let test_record_ty_field_order_canonical = () => {
  let l1 = Label.fresh();
  let l2 = Label.fresh();
  let h1 = Label.hash(l1);
  let h2 = Label.hash(l2);
  let r_xy = Ty.Record([(h1, Ty.Int), (h2, Ty.String)]);
  let r_yx = Ty.Record([(h2, Ty.String), (h1, Ty.Int)]);
  Alcotest.(check(string))(
    "record field order doesn't affect Ty hash",
    Ty.hash(r_xy),
    Ty.hash(r_yx),
  );
};

/* Tuple type hashes are arity-sensitive. */
let test_tuple_ty_arity_distinct = () => {
  let t2 = Ty.Tuple([Ty.Int, Ty.Int]);
  let t3 = Ty.Tuple([Ty.Int, Ty.Int, Ty.Int]);
  if (Ty.hash(t2) == Ty.hash(t3)) {
    Alcotest.fail("Tuple(2) and Tuple(3) should hash differently");
  };
};

/* List type hash is element-sensitive. */
let test_list_ty_element_distinct = () => {
  let li = Ty.List(Ty.Int);
  let lb = Ty.List(Ty.Bool);
  if (Ty.hash(li) == Ty.hash(lb)) {
    Alcotest.fail("List Int and List Bool should hash differently");
  };
};

/* ==================== Recovery for new partial forms ==================== */

/* `[1, 2,` (no closing bracket) should recover into a parseable
   form via Parse_recover. The exact shape isn't important — only
   that the parser doesn't bail. */
let test_recovery_truncated_list = () => {
  let surface = Parse_recover.parse("[1, 2,");
  switch (surface) {
  | Surface_ast.Hole
  | Surface_ast.List_lit(_) => ()
  | _ =>
    Alcotest.failf(
      "expected Hole or List_lit for truncated `[1, 2,` — got %s",
      Pretty.print_surface(surface),
    )
  };
};

/* `{ x = 1,` should recover. */
let test_recovery_truncated_record = () => {
  let surface = Parse_recover.parse("{ x = 1,");
  switch (surface) {
  | Surface_ast.Hole
  | Surface_ast.Record_lit(_) => ()
  | _ =>
    Alcotest.failf(
      "expected Hole or Record_lit for truncated `{ x = 1,` — got %s",
      Pretty.print_surface(surface),
    )
  };
};

/* `p.` (trailing dot, no name) should recover. */
let test_recovery_dangling_dot = () => {
  let surface = Parse_recover.parse("p.");
  /* The parser may produce either Hole or Var(p) followed by hole —
     just check it parsed something. */
  let _ = surface;
  ();
};

/* QCheck: parsing remains total over arbitrary printable ASCII —
   the recovery layer always returns a Surface_ast.t, even for the
   new bracket/brace tokens. */
let total_parse_new_punctuation =
  QCheck.Test.make(
    ~count=200,
    ~name="parse total on strings sprinkled with new punctuation",
    QCheck.string_printable,
    src => {
      try({
        let _: Surface_ast.t = Parse_recover.parse(src);
        true;
      }) {
      | _ => false
      };
    },
  );

/* ==================== Label sharing across record types ==================== */

/* The labels `x` and `y` declared in one record type must be reused
   by name-resolution when a second record type declares the same
   names — doc 11's "intentional sharing on demand" behavior. */
let test_labels_shared_across_record_types = () => {
  let (store, _att, ns) = make_substrate();
  let r_point =
    Parse_recover.parse_ty("{ x : Int, y : Int }")
    |> Resolver.ingest_ty(~namespace=ns, ~store);
  let r_vector =
    Parse_recover.parse_ty("{ x : Int, y : Int }")
    |> Resolver.ingest_ty(~namespace=ns, ~store);
  switch (r_point, r_vector) {
  | (Ok({hash: h_point, _}), Ok({hash: h_vector, _})) =>
    /* Both record types have the same fields and the same label
       hashes, so canonicalized they hash identically. */
    Alcotest.(check(string))(
      "Point and Vector share record-type hash via shared labels",
      h_point,
      h_vector,
    )
  | _ => Alcotest.fail("record type ingest failed")
  };
  /* And the namespace has x and y bound to single label hashes. */
  switch (Namespace.resolve(ns, "x"), Namespace.resolve(ns, "y")) {
  | (Some(_), Some(_)) => ()
  | _ => Alcotest.fail("x and y labels not bound after declarations")
  };
};

/* When labels are minted by a record-type declaration, the label hash
   participates in the Type's canonical bytes — so a record type
   referencing a freshly-minted `x` hashes differently from one
   referencing a different `x`. */
let test_distinct_x_labels_yield_distinct_record_types = () => {
  /* Two separate substrates: each mints its own fresh `x` and `y`. */
  let (store_a, _att_a, ns_a) = make_substrate();
  let (store_b, _att_b, ns_b) = make_substrate();
  let r_a =
    Parse_recover.parse_ty("{ x : Int, y : Int }")
    |> Resolver.ingest_ty(~namespace=ns_a, ~store=store_a);
  let r_b =
    Parse_recover.parse_ty("{ x : Int, y : Int }")
    |> Resolver.ingest_ty(~namespace=ns_b, ~store=store_b);
  switch (r_a, r_b) {
  | (Ok({hash: h_a, _}), Ok({hash: h_b, _})) =>
    if (h_a == h_b) {
      Alcotest.fail(
        "two substrates with independently-minted x/y labels "
        ++ "produced the same record-type hash",
      );
    }
  | _ => Alcotest.fail("record type ingest failed")
  };
};

/* ==================== List primitives ==================== */

let make_substrate_with_prims_p10 = () => {
  let (store, att, ns) = make_substrate();
  Primitives.install(~store, ~att, ~ns);
  (store, att, ns);
};

let test_list_range_prim = () => {
  let (store, att, ns) = make_substrate_with_prims_p10();
  let r = must_ingest(~ns, ~store, ~att, "list.range 4");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.List_lit(items)) =>
    Alcotest.(check(int))(
      "range 4 produces 4 items",
      4,
      List.length(items),
    )
  | _ => Alcotest.fail("expected List_lit from list.range 4")
  };
};

let test_list_sum_prim = () => {
  let (store, att, ns) = make_substrate_with_prims_p10();
  let r = must_ingest(~ns, ~store, ~att, "list.sum [1, 2, 3, 4]");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(10)) => ()
  | _ => Alcotest.fail("expected list.sum [1,2,3,4] = 10")
  };
};

let test_list_product_prim = () => {
  let (store, att, ns) = make_substrate_with_prims_p10();
  let r = must_ingest(~ns, ~store, ~att, "list.product [2, 3, 4]");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(24)) => ()
  | _ => Alcotest.fail("expected list.product [2,3,4] = 24")
  };
};

let test_list_length_int_prim = () => {
  let (store, att, ns) = make_substrate_with_prims_p10();
  let r = must_ingest(~ns, ~store, ~att, "list.length_int [10, 20, 30, 40, 50]");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.Int_lit(5)) => ()
  | _ => Alcotest.fail("expected length = 5")
  };
};

let test_list_cons_int_prim = () => {
  let (store, att, ns) = make_substrate_with_prims_p10();
  let r = must_ingest(~ns, ~store, ~att, "list.cons_int 0 [1, 2, 3]");
  let v = must_eval(~store, ~att, r.hash);
  switch (Store.lookup_term(store, v)) {
  | Some(Node.List_lit(items)) =>
    Alcotest.(check(int))("cons prepends", 4, List.length(items))
  | _ => Alcotest.fail("expected List_lit from cons")
  };
};

/* ==================== Test registration ==================== */

let () =
  Alcotest.run(
    "p10-minted-labels",
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
      (
        "primitives",
        [
          Alcotest.test_case(
            "string.length",
            `Quick,
            test_prim_string_length,
          ),
          Alcotest.test_case(
            "string.reverse",
            `Quick,
            test_prim_string_reverse,
          ),
          Alcotest.test_case(
            "string.substring (ternary)",
            `Quick,
            test_prim_substring_ternary,
          ),
          Alcotest.test_case(
            "compose primitives",
            `Quick,
            test_prim_int_to_string_compose,
          ),
          Alcotest.test_case(
            "hash stable across substrates",
            `Quick,
            test_prim_hash_stable,
          ),
          Alcotest.test_case(
            "typecheck aspect on wrapping Lam",
            `Quick,
            test_prim_typecheck_aspect,
          ),
          Alcotest.test_case(
            "user wrapper composes",
            `Quick,
            test_prim_user_wrapper,
          ),
        ],
      ),
      (
        "type-bindings",
        [
          Alcotest.test_case(
            "named alias = structural",
            `Quick,
            test_named_type_alias_identity,
          ),
          Alcotest.test_case(
            "two aliases share hash",
            `Quick,
            test_two_aliases_share_hash,
          ),
          Alcotest.test_case(
            "mixed-case dotted alias in annotation",
            `Quick,
            test_mixed_case_dotted_name_in_annotation,
          ),
          Alcotest.test_case(
            "recursive alias",
            `Quick,
            test_recursive_alias,
          ),
          Alcotest.test_case(
            "term in type position errors",
            `Quick,
            test_term_in_type_position_errors,
          ),
          Alcotest.test_case(
            "type in term position errors",
            `Quick,
            test_type_in_term_position_errors,
          ),
          Alcotest.test_case(
            "Type_of aspect round-trips",
            `Quick,
            test_type_of_aspect_round_trips,
          ),
          Alcotest.test_case(
            "Type_of aspect dedups",
            `Quick,
            test_type_of_aspect_dedups,
          ),
        ],
      ),
      (
        "minted-identity",
        [
          Alcotest.test_case(
            "Mint.fresh produces distinct marks",
            `Quick,
            test_mint_fresh_distinct,
          ),
          Alcotest.test_case(
            "Mint counter is reset-stable + within-run distinct",
            `Quick,
            test_mint_counter_stable,
          ),
          Alcotest.test_case(
            "register_named_term mints; unwrap returns shared body",
            `Quick,
            test_register_named_term_mints,
          ),
          Alcotest.test_case(
            "Label.fresh produces distinct hashes",
            `Quick,
            test_label_mints_distinct,
          ),
          Alcotest.test_case(
            "Ty.Record field order canonicalized",
            `Quick,
            test_record_ty_field_order_canonical,
          ),
          Alcotest.test_case(
            "Ty.Tuple arity distinct hashes",
            `Quick,
            test_tuple_ty_arity_distinct,
          ),
          Alcotest.test_case(
            "Ty.List element type distinct hashes",
            `Quick,
            test_list_ty_element_distinct,
          ),
          Alcotest.test_case(
            "Tuple AST round-trip",
            `Quick,
            test_tuple_ast_roundtrip,
          ),
          Alcotest.test_case(
            "List literal AST round-trip",
            `Quick,
            test_list_ast_roundtrip,
          ),
          Alcotest.test_case(
            "Record literal canonical by label order",
            `Quick,
            test_record_lit_label_identity,
          ),
          Alcotest.test_case(
            "Project_index of tuple evaluates",
            `Quick,
            test_project_index_tuple,
          ),
          Alcotest.test_case(
            "Project_field of record evaluates",
            `Quick,
            test_project_field_record,
          ),
          Alcotest.test_case(
            "Record_update replaces selected fields",
            `Quick,
            test_record_update,
          ),
        ],
      ),
      (
        "new-surface-syntax",
        [
          Alcotest.test_case(
            "(a, b, c) parses as Tuple",
            `Quick,
            test_parse_tuple_3,
          ),
          Alcotest.test_case(
            "[1, 2, 3] parses as List_lit",
            `Quick,
            test_parse_list_lit,
          ),
          Alcotest.test_case(
            "[] parses as empty List_lit",
            `Quick,
            test_parse_empty_list,
          ),
          Alcotest.test_case(
            "{ x = 1, y = 2 } parses + mints labels",
            `Quick,
            test_parse_record_lit,
          ),
          Alcotest.test_case(
            "p.x field projection evaluates",
            `Quick,
            test_parse_field_projection_eval,
          ),
          Alcotest.test_case(
            "t.0 index projection evaluates",
            `Quick,
            test_parse_index_projection_eval,
          ),
          Alcotest.test_case(
            "{ p with x = 99 } record update evaluates",
            `Quick,
            test_parse_record_update_eval,
          ),
          Alcotest.test_case(
            "List Int parses as List type",
            `Quick,
            test_parse_list_type,
          ),
          Alcotest.test_case(
            "{ x : Int, y : Int } parses as Record type",
            `Quick,
            test_parse_record_type,
          ),
        ],
      ),
      (
        "label-sharing",
        [
          Alcotest.test_case(
            "x and y labels shared across Point + Vector",
            `Quick,
            test_labels_shared_across_record_types,
          ),
          Alcotest.test_case(
            "independently-minted x/y produce distinct types",
            `Quick,
            test_distinct_x_labels_yield_distinct_record_types,
          ),
        ],
      ),
      (
        "list-primitives",
        [
          Alcotest.test_case(
            "list.range 4",
            `Quick,
            test_list_range_prim,
          ),
          Alcotest.test_case(
            "list.sum [1,2,3,4] = 10",
            `Quick,
            test_list_sum_prim,
          ),
          Alcotest.test_case(
            "list.product [2,3,4] = 24",
            `Quick,
            test_list_product_prim,
          ),
          Alcotest.test_case(
            "list.length_int = 5",
            `Quick,
            test_list_length_int_prim,
          ),
          Alcotest.test_case(
            "list.cons_int prepends",
            `Quick,
            test_list_cons_int_prim,
          ),
        ],
      ),
      (
        "recovery-new-forms",
        [
          Alcotest.test_case(
            "truncated `[1, 2,` recovers",
            `Quick,
            test_recovery_truncated_list,
          ),
          Alcotest.test_case(
            "truncated `{ x = 1,` recovers",
            `Quick,
            test_recovery_truncated_record,
          ),
          Alcotest.test_case(
            "trailing `p.` recovers",
            `Quick,
            test_recovery_dangling_dot,
          ),
          QCheck_alcotest.to_alcotest(total_parse_new_punctuation),
        ],
      ),
    ],
  );
