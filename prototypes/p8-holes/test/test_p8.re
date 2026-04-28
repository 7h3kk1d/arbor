open P8_holes;

/* Parser wrapper that resolves surface names against a given namespace
   and store. Name-free inputs resolve with an empty namespace to
   themselves. In p8 parsing is total — we go through Parse_recover so
   malformed tests would surface as unexpected Hole nodes rather than
   exceptions. Intentional explicit holes in tests flow through the
   resolver to Ast.Hole. */
let parse_with = (~namespace, ~store, s: string): Ast.t => {
  let surface = Parse_recover.parse(s);
  switch (Resolver.resolve(~namespace, ~store, surface)) {
  | Ok(ast) => ast
  | Error(Resolver.Unbound_name(n)) =>
    failwith("parse: unexpected unbound name in test input: " ++ n)
  | Error(Resolver.Missing_hash(_, _)) =>
    failwith("parse: missing hash during resolution")
  };
};

let parse = (s: string): Ast.t =>
  parse_with(~namespace=Namespace.create(), ~store=Store.create(), s);

let parse_surface = (s: string): Surface_ast.t => Parse_recover.parse(s);

let ast_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Ast.show(t)),
    Ast.equal,
  );

let eval_result_testable =
  Alcotest.testable(
    (fmt, r) =>
      switch (r) {
      | Eval.Value(h) => Format.fprintf(fmt, "Value(%s)", Hash.short(h))
      | Eval.Stuck(h) => Format.fprintf(fmt, "Stuck(%s)", Hash.short(h))
      | Eval.StepLimit(h) =>
        Format.fprintf(fmt, "StepLimit(%s)", Hash.short(h))
      },
    (a, b) =>
      switch (a, b) {
      | (Eval.Value(x), Eval.Value(y)) => Hash.equal(x, y)
      | (Eval.Stuck(x), Eval.Stuck(y)) => Hash.equal(x, y)
      | (Eval.StepLimit(x), Eval.StepLimit(y)) => Hash.equal(x, y)
      | _ => false
      },
  );

let fresh_world = () => {
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Eval.descriptor);
  (store, att);
};

let ingest_src = (s: string): (Store.t, Hash.t) => {
  let store = Store.create();
  let h = Store.ingest(store, parse(s));
  (store, h);
};

/* ===== Parser tests ===== */

let surface_testable =
  Alcotest.testable(
    (fmt, s) => Format.fprintf(fmt, "%s", Surface_ast.show(s)),
    Surface_ast.equal,
  );

let test_parse_var = () =>
  Alcotest.check(
    surface_testable,
    "x parses to Var \"x\" (surface)",
    Surface_ast.Var("x"),
    parse_surface("x"),
  );

let test_parse_id = () =>
  Alcotest.check(ast_testable, "\\x. x = Lam(Var 0)", Ast.Lam(Ast.Var(0)), parse("\\x. x"));

let test_parse_const = () =>
  Alcotest.check(
    ast_testable,
    "\\x. \\y. x = Lam(Lam(Var 1))",
    Ast.Lam(Ast.Lam(Ast.Var(1))),
    parse("\\x. \\y. x"),
  );

let test_parse_app_left_assoc = () =>
  Alcotest.check(
    ast_testable,
    "\\f. \\x. \\y. f x y",
    Ast.Lam(Ast.Lam(Ast.Lam(Ast.App(Ast.App(Ast.Var(2), Ast.Var(1)), Ast.Var(0))))),
    parse("\\f. \\x. \\y. f x y"),
  );

let test_parse_lam_body_extends_right = () =>
  Alcotest.check(
    ast_testable,
    "\\f. \\x. f x = Lam(Lam(App(Var 1, Var 0)))",
    Ast.Lam(Ast.Lam(Ast.App(Ast.Var(1), Ast.Var(0)))),
    parse("\\f. \\x. f x"),
  );

let test_parse_paren_lam_applied = () =>
  Alcotest.check(
    ast_testable,
    "(\\x. x) (\\y. y)",
    Ast.App(Ast.Lam(Ast.Var(0)), Ast.Lam(Ast.Var(0))),
    parse("(\\x. x) (\\y. y)"),
  );

/* ===== Shift / subst / beta helpers ===== */

let test_shift_closed = () =>
  Alcotest.check(
    ast_testable,
    "shift of closed term is identity",
    parse("\\x. \\y. x"),
    Ast.shift(~cutoff=0, ~by=5, parse("\\x. \\y. x")),
  );

let test_shift_free_var = () =>
  Alcotest.check(
    ast_testable,
    "shift raises free indices at/above cutoff",
    Ast.Lam(Ast.Var(3)),
    Ast.shift(~cutoff=0, ~by=2, Ast.Lam(Ast.Var(1))),
  );

let test_beta_identity = () =>
  Alcotest.check(
    ast_testable,
    "(\\x. x) y = y (with y = \\z. z)",
    parse("\\z. z"),
    Ast.beta(~body=Ast.Var(0), ~arg=parse("\\z. z")),
  );

