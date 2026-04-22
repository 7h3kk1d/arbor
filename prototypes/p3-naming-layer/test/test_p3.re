open P3_naming_layer;

/* p3 parser returns Surface_ast.t; tests want an Ast.t. Name-free inputs
   resolve to themselves with an empty namespace and store. */
let parse = (s: string): Ast.t => {
  let lexbuf = Lexing.from_string(s);
  let surface = Parser.main(Lexer.token, lexbuf);
  switch (
    Resolver.resolve(~namespace=Namespace.create(), ~store=Store.create(), surface)
  ) {
  | Ok(ast) => ast
  | Error(Resolver.Unbound_name(n)) =>
    failwith("parse: unexpected name in test input: " ++ n)
  | Error(Resolver.Missing_hash(_, _)) =>
    failwith("parse: missing hash during resolution")
  };
};

let parse_surface = (s: string): Surface_ast.t => {
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

/* ===== Namespace ===== */

let test_ns_bind_resolve = () => {
  let ns = Namespace.create();
  let h = "deadbeef";
  Namespace.bind(ns, ~name="foo", h);
  Alcotest.(check(bool))(
    "resolve returns bound hash",
    true,
    Namespace.resolve(ns, "foo") == Some(h),
  );
};

let test_ns_double_bind_raises = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "aaa");
  let raised =
    try(
      {
        Namespace.bind(ns, ~name="foo", "bbb");
        false;
      }
    ) {
    | Namespace.Name_already_bound(_) => true
    };
  Alcotest.(check(bool))("second bind raises", true, raised);
};

let test_ns_rebind_overwrites = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "aaa");
  Namespace.rebind(ns, ~name="foo", "bbb");
  Alcotest.(check(bool))(
    "rebind updates forward lookup",
    true,
    Namespace.resolve(ns, "foo") == Some("bbb"),
  );
  /* Reverse should no longer have aaa -> foo */
  Alcotest.(check(int))(
    "old hash no longer has the name",
    0,
    List.length(Namespace.names_of(ns, "aaa")),
  );
  Alcotest.(check(int))(
    "new hash has the name",
    1,
    List.length(Namespace.names_of(ns, "bbb")),
  );
};

let test_ns_unbind = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "aaa");
  Alcotest.(check(bool))("unbind reports removal", true, Namespace.unbind(ns, ~name="foo"));
  Alcotest.(check(bool))("resolve is now None", true, Namespace.resolve(ns, "foo") == None);
  Alcotest.(check(bool))("second unbind reports nothing removed", false, Namespace.unbind(ns, ~name="foo"));
};

let test_ns_aliases_sorted = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="zeta", "aaa");
  Namespace.bind(ns, ~name="alpha", "aaa");
  Namespace.bind(ns, ~name="mu", "aaa");
  Alcotest.(check(list(string)))(
    "names_of returns all aliases sorted",
    ["alpha", "mu", "zeta"],
    Namespace.names_of(ns, "aaa"),
  );
};

let test_ns_rename = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "aaa");
  Alcotest.(check(bool))(
    "rename ok when target free",
    true,
    Namespace.rename(ns, ~from="foo", ~to_="bar") == Ok(),
  );
  Alcotest.(check(bool))(
    "target resolves",
    true,
    Namespace.resolve(ns, "bar") == Some("aaa"),
  );
  Alcotest.(check(bool))(
    "source no longer resolves",
    true,
    Namespace.resolve(ns, "foo") == None,
  );
  /* rename from an unbound name */
  Alcotest.(check(bool))(
    "source unbound error",
    true,
    Namespace.rename(ns, ~from="nope", ~to_="x") == Error(Namespace.Source_unbound),
  );
  /* target already bound */
  Namespace.bind(ns, ~name="foo", "bbb");
  Alcotest.(check(bool))(
    "target already bound error",
    true,
    Namespace.rename(ns, ~from="bar", ~to_="foo") == Error(Namespace.Target_already_bound),
  );
};

let test_ns_reserved = () => {
  let ns = Namespace.create();
  let try_bind = name =>
    try(
      {
        Namespace.bind(ns, ~name, "aaa");
        false;
      }
    ) {
    | Namespace.Name_reserved(_) => true
    };
  List.iter(
    kw =>
      Alcotest.(check(bool))(
        "reserved keyword rejected: " ++ kw,
        true,
        try_bind(kw),
      ),
    ["true", "false", "succ", "pred", "iszero", "if", "then", "else"],
  );
};

/* ===== Resolver ===== */

let test_resolver_name_substitution = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_one = Store.ingest(store, parse("succ 0"));
  Namespace.bind(ns, ~name="one", h_one);
  let surface = parse_surface("succ one");
  switch (Resolver.resolve(~namespace=ns, ~store, surface)) {
  | Error(_) => Alcotest.fail("resolve failed")
  | Ok(ast) =>
    Alcotest.check(
      ast_testable,
      "succ one ≡ succ (succ 0)",
      Ast.Succ(Ast.Succ(Ast.Zero)),
      ast,
    )
  };
};