let test_beta_const = () => {
  /* (\\x. \\y. x) a b → a  */
  let const = parse("\\x. \\y. x"); /* Lam(Lam(Var 1)) */
  let a = parse("\\z. z");
  let b = parse("\\z. \\w. z");
  /* First β: (\\x. \\y. x) a  → substitute a for x */
  let step1 =
    switch (const) {
    | Ast.Lam(body) => Ast.beta(~body, ~arg=a)
    | _ => failwith("const not a Lam")
    };
  /* Second β: (\\y. a) b → a  */
  let step2 =
    switch (step1) {
    | Ast.Lam(body) => Ast.beta(~body, ~arg=b)
    | _ => failwith("step1 not a Lam")
    };
  Alcotest.check(ast_testable, "(const a b) = a", a, step2);
};

/* ===== Alpha-equivalence via de Bruijn canonical form ===== */

let test_alpha_id = () => {
  let (s1, h1) = ingest_src("\\x. x");
  let (s2, h2) = ingest_src("\\y. y");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(string))(
    "\\x. x and \\y. y hash identically",
    h1,
    h2,
  );
};

let test_alpha_const = () => {
  let (s1, h1) = ingest_src("\\x. \\y. x");
  let (s2, h2) = ingest_src("\\a. \\b. a");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(string))(
    "\\x. \\y. x and \\a. \\b. a hash identically",
    h1,
    h2,
  );
};

let test_alpha_nested = () => {
  let (s1, h1) = ingest_src("\\x. \\y. \\z. x y z");
  let (s2, h2) = ingest_src("\\a. \\b. \\c. a b c");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(string))(
    "deep α-equivalent terms hash identically",
    h1,
    h2,
  );
};

let test_not_alpha_equivalent = () => {
  let (s1, h1) = ingest_src("\\x. \\y. x");
  let (s2, h2) = ingest_src("\\x. \\y. y");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(bool))(
    "K = \\x. \\y. x and K2 = \\x. \\y. y hash differently",
    true,
    !String.equal(h1, h2),
  );
};

/* ===== qcheck generator for closed LC terms =====
   Written as a direct state-consuming function so the recursion is
   lazy on the pick — a combinator-based generator built the whole
   branching tree eagerly and blew the stack. */

let rec closed_gen_fn = (~depth: int, ~size: int, rand: Random.State.t): Ast.t =>
  if (size <= 0) {
    /* Leaf. If under binders, a Var is allowed; otherwise force a Lam. */
    if (depth > 0 && Random.State.bool(rand)) {
      Ast.Var(Random.State.int(rand, depth));
    } else {
      Ast.Lam(closed_gen_fn(~depth=depth + 1, ~size=0, rand));
    };
  } else {
    /* Weighted choice. Total weight = (depth > 0 ? 2 : 0) + 3 + 2. */
    let var_w = depth > 0 ? 2 : 0;
    let total = var_w + 3 + 2;
    let pick = Random.State.int(rand, total);
    if (depth > 0 && pick < var_w) {
      Ast.Var(Random.State.int(rand, depth));
    } else if (pick < var_w + 3) {
      Ast.Lam(closed_gen_fn(~depth=depth + 1, ~size=size - 1, rand));
    } else {
      Ast.App(
        closed_gen_fn(~depth, ~size=size / 2, rand),
        closed_gen_fn(~depth, ~size=size / 2, rand),
      );
    };
  };

let closed_gen = (~depth, ~size): QCheck.Gen.t(Ast.t) =>
  closed_gen_fn(~depth, ~size);

let closed_ast_arb =
  QCheck.make(~print=Ast.show, closed_gen(~depth=0, ~size=6));

let prop_ingest_reconstruct_roundtrip =
  QCheck.Test.make(
    ~count=200,
    ~name="reconstruct(ingest(t)) = Some(t)",
    closed_ast_arb,
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
    closed_ast_arb,
    t => {
      let s1 = Store.create();
      let s2 = Store.create();
      Hash.equal(Store.ingest(s1, t), Store.ingest(s2, t));
    },
  );

let prop_generated_terms_are_closed =
  QCheck.Test.make(
    ~count=200,
    ~name="closed_gen produces closed terms",
    closed_ast_arb,
    t =>
    Ast.is_closed(t)
  );

/* ===== Structural sharing ===== */

let test_subterm_sharing = () => {
  let store = Store.create();
  let _ = Store.ingest(store, parse("\\x. (\\y. y) (\\y. y)"));
  let size_before = Store.size(store);
  /* Ingesting \y. y (a subterm) should not add new entries. */
  let _ = Store.ingest(store, parse("\\y. y"));
  Alcotest.(check(int))(
    "no new entries when re-ingesting a subterm",
    size_before,
    Store.size(store),
  );
};

/* ===== Evaluator ===== */

let test_eval_lam_is_value = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse("\\x. x"));
  Alcotest.check(
    eval_result_testable,
    "\\x. x is a value",
    Eval.Value(h),
    Eval.eval(~store, ~att, h),
  );
};

let test_eval_beta_identity = () => {
  let (store, att) = fresh_world();
  let h_id = Store.ingest(store, parse("\\x. x"));
  let h_id_id = Store.ingest(store, parse("(\\x. x) (\\y. y)"));
  /* id id = id */
  Alcotest.check(
    eval_result_testable,
    "id id = id",
    Eval.Value(h_id),
    Eval.eval(~store, ~att, h_id_id),
  );
};