let test_resolver_unbound = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let surface = parse_surface("succ foo");
  switch (Resolver.resolve(~namespace=ns, ~store, surface)) {
  | Ok(_) => Alcotest.fail("expected Unbound_name")
  | Error(Resolver.Unbound_name(n)) =>
    Alcotest.(check(string))("unbound name carries identifier", "foo", n)
  | Error(Resolver.Missing_hash(_, _)) => Alcotest.fail("unexpected Missing_hash")
  };
};

let test_resolver_edit_time_structural_sharing = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_one = Store.ingest(store, parse("succ 0"));
  Namespace.bind(ns, ~name="one", h_one);
  let h_two_named =
    switch (Resolver.resolve(~namespace=ns, ~store, parse_surface("succ one"))) {
    | Ok(a) => Store.ingest(store, a)
    | Error(_) => Alcotest.fail("resolve failed") |> ignore |> (() => "")
    };
  let h_two_direct = Store.ingest(store, parse("succ (succ 0)"));
  Alcotest.(check(bool))(
    "edit-time resolution of named expr hashes identically to direct",
    true,
    Hash.equal(h_two_named, h_two_direct),
  );
};

/* ===== No silent breakage ===== */

let test_no_silent_breakage = () => {
  let store = Store.create();
  let ns = Namespace.create();
  /* bind one := succ 0 */
  let h_one_v1 = Store.ingest(store, parse("succ 0"));
  Namespace.bind(ns, ~name="one", h_one_v1);
  /* bind two := succ one (resolved) */
  let h_two =
    switch (Resolver.resolve(~namespace=ns, ~store, parse_surface("succ one"))) {
    | Ok(a) => Store.ingest(store, a)
    | Error(_) => Alcotest.fail("resolve failed") |> ignore |> (() => "")
    };
  Namespace.bind(ns, ~name="two", h_two);
  /* confirm two's stored child is h_one_v1 */
  let child_hash_before =
    switch (Store.lookup(store, h_two)) {
    | Some(Node.Succ(ch)) => ch
    | _ => Alcotest.fail("two is not Succ(_)") |> ignore |> (() => "")
    };
  Alcotest.(check(bool))(
    "two's child equals h_one_v1",
    true,
    Hash.equal(child_hash_before, h_one_v1),
  );
  /* rebind one to a different hash */
  let h_one_v2 = Store.ingest(store, parse("pred (succ 0)"));
  Namespace.rebind(ns, ~name="one", h_one_v2);
  Alcotest.(check(bool))(
    "namespace now resolves one to v2",
    true,
    Namespace.resolve(ns, "one") == Some(h_one_v2),
  );
  /* but two's stored node still references h_one_v1 — no silent breakage */
  let child_hash_after =
    switch (Store.lookup(store, h_two)) {
    | Some(Node.Succ(ch)) => ch
    | _ => Alcotest.fail("two is not Succ(_) after rebind") |> ignore |> (() => "")
    };
  Alcotest.(check(bool))(
    "two's child hash unchanged after rebind",
    true,
    Hash.equal(child_hash_after, h_one_v1),
  );
  Alcotest.(check(bool))(
    "two reconstructs to its original AST",
    true,
    switch (Store.reconstruct(store, h_two)) {
    | Some(t) => Ast.equal(t, Ast.Succ(Ast.Succ(Ast.Zero)))
    | None => false
    },
  );
};

/* ===== Pretty-printer name substitution ===== */

let test_pretty_substitutes_named_child = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_one = Store.ingest(store, parse("succ 0"));
  Namespace.bind(ns, ~name="one", h_one);
  let h_two = Store.ingest(store, parse("succ (succ 0)"));
  let surface = Pretty.surface_of_hash(~namespace=ns, store, h_two);
  Alcotest.(check(bool))(
    "surface_of_hash substitutes named child",
    true,
    Surface_ast.equal(surface, Surface_ast.Succ(Surface_ast.Name("one"))),
  );
  Alcotest.(check(string))(
    "print_surface emits 'succ one'",
    "succ one",
    Pretty.print_surface(surface),
  );
};

let test_pretty_top_level_not_substituted = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_one = Store.ingest(store, parse("succ 0"));
  Namespace.bind(ns, ~name="one", h_one);
  let surface = Pretty.surface_of_hash(~namespace=ns, store, h_one);
  Alcotest.(check(bool))(
    "top level never collapses to Name",
    true,
    Surface_ast.equal(surface, Surface_ast.Succ(Surface_ast.Zero)),
  );
};

let test_pretty_multi_name_picks_alphabetical = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_one = Store.ingest(store, parse("succ 0"));
  Namespace.bind(ns, ~name="zeta", h_one);
  Namespace.bind(ns, ~name="alpha", h_one);
  let h_two = Store.ingest(store, parse("succ (succ 0)"));
  let s = Pretty.print_named(~namespace=ns, store, h_two);
  Alcotest.(check(string))(
    "printer picks alphabetically first name",
    "succ alpha",
    s,
  );
};