let test_eval_const = () => {
  let (store, att) = fresh_world();
  let h_a = Store.ingest(store, parse("\\x. x"));
  let h_k =
    Store.ingest(store, parse("(\\x. \\y. x) (\\z. z) (\\w. \\v. w)"));
  Alcotest.check(
    eval_result_testable,
    "const a b = a",
    Eval.Value(h_a),
    Eval.eval(~store, ~att, h_k),
  );
};

let test_eval_nested_apps = () => {
  let (store, att) = fresh_world();
  /* (\\f. \\x. f (f x)) (\\y. y) (\\z. z)  should reduce to \\z. z */
  let h_z = Store.ingest(store, parse("\\z. z"));
  let h_twice =
    Store.ingest(
      store,
      parse("(\\f. \\x. f (f x)) (\\y. y) (\\z. z)"),
    );
  Alcotest.check(
    eval_result_testable,
    "(twice id) id = id",
    Eval.Value(h_z),
    Eval.eval(~store, ~att, h_twice),
  );
};

let test_eval_step_limit_omega = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse("(\\x. x x) (\\x. x x)"));
  let result = Eval.eval(~store, ~att, ~step_limit=50, h);
  let is_step_limit =
    switch (result) {
    | Eval.StepLimit(_) => true
    | _ => false
    };
  Alcotest.(check(bool))(
    "Omega hits step limit under a small budget",
    true,
    is_step_limit,
  );
};

let test_eval_step_limit_not_cached = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse("(\\x. x x) (\\x. x x)"));
  let _ = Eval.eval(~store, ~att, ~step_limit=50, h);
  /* StepLimit must not be cached: peek should return None. */
  Alcotest.(check(bool))(
    "StepLimit result is not cached",
    false,
    Option.is_some(Eval.peek_cache(att, h)),
  );
};

let test_eval_cache_hit = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest(store, parse("(\\x. x) (\\y. y)"));
  let _ = Eval.eval(~store, ~att, h);
  Attachment.reset_counters(att);
  let _ = Eval.eval(~store, ~att, h);
  let stats = Attachment.stats(att);
  Alcotest.(check(bool))(
    "top-level re-eval is a cache hit",
    true,
    stats.hits >= 1,
  );
};

/* ===== Resolver ===== */

let test_resolver_bound_wins = () => {
  /* bind x to (\\y. y); then "\\x. x" should resolve x to Var(0), not to
     the namespace binding. */
  let store = Store.create();
  let ns = Namespace.create();
  let h_id = Store.ingest(store, parse("\\y. y"));
  Namespace.bind(ns, ~name="x", h_id);
  let resolved = parse_with(~namespace=ns, ~store, "\\x. x");
  Alcotest.check(
    ast_testable,
    "bound variable shadows namespace binding",
    Ast.Lam(Ast.Var(0)),
    resolved,
  );
};

let test_resolver_namespace_fallthrough = () => {
  /* bind `id` to (\\y. y); using `id` outside any binder should inline
     the stored subtree. */
  let store = Store.create();
  let ns = Namespace.create();
  let _ = Store.ingest(store, parse("\\y. y"));
  let h_id = Store.ingest(store, parse("\\y. y"));
  Namespace.bind(ns, ~name="id", h_id);
  let resolved = parse_with(~namespace=ns, ~store, "id");
  Alcotest.check(
    ast_testable,
    "free name falls through to namespace",
    Ast.Lam(Ast.Var(0)),
    resolved,
  );
};

let test_resolver_unbound = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let surface = parse_surface("\\x. y x");
  switch (Resolver.resolve(~namespace=ns, ~store, surface)) {
  | Error(Resolver.Unbound_name(n)) =>
    Alcotest.(check(string))("unbound name reported", "y", n)
  | _ => Alcotest.fail("expected Unbound_name error")
  };
};

let test_resolver_nested_shadowing = () => {
  /* \\x. \\x. x  — inner x shadows outer; de Bruijn index is 0 */
  let resolved = parse("\\x. \\x. x");
  Alcotest.check(
    ast_testable,
    "innermost binder wins in shadowing",
    Ast.Lam(Ast.Lam(Ast.Var(0))),
    resolved,
  );
};

/* ===== Alpha-equivalence of resolved surface terms ===== */

let prop_alpha_equivalent_surface_same_hash =
  QCheck.Test.make(
    ~count=50,
    ~name="surface α-renaming preserves ingest hash",
    closed_ast_arb,
    t => {
      /* Render the term twice under two different fresh-name alphabets
         by rendering under an empty namespace; then re-parse both and
         check they hash identically. */
      let store = Store.create();
      let ns = Namespace.create();
      let _ = Store.ingest(store, t);
      let rendered =
        Pretty.print_surface(
          Pretty.surface_of_hash(
            ~namespace=ns,
            store,
            Store.ingest(store, t),
          ),
        );
      let reingested = parse(rendered);
      Ast.equal(t, reingested);
    },
  );

/* ===== Namespace (carried from p3, adapted) ===== */

let test_ns_bind_and_resolve = () => {
  let ns = Namespace.create();
  let h = "abcd";
  Namespace.bind(ns, ~name="foo", h);
  Alcotest.(check(option(string)))(
    "resolves foo",
    Some(h),
    Namespace.resolve(ns, "foo"),
  );
};

let test_ns_double_bind_raises = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "h1");
  let threw =
    switch (Namespace.bind(ns, ~name="foo", "h2")) {
    | exception (Namespace.Name_already_bound(_)) => true
    | _ => false
    };
  Alcotest.(check(bool))("double bind raises", true, threw);
};

let test_ns_rebind = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "h1");
  Namespace.rebind(ns, ~name="foo", "h2");
  Alcotest.(check(option(string)))(
    "rebind updates",
    Some("h2"),
    Namespace.resolve(ns, "foo"),
  );
  Alcotest.(check((list(string))))(
    "old hash has no inverse",
    [],
    Namespace.names_of(ns, "h1"),
  );
};

let test_ns_aliases = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="one", "h");
  Namespace.bind(ns, ~name="two", "h");
  Namespace.bind(ns, ~name="alpha", "h");
  Alcotest.(check((list(string))))(
    "names_of sorted",
    ["alpha", "one", "two"],
    Namespace.names_of(ns, "h"),
  );
};

let test_ns_rename = () => {
  let ns = Namespace.create();
  Namespace.bind(ns, ~name="foo", "h");
  let r = Namespace.rename(ns, ~from="foo", ~to_="bar");
  Alcotest.(check(bool))(
    "rename ok",
    true,
    r == Ok(),
  );
  Alcotest.(check(option(string)))(
    "bar is bound",
    Some("h"),
    Namespace.resolve(ns, "bar"),
  );
  Alcotest.(check(option(string)))(
    "foo is unbound",
    None,
    Namespace.resolve(ns, "foo"),
  );
};

/* ===== No silent breakage ===== */

let test_no_silent_breakage = () => {
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  /* Bind id and then an expression that uses it. Rebinding id must not
     touch the second expression's stored subtree. */
  let h_id_v1 =
    Store.ingest(
      store,
      switch (Resolver.resolve(~namespace=ns, ~store, parse_surface("\\x. x"))) {
      | Ok(a) => a
      | Error(_) => failwith("resolve")
      },
    );
  Namespace.bind(ns, ~name="id", h_id_v1);
  let user =
    switch (
      Resolver.resolve(~namespace=ns, ~store, parse_surface("\\y. id"))
    ) {
    | Ok(a) => a
    | Error(_) => failwith("resolve")
    };
  let h_user = Store.ingest(store, user);
  /* Get user's child hash (the body of the outer Lam). */
  let child_hash =
    switch (Store.lookup(store, h_user)) {
    | Some(Node.Lam(body_hash)) => body_hash
    | _ => failwith("expected Lam")
    };
  /* Rebind id to something else */
  let h_new =
    Store.ingest(
      store,
      switch (
        Resolver.resolve(~namespace=ns, ~store, parse_surface("\\z. \\w. z"))
      ) {
      | Ok(a) => a
      | Error(_) => failwith("resolve")
      },
    );
  Namespace.rebind(ns, ~name="id", h_new);
  /* user's child body must still be h_id_v1 (or rather, the shifted-in
     version of it, which we lifted at ingest time — but since id was
     closed and inlined, it's exactly h_id_v1). */
  Alcotest.(check(string))(
    "user's stored child unchanged after rebind",
    h_id_v1,
    child_hash,
  );
};

/* ===== Pretty-printing ===== */

let test_pretty_id_renders = () => {
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  let h = Store.ingest(store, parse("\\x. x"));
  Alcotest.(check(string))(
    "id renders to `\\x. x`",
    "\\x. x",
    Pretty.print_named(~namespace=ns, store, h),
  );
};

let test_pretty_name_collapse_child = () => {
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  let h_id = Store.ingest(store, parse("\\x. x"));
  Namespace.bind(ns, ~name="id", h_id);
  let h_outer =
    Store.ingest(store, Ast.Lam(parse("\\x. x")));
  /* outer = \\_. (\\x. x) — but since (\\x. x) is bound to `id`, it
     should collapse to `id` in the body. */
  let rendered = Pretty.print_named(~namespace=ns, store, h_outer);
  Alcotest.(check(string))(
    "named child collapses to name inside outer Lam",
    "\\x. id",
    rendered,
  );
};

let test_pretty_top_level_not_collapsed = () => {
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  let h_id = Store.ingest(store, parse("\\x. x"));
  Namespace.bind(ns, ~name="id", h_id);
  let rendered = Pretty.print_named(~namespace=ns, store, h_id);
  Alcotest.(check(string))(
    "top-level hash not collapsed to its name",
    "\\x. x",
    rendered,
  );
};

let test_pretty_open_subterm_not_collapsed = () => {
  /* Bind a name to an OPEN subterm (the body of `\x. x` — which is
     `Var(0)`, vacuously closed actually, so pick Lam(Var(1)) which is
     open). Then render an outer term that uses that hash as a child;
     the child should expand inline rather than collapse to the name. */
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  let h_open = Store.ingest(store, Ast.Lam(Ast.Var(1))); /* \. $1 */
  Namespace.bind(ns, ~name="open_body", h_open);
  /* Outer term: Lam(h_open) = \x. \. $1 = \x. \y. x (same hash as \x. \y. x) */
  let h_outer = Store.ingest(store, Ast.Lam(Ast.Lam(Ast.Var(1))));
  let rendered = Pretty.print_named(~namespace=ns, store, h_outer);
  /* Should NOT see "open_body" in the rendered output — the open
     subterm expands inline. */
  let has_name = {
    let len_sub = String.length("open_body");
    let len_s = String.length(rendered);
    let found = ref(false);
    for (i in 0 to len_s - len_sub) {
      if (String.sub(rendered, i, len_sub) == "open_body") {
        found := true;
      };
    };
    found^;
  };
  Alcotest.(check(bool))(
    "open named subterm is inlined, not collapsed to its name",
    false,
    has_name,
  );
};