let test_pretty_without_namespace_unchanged = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h = Store.ingest(store, parse("if iszero 0 then succ 0 else 0"));
  let with_names = Pretty.print_named(~namespace=ns, store, h);
  let raw = Pretty.print(store, h);
  Alcotest.(check(string))(
    "empty namespace: named printer matches raw printer",
    raw,
    with_names,
  );
};

/* ===== Hash display prefix and input tolerance ===== */

let test_hash_display_prefix = () => {
  let h = "1ec244c78f4200000000";
  Alcotest.(check(string))(
    "Hash.to_string uses '#' prefix",
    "#" ++ h,
    Hash.to_string(h),
  );
  Alcotest.(check(string))(
    "Hash.short uses '#' prefix",
    "#1ec244c78f42",
    Hash.short(h),
  );
};

let test_hash_input_tolerance = () => {
  let store = Store.create();
  let h = Store.ingest(store, parse("succ 0"));
  let bare = h;
  let hashed = "#" ++ h;
  let legacy = "h:" ++ h;
  let r1 = Store.resolve_prefix(store, bare);
  let r2 = Store.resolve_prefix(store, hashed);
  let r3 = Store.resolve_prefix(store, legacy);
  let eq_found =
    fun
    | (Hash.Found(a), Hash.Found(b)) => Hash.equal(a, b)
    | _ => false;
  Alcotest.(check(bool))("bare hex resolves", true, eq_found((r1, Hash.Found(h))));
  Alcotest.(check(bool))("'#' prefix resolves", true, eq_found((r2, Hash.Found(h))));
  Alcotest.(check(bool))("legacy 'h:' prefix resolves", true, eq_found((r3, Hash.Found(h))));
};

let test_looks_like_hash_prefix = () => {
  Alcotest.(check(bool))("'#deadbeef' looks like hash", true, Hash.looks_like_hash_prefix("#deadbeef"));
  Alcotest.(check(bool))("'h:deadbeef' looks like hash", true, Hash.looks_like_hash_prefix("h:deadbeef"));
  Alcotest.(check(bool))(
    "'deadbeef' (bare hex, >= 4) looks like hash",
    true,
    Hash.looks_like_hash_prefix("deadbeef"),
  );
  Alcotest.(check(bool))("'one' is not a hash prefix", false, Hash.looks_like_hash_prefix("one"));
  Alcotest.(check(bool))("'abc' (too short, bare) is not", false, Hash.looks_like_hash_prefix("abc"));
  Alcotest.(check(bool))(
    "'#abc' (prefixed but short) IS a hash",
    true,
    Hash.looks_like_hash_prefix("#abc"),
  );
  Alcotest.(check(bool))("'#xyz' (prefixed, non-hex) is not", false, Hash.looks_like_hash_prefix("#xyz"));
};

/* ===== Runner ===== */

let () =
  Alcotest.run(
    "p3-naming-layer",
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
      (
        "namespace",
        [
          Alcotest.test_case("bind + resolve", `Quick, test_ns_bind_resolve),
          Alcotest.test_case("double bind raises", `Quick, test_ns_double_bind_raises),
          Alcotest.test_case("rebind overwrites", `Quick, test_ns_rebind_overwrites),
          Alcotest.test_case("unbind", `Quick, test_ns_unbind),
          Alcotest.test_case("aliases sorted", `Quick, test_ns_aliases_sorted),
          Alcotest.test_case("rename", `Quick, test_ns_rename),
          Alcotest.test_case("reserved keywords", `Quick, test_ns_reserved),
        ],
      ),
      (
        "resolver",
        [
          Alcotest.test_case("name substitution", `Quick, test_resolver_name_substitution),
          Alcotest.test_case("unbound error", `Quick, test_resolver_unbound),
          Alcotest.test_case(
            "edit-time structural sharing",
            `Quick,
            test_resolver_edit_time_structural_sharing,
          ),
        ],
      ),
      (
        "no-silent-breakage",
        [
          Alcotest.test_case(
            "rebind does not mutate stored programs",
            `Quick,
            test_no_silent_breakage,
          ),
        ],
      ),
      (
        "pretty-names",
        [
          Alcotest.test_case(
            "substitutes named child",
            `Quick,
            test_pretty_substitutes_named_child,
          ),
          Alcotest.test_case(
            "top level not substituted",
            `Quick,
            test_pretty_top_level_not_substituted,
          ),
          Alcotest.test_case(
            "multi-name picks alphabetical",
            `Quick,
            test_pretty_multi_name_picks_alphabetical,
          ),
          Alcotest.test_case(
            "empty namespace matches raw",
            `Quick,
            test_pretty_without_namespace_unchanged,
          ),
        ],
      ),
      (
        "hash-display",
        [
          Alcotest.test_case("to_string uses #", `Quick, test_hash_display_prefix),
          Alcotest.test_case("input tolerates # and h:", `Quick, test_hash_input_tolerance),
          Alcotest.test_case("looks_like_hash_prefix", `Quick, test_looks_like_hash_prefix),
        ],
      ),
    ],
  );