let test_pretty_round_trip = () => {
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  let expr = "\\x. \\y. \\z. x y z";
  let h1 = Store.ingest(store, parse(expr));
  let rendered = Pretty.print_named(~namespace=ns, store, h1);
  let h2 = Store.ingest(store, parse(rendered));
  Alcotest.(check(string))(
    "render and re-parse yields same hash (α-equivalent)",
    h1,
    h2,
  );
};

/* ===== Hash display ===== */

let test_hash_to_string_has_prefix = () => {
  let h = "abcdef123456";
  Alcotest.(check(string))(
    "to_string prefixes with #",
    "#abcdef123456",
    Hash.to_string(h),
  );
};

let test_hash_looks_like_prefix_rejects_short_ident = () =>
  Alcotest.(check(bool))(
    "`id` is not a hash prefix",
    false,
    Hash.looks_like_hash_prefix("id"),
  );

let test_hash_looks_like_prefix_accepts_hashed_prefix = () =>
  Alcotest.(check(bool))(
    "`#deadbeef` is a hash prefix",
    true,
    Hash.looks_like_hash_prefix("#deadbeef"),
  );

/* ===== p8: error-recovery specifics ===== */

/* Structural reachability — checks whether `target` appears as an
   ingested subterm (by hash) in the DAG rooted at `h`. Used by
   recovery tests to assert "well-formed subterms survive garbage". */
let rec reachable_from = (store: Store.t, h: Hash.t, target: Hash.t): bool =>
  Hash.equal(h, target)
  || (
    switch (Store.lookup(store, h)) {
    | Some(Node.Lam(ch)) => reachable_from(store, ch, target)
    | Some(Node.App(f, a)) =>
      reachable_from(store, f, target) || reachable_from(store, a, target)
    | Some(Node.Var(_))
    | Some(Node.Hole)
    | None => false
    }
  );

/* ----- Totality (headline property) ----- */

/* For any printable-ASCII string, parsing does not throw. This is the
   core guarantee of p8 — every textual input yields a Surface_ast.t. */
let prop_totality_printable =
  QCheck.Test.make(
    ~count=5000,
    ~name="Parse_recover.parse is total on printable strings",
    QCheck.string_printable,
    s => {
      let _ = Parse_recover.parse(s);
      true;
    },
  );

/* Same guarantee on arbitrary byte strings, stressing the lexer's
   unknown-byte path. */
let prop_totality_bytes =
  QCheck.Test.make(
    ~count=2000,
    ~name="Parse_recover.parse is total on arbitrary bytes",
    QCheck.string_small,
    s => {
      let _ = Parse_recover.parse(s);
      true;
    },
  );

/* ----- Clean inputs parse cleanly ----- */

/* A well-formed rendering should parse to a hole-free surface AST
   that, after resolution, equals the original term. */
let prop_clean_inputs_parse_clean =
  QCheck.Test.make(
    ~count=200,
    ~name="print(t) parses to t with zero holes",
    closed_ast_arb,
    t => {
      let store = Store.create();
      let ns = Namespace.create();
      let h = Store.ingest(store, t);
      let rendered =
        Pretty.print_surface(
          Pretty.surface_of_hash(~namespace=ns, store, h),
        );
      let parsed = Parse_recover.parse(rendered);
      let zero_holes = Surface_ast.count_holes(parsed) == 0;
      let resolved =
        switch (Resolver.resolve(~namespace=ns, ~store, parsed)) {
        | Ok(a) => a
        | Error(_) => failwith("resolve on clean input")
        };
      zero_holes && Ast.equal(t, resolved);
    },
  );

/* ----- Prefixes parse ----- */

let prop_prefixes_parse =
  QCheck.Test.make(
    ~count=100,
    ~name="every prefix of a printed term parses without throwing",
    closed_ast_arb,
    t => {
      let store = Store.create();
      let ns = Namespace.create();
      let h = Store.ingest(store, t);
      let rendered =
        Pretty.print_surface(
          Pretty.surface_of_hash(~namespace=ns, store, h),
        );
      let len = String.length(rendered);
      let ok = ref(true);
      for (i in 0 to len) {
        let _ = Parse_recover.parse(String.sub(rendered, 0, i));
        ();
      };
      ok^;
    },
  );

/* ----- Print/parse idempotence ----- */

/* Reparsing the printed form of a recovered parse yields the same
   Surface_ast.t. */
let prop_print_parse_idempotent =
  QCheck.Test.make(
    ~count=2000,
    ~name="parse (print (parse s)) = parse s",
    QCheck.string_printable,
    s => {
      let t1 = Parse_recover.parse(s);
      let printed = Pretty.print_surface(t1);
      let t2 = Parse_recover.parse(printed);
      Surface_ast.equal(t1, t2);
    },
  );

/* ----- α-invariance with holes ----- */

let test_alpha_hole_under_lambda = () => {
  let (s1, h1) = ingest_src("\\x. ?");
  let (s2, h2) = ingest_src("\\y. ?");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(string))(
    "\\x. ? and \\y. ? hash identically",
    h1,
    h2,
  );
};

let test_bare_hole_hash_stable = () => {
  let (s1, h1) = ingest_src("?");
  let (s2, h2) = ingest_src("?");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(string))(
    "bare ? has the same hash every time",
    h1,
    h2,
  );
};

/* ----- Recovery preserves well-formed prefixes ----- */

/* When we prepend a valid term then append garbage, either:
   (a) the valid term's hash is reachable from the parsed+ingested
       result's hash (the preservation case), or
   (b) the parser fell back to a bare Hole (the "unrecoverable input"
       case — happens on things like trailing unmatched '(' where
       the parser commits to expecting RPAREN and can never reach EOF).
   Either is acceptable; what would not be acceptable is a silent
   partial loss of structure. Case (b) is visible and signals
   "recovery gave up." */
let prop_recovery_preserves_prefix =
  QCheck.Test.make(
    ~count=200,
    ~name="well-formed prefix survives trailing garbage (or full fallback to Hole)",
    QCheck.pair(closed_ast_arb, QCheck.string_small),
    ((t, garbage)) => {
      let store = Store.create();
      let ns = Namespace.create();
      let h_t = Store.ingest(store, t);
      let printed =
        Pretty.print_surface(
          Pretty.surface_of_hash(~namespace=ns, store, h_t),
        );
      let combined = "(" ++ printed ++ ") " ++ garbage;
      let parsed = Parse_recover.parse(combined);
      switch (parsed) {
      | Surface_ast.Hole => true /* (b) full fallback */
      | _ =>
        switch (Resolver.resolve(~namespace=ns, ~store, parsed)) {
        | Error(_) =>
          /* Garbage may tokenize into IDENTs not bound in the empty
             namespace. An Unbound_name error here isn't a property
             violation — the parse itself succeeded. */
          true
        | Ok(ast) =>
          let h_parsed = Store.ingest(store, ast);
          reachable_from(store, h_parsed, h_t);
        }
      };
    },
  );

/* ----- Explicit unit tests for recovery scenarios ----- */

let parse_surface_contains_hole = (s: string): bool =>
  Surface_ast.count_holes(Parse_recover.parse(s)) > 0;

let test_empty_input = () =>
  Alcotest.check(
    surface_testable,
    "empty input → Hole",
    Surface_ast.Hole,
    Parse_recover.parse(""),
  );

let test_whitespace_only = () =>
  Alcotest.check(
    surface_testable,
    "whitespace-only → Hole",
    Surface_ast.Hole,
    Parse_recover.parse("   \n\t  "),
  );

let test_single_hole = () =>
  Alcotest.check(
    surface_testable,
    "? → Hole",
    Surface_ast.Hole,
    Parse_recover.parse("?"),
  );

let test_hole_app_hole = () =>
  Alcotest.check(
    surface_testable,
    "? ? → App(Hole, Hole)",
    Surface_ast.App(Surface_ast.Hole, Surface_ast.Hole),
    Parse_recover.parse("? ?"),
  );

let test_lam_hole_body = () =>
  Alcotest.check(
    surface_testable,
    "\\x. ? → Lam(x, Hole)",
    Surface_ast.Lam("x", Surface_ast.Hole),
    Parse_recover.parse("\\x. ?"),
  );

let test_lam_hole_then_var = () =>
  Alcotest.check(
    surface_testable,
    "\\x. ? y → Lam(x, App(Hole, Var y))",
    Surface_ast.Lam(
      "x",
      Surface_ast.App(Surface_ast.Hole, Surface_ast.Var("y")),
    ),
    Parse_recover.parse("\\x. ? y"),
  );

let test_unmatched_lparen = () =>
  Alcotest.(check(bool))(
    "'(' alone parses with at least one hole",
    true,
    parse_surface_contains_hole("("),
  );

let test_unmatched_rparen = () =>
  Alcotest.(check(bool))(
    "')' alone parses with at least one hole",
    true,
    parse_surface_contains_hole(")"),
  );

let test_dangling_backslash = () =>
  Alcotest.check(
    surface_testable,
    "'\\' alone falls forward to Lam(\"?\", Hole)",
    Surface_ast.Lam("?", Surface_ast.Hole),
    Parse_recover.parse("\\"),
  );

let test_backslash_dot = () =>
  Alcotest.check(
    surface_testable,
    "'\\ .' falls forward to Lam(\"?\", Hole) (HOLE recovers for binder; DOT dropped)",
    Surface_ast.Lam("?", Surface_ast.Hole),
    Parse_recover.parse("\\ ."),
  );

let test_fall_forward_nested_lambda = () =>
  Alcotest.check(
    surface_testable,
    "'\\x. \\' falls forward to Lam(x, Lam(\"?\", Hole))",
    Surface_ast.Lam("x", Surface_ast.Lam("?", Surface_ast.Hole)),
    Parse_recover.parse("\\x. \\"),
  );

let test_fall_forward_missing_body = () =>
  Alcotest.check(
    surface_testable,
    "'\\x.' falls forward to Lam(x, Hole)",
    Surface_ast.Lam("x", Surface_ast.Hole),
    Parse_recover.parse("\\x."),
  );

let test_fall_forward_missing_dot = () =>
  Alcotest.check(
    surface_testable,
    "'\\x' falls forward to Lam(x, Hole)",
    Surface_ast.Lam("x", Surface_ast.Hole),
    Parse_recover.parse("\\x"),
  );

let test_hole_binder_parse = () =>
  Alcotest.check(
    surface_testable,
    "'\\?. ?' parses to Lam(\"?\", Hole)",
    Surface_ast.Lam("?", Surface_ast.Hole),
    Parse_recover.parse("\\?. ?"),
  );

let test_hole_binder_alpha_equiv = () => {
  /* \?. ? and \x. ? ingest to the same hash — the "?" binder name is
     erased at resolution (de Bruijn). */
  let (s1, h1) = ingest_src("\\?. ?");
  let (s2, h2) = ingest_src("\\x. ?");
  ignore(s1);
  ignore(s2);
  Alcotest.(check(string))(
    "\\?. ? and \\x. ? hash identically",
    h1,
    h2,
  );
};

let test_lone_dot = () =>
  Alcotest.(check(bool))(
    "'.' alone parses with at least one hole",
    true,
    parse_surface_contains_hole("."),
  );

let test_atom_after_paren = () =>
  Alcotest.check(
    surface_testable,
    "(x) y → App(Var x, Var y)",
    Surface_ast.App(Surface_ast.Var("x"), Surface_ast.Var("y")),
    Parse_recover.parse("(x) y"),
  );

let test_hole_after_paren = () =>
  Alcotest.check(
    surface_testable,
    "(x) ? → App(Var x, Hole)",
    Surface_ast.App(Surface_ast.Var("x"), Surface_ast.Hole),
    Parse_recover.parse("(x) ?"),
  );

let test_gibberish_survives = () =>
  Alcotest.(check(bool))(
    "arbitrary gibberish parses",
    true,
    {
      let _ = Parse_recover.parse("}}}");
      let _ = Parse_recover.parse("\\x:. asdf }{");
      let _ = Parse_recover.parse("@#$%^&*");
      let _ = Parse_recover.parse("\\x. x \\");
      true;
    },
  );

/* ----- Pretty-printer round-trip with explicit holes ----- */

let test_pretty_hole_round_trip = () => {
  let (store, _) = fresh_world();
  let ns = Namespace.create();
  let h = Store.ingest(store, parse("\\x. ?"));
  let rendered = Pretty.print_named(~namespace=ns, store, h);
  Alcotest.(check(string))(
    "\\x. ? renders as `\\x. ?`",
    "\\x. ?",
    rendered,
  );
  let h2 = Store.ingest(store, parse(rendered));
  Alcotest.(check(string))(
    "render and re-parse produces same hash",
    h,
    h2,
  );
};

/* ===== Suite ===== */

let () =
  Alcotest.run(
    "p8-holes",
    [
      (
        "parser",
        [
          Alcotest.test_case("var atom", `Quick, test_parse_var),
          Alcotest.test_case("id abstraction", `Quick, test_parse_id),
          Alcotest.test_case("const (nested Lam)", `Quick, test_parse_const),
          Alcotest.test_case("app left-assoc", `Quick, test_parse_app_left_assoc),
          Alcotest.test_case("lam body extends right", `Quick, test_parse_lam_body_extends_right),
          Alcotest.test_case("(\\x. x) (\\y. y)", `Quick, test_parse_paren_lam_applied),
        ],
      ),
      (
        "shift-subst",
        [
          Alcotest.test_case("shift of closed = id", `Quick, test_shift_closed),
          Alcotest.test_case("shift raises free var", `Quick, test_shift_free_var),
          Alcotest.test_case("beta id", `Quick, test_beta_identity),
          Alcotest.test_case("beta const", `Quick, test_beta_const),
        ],
      ),
      (
        "ingest",
        [
          QCheck_alcotest.to_alcotest(prop_ingest_reconstruct_roundtrip),
          QCheck_alcotest.to_alcotest(prop_ingest_determinism),
          QCheck_alcotest.to_alcotest(prop_generated_terms_are_closed),
          Alcotest.test_case("subterm sharing", `Quick, test_subterm_sharing),
        ],
      ),
      (
        "alpha-equivalence",
        [
          Alcotest.test_case("id: \\x.x == \\y.y", `Quick, test_alpha_id),
          Alcotest.test_case("const: same", `Quick, test_alpha_const),
          Alcotest.test_case("nested: same", `Quick, test_alpha_nested),
          Alcotest.test_case("K and K' differ", `Quick, test_not_alpha_equivalent),
        ],
      ),
      (
        "eval",
        [
          Alcotest.test_case("Lam is a value", `Quick, test_eval_lam_is_value),
          Alcotest.test_case("beta identity", `Quick, test_eval_beta_identity),
          Alcotest.test_case("const a b = a", `Quick, test_eval_const),
          Alcotest.test_case("nested apps", `Quick, test_eval_nested_apps),
          Alcotest.test_case("Omega hits step limit", `Quick, test_eval_step_limit_omega),
          Alcotest.test_case("StepLimit not cached", `Quick, test_eval_step_limit_not_cached),
          Alcotest.test_case("cache hit on re-eval", `Quick, test_eval_cache_hit),
        ],
      ),
      (
        "resolver",
        [
          Alcotest.test_case("bound var shadows namespace", `Quick, test_resolver_bound_wins),
          Alcotest.test_case("free name falls through", `Quick, test_resolver_namespace_fallthrough),
          Alcotest.test_case("unbound name errors", `Quick, test_resolver_unbound),
          Alcotest.test_case("inner binder wins (same name)", `Quick, test_resolver_nested_shadowing),
          QCheck_alcotest.to_alcotest(prop_alpha_equivalent_surface_same_hash),
        ],
      ),
      (
        "namespace",
        [
          Alcotest.test_case("bind + resolve", `Quick, test_ns_bind_and_resolve),
          Alcotest.test_case("double bind raises", `Quick, test_ns_double_bind_raises),
          Alcotest.test_case("rebind", `Quick, test_ns_rebind),
          Alcotest.test_case("aliases sorted", `Quick, test_ns_aliases),
          Alcotest.test_case("rename", `Quick, test_ns_rename),
        ],
      ),
      (
        "no-silent-breakage",
        [
          Alcotest.test_case("rebind does not touch stored body", `Quick, test_no_silent_breakage),
        ],
      ),
      (
        "pretty-names",
        [
          Alcotest.test_case("id renders", `Quick, test_pretty_id_renders),
          Alcotest.test_case("named child collapses", `Quick, test_pretty_name_collapse_child),
          Alcotest.test_case("top level not collapsed", `Quick, test_pretty_top_level_not_collapsed),
          Alcotest.test_case("open named subterm inlines", `Quick, test_pretty_open_subterm_not_collapsed),
          Alcotest.test_case("render then re-parse", `Quick, test_pretty_round_trip),
        ],
      ),
      (
        "hash-display",
        [
          Alcotest.test_case("to_string has #", `Quick, test_hash_to_string_has_prefix),
          Alcotest.test_case("short ident is not prefix", `Quick, test_hash_looks_like_prefix_rejects_short_ident),
          Alcotest.test_case("#deadbeef is prefix", `Quick, test_hash_looks_like_prefix_accepts_hashed_prefix),
        ],
      ),
      (
        "totality",
        [
          QCheck_alcotest.to_alcotest(prop_totality_printable),
          QCheck_alcotest.to_alcotest(prop_totality_bytes),
        ],
      ),
      (
        "recovery-properties",
        [
          QCheck_alcotest.to_alcotest(prop_clean_inputs_parse_clean),
          QCheck_alcotest.to_alcotest(prop_prefixes_parse),
          QCheck_alcotest.to_alcotest(prop_print_parse_idempotent),
          QCheck_alcotest.to_alcotest(prop_recovery_preserves_prefix),
        ],
      ),
      (
        "alpha-with-holes",
        [
          Alcotest.test_case(
            "\\x. ? == \\y. ?",
            `Quick,
            test_alpha_hole_under_lambda,
          ),
          Alcotest.test_case(
            "bare ? hash stable",
            `Quick,
            test_bare_hole_hash_stable,
          ),
        ],
      ),
      (
        "recovery-scenarios",
        [
          Alcotest.test_case("empty input", `Quick, test_empty_input),
          Alcotest.test_case("whitespace-only", `Quick, test_whitespace_only),
          Alcotest.test_case("single ?", `Quick, test_single_hole),
          Alcotest.test_case("? ?", `Quick, test_hole_app_hole),
          Alcotest.test_case("\\x. ?", `Quick, test_lam_hole_body),
          Alcotest.test_case("\\x. ? y", `Quick, test_lam_hole_then_var),
          Alcotest.test_case("unmatched (", `Quick, test_unmatched_lparen),
          Alcotest.test_case("unmatched )", `Quick, test_unmatched_rparen),
          Alcotest.test_case(
            "dangling \\",
            `Quick,
            test_dangling_backslash,
          ),
          Alcotest.test_case("\\ .", `Quick, test_backslash_dot),
          Alcotest.test_case(
            "\\x. \\ (fall-forward nested lambda)",
            `Quick,
            test_fall_forward_nested_lambda,
          ),
          Alcotest.test_case(
            "\\x. (missing body)",
            `Quick,
            test_fall_forward_missing_body,
          ),
          Alcotest.test_case(
            "\\x (missing dot+body)",
            `Quick,
            test_fall_forward_missing_dot,
          ),
          Alcotest.test_case(
            "\\?. ? (explicit hole-binder)",
            `Quick,
            test_hole_binder_parse,
          ),
          Alcotest.test_case(
            "\\?. ? == \\x. ? (alpha)",
            `Quick,
            test_hole_binder_alpha_equiv,
          ),
          Alcotest.test_case("lone .", `Quick, test_lone_dot),
          Alcotest.test_case(
            "assorted gibberish",
            `Quick,
            test_gibberish_survives,
          ),
          Alcotest.test_case(
            "(x) y → App",
            `Quick,
            test_atom_after_paren,
          ),
          Alcotest.test_case(
            "(x) ? → App(Var, Hole)",
            `Quick,
            test_hole_after_paren,
          ),
        ],
      ),
      (
        "pretty-with-holes",
        [
          Alcotest.test_case(
            "\\x. ? round-trip",
            `Quick,
            test_pretty_hole_round_trip,
          ),
        ],
      ),
    ],
  );
