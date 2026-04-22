open P5_multi_language;

/* ==================== Helpers ==================== */

let fresh_world = () => {
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Arith_eval.descriptor);
  Attachment.register_descriptor(att, Lc_eval.descriptor);
  Attachment.register_descriptor(att, Arith_to_lc_church.descriptor);
  (store, att);
};

let parse_arith = (s: string): Arith_ast.t => {
  let lexbuf = Lexing.from_string(s);
  let surface = Arith_parser.main(Arith_lexer.token, lexbuf);
  switch (
    Resolver.resolve_arith(
      ~namespace=Namespace.create(),
      ~store=Store.create(),
      surface,
    )
  ) {
  | Ok(ast) => ast
  | Error(e) => failwith("parse_arith: " ++ Resolver.error_to_string(e))
  };
};

let parse_lc = (s: string): Lc_ast.t => {
  let lexbuf = Lexing.from_string(s);
  let surface = Lc_parser.main(Lc_lexer.token, lexbuf);
  switch (
    Resolver.resolve_lc(
      ~namespace=Namespace.create(),
      ~store=Store.create(),
      surface,
    )
  ) {
  | Ok(ast) => ast
  | Error(e) => failwith("parse_lc: " ++ Resolver.error_to_string(e))
  };
};

let ingest_arith = (store, src) =>
  Store.ingest_arith(store, parse_arith(src));

let ingest_lc = (store, src) => Store.ingest_lc(store, parse_lc(src));

/* Deep β-normalizer for lc terms. Normal-order: outermost redexes first,
   recurses under binders. Used only in tests because Lc_eval stops at
   weak-head normal form and the translator's correctness is a β-normal
   property (Church numerals are the NF, not the WHNF, of their
   construction).

   Terminates for strongly normalizing inputs (Church-encoded output of
   the translator is SN for any closed arith source). */
let rec deep_normalize = (t: Lc_ast.t): Lc_ast.t =>
  switch (t) {
  | Lc_ast.Var(_) => t
  | Lc_ast.Lam(body) => Lc_ast.Lam(deep_normalize(body))
  | Lc_ast.App(f, a) =>
    let f' = deep_normalize(f);
    let a' = deep_normalize(a);
    switch (f') {
    | Lc_ast.Lam(body) => deep_normalize(Lc_ast.beta(~body, ~arg=a'))
    | _ => Lc_ast.App(f', a')
    }
  };

let translate_to_nf = (store, att, h_arith): Lc_ast.t =>
  switch (Arith_to_lc_church.translate(~store, ~att, h_arith)) {
  | Error(e) =>
    failwith("translate: " ++ Arith_to_lc_church.error_to_string(e))
  | Ok((target, _)) =>
    switch (Store.reconstruct_lc(store, target)) {
    | None => failwith("reconstruct_lc returned None")
    | Some(ast) => deep_normalize(ast)
    }
  };

let lc_nf_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Lc_ast.show(t)),
    Lc_ast.equal,
  );

/* Pre-canonicalized (β-normal) Church numerals. \f. \x. f^n x. */
let church_nf_zero: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(0)));
let church_nf_one: Lc_ast.t =
  Lc_ast.Lam(Lc_ast.Lam(Lc_ast.App(Lc_ast.Var(1), Lc_ast.Var(0))));
let church_nf_two: Lc_ast.t =
  Lc_ast.Lam(
    Lc_ast.Lam(
      Lc_ast.App(Lc_ast.Var(1), Lc_ast.App(Lc_ast.Var(1), Lc_ast.Var(0))),
    ),
  );
let church_nf_true: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(1)));
let church_nf_false: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(0)));

/* ==================== Disjoint hash spaces ==================== */

let test_language_tags_disjoin_hashes = () => {
  /* An arith True and any lc term would never collide normally (different
     shapes), but make sure two same-shape nodes (no children) still land in
     disjoint spaces. The smallest hashable arith node is True; the
     smallest lc node is Var(0). Their pre-tag bytes differ already, but
     the *language prefix* is the primary guarantee. */
  let store = Store.create();
  let h_arith_true = Store.ingest_arith(store, Arith_ast.True);
  let h_lc_var0 = Store.ingest_lc(store, Lc_ast.Var(0));
  Alcotest.(check(bool))(
    "arith True and lc Var(0) hash distinctly",
    true,
    !Hash.equal(h_arith_true, h_lc_var0),
  );
  /* And both remain retrievable via language_of. */
  Alcotest.(check(option(string)))(
    "arith hash reports language 'arith'",
    Some("arith"),
    Store.language_of(store, h_arith_true),
  );
  Alcotest.(check(option(string)))(
    "lc hash reports language 'lc'",
    Some("lc"),
    Store.language_of(store, h_lc_var0),
  );
};

/* ==================== Cross-language reference rejection ==================== */

let test_cross_language_reject_arith_child = () => {
  let store = Store.create();
  let h_lc = Store.ingest_lc(store, Lc_ast.Lam(Lc_ast.Var(0)));
  /* Try to register Arith_node.Succ(h_lc) by hand: should raise. */
  let threw =
    switch (Store.register_arith_node(store, Arith_node.Succ(h_lc))) {
    | exception (Store.Language_mismatch(_, _, _)) => true
    | _ => false
    };
  Alcotest.(check(bool))(
    "arith parent rejects lc child",
    true,
    threw,
  );
};

let test_cross_language_reject_lc_child = () => {
  let store = Store.create();
  let h_arith = Store.ingest_arith(store, Arith_ast.Zero);
  /* Try to register Lc_node.Lam(h_arith) by hand: should raise. */
  let threw =
    switch (Store.register_lc_node(store, Lc_node.Lam(h_arith))) {
    | exception (Store.Language_mismatch(_, _, _)) => true
    | _ => false
    };
  Alcotest.(check(bool))(
    "lc parent rejects arith child",
    true,
    threw,
  );
};

/* ==================== Ingest / reconstruct roundtrip per language ==================== */

let arith_ast_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Arith_ast.show(t)),
    Arith_ast.equal,
  );

let lc_ast_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Lc_ast.show(t)),
    Lc_ast.equal,
  );

let test_arith_roundtrip = () => {
  let store = Store.create();
  let ast = parse_arith("if iszero (succ 0) then 0 else pred 0");
  let h = Store.ingest_arith(store, ast);
  switch (Store.reconstruct_arith(store, h)) {
  | Some(t') =>
    Alcotest.check(arith_ast_testable, "arith roundtrip", ast, t')
  | None => Alcotest.fail("reconstruct_arith returned None")
  };
};

let test_lc_roundtrip = () => {
  let store = Store.create();
  let ast = parse_lc("\\f. \\x. f (f x)");
  let h = Store.ingest_lc(store, ast);
  switch (Store.reconstruct_lc(store, h)) {
  | Some(t') => Alcotest.check(lc_ast_testable, "lc roundtrip", ast, t')
  | None => Alcotest.fail("reconstruct_lc returned None")
  };
};

let test_reconstruct_cross_language_returns_none = () => {
  let store = Store.create();
  let h_arith = Store.ingest_arith(store, Arith_ast.Zero);
  Alcotest.(check(bool))(
    "reconstruct_lc on arith hash is None",
    true,
    Store.reconstruct_lc(store, h_arith) == None,
  );
  let h_lc = Store.ingest_lc(store, Lc_ast.Var(0));
  Alcotest.(check(bool))(
    "reconstruct_arith on lc hash is None",
    true,
    Store.reconstruct_arith(store, h_lc) == None,
  );
};

/* ==================== α-equivalence carried from p4 ==================== */

let test_alpha_lc_still_works = () => {
  let store = Store.create();
  let h1 = Store.ingest_lc(store, parse_lc("\\x. x"));
  let h2 = Store.ingest_lc(store, parse_lc("\\y. y"));
  Alcotest.(check(string))(
    "\\x. x and \\y. y share a hash",
    h1,
    h2,
  );
};

/* ==================== Resolver language guard ==================== */

let test_resolver_language_mismatch_arith_expects_arith = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_id = Store.ingest_lc(store, parse_lc("\\x. x"));
  Namespace.bind(ns, ~name="id", h_id);
  /* Try to resolve `succ id` as arith — id is lc, so mismatch. */
  let lexbuf = Lexing.from_string("succ id");
  let surface = Arith_parser.main(Arith_lexer.token, lexbuf);
  switch (Resolver.resolve_arith(~namespace=ns, ~store, surface)) {
  | Error(Resolver.Language_mismatch(n, expected, actual)) =>
    Alcotest.(check(string))("mismatched name", "id", n);
    Alcotest.(check(string))("expected arith", "arith", expected);
    Alcotest.(check(string))("actual lc", "lc", actual);
  | _ => Alcotest.fail("expected Language_mismatch")
  };
};

let test_resolver_language_mismatch_lc_expects_lc = () => {
  let store = Store.create();
  let ns = Namespace.create();
  let h_one = Store.ingest_arith(store, parse_arith("succ 0"));
  Namespace.bind(ns, ~name="one", h_one);
  /* Try to resolve `\x. one` as lc — one is arith, so mismatch. */
  let lexbuf = Lexing.from_string("\\x. one");
  let surface = Lc_parser.main(Lc_lexer.token, lexbuf);
  switch (Resolver.resolve_lc(~namespace=ns, ~store, surface)) {
  | Error(Resolver.Language_mismatch(n, expected, actual)) =>
    Alcotest.(check(string))("mismatched name", "one", n);
    Alcotest.(check(string))("expected lc", "lc", expected);
    Alcotest.(check(string))("actual arith", "arith", actual);
  | _ => Alcotest.fail("expected Language_mismatch")
  };
};

/* ==================== Arith eval, carried from p3 ==================== */

let test_arith_eval_simple = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "if iszero 0 then succ 0 else 0");
  let h_expected = ingest_arith(store, "succ 0");
  switch (Arith_eval.eval(~store, ~att, h)) {
  | Arith_eval.Value(h') =>
    Alcotest.(check(string))("arith eval agrees", h_expected, h')
  | _ => Alcotest.fail("expected Value")
  };
};

/* ==================== Lc eval, carried from p4 ==================== */

let test_lc_eval_simple = () => {
  let (store, att) = fresh_world();
  let h_id = ingest_lc(store, "\\x. x");
  let h_id_id = ingest_lc(store, "(\\x. x) (\\y. y)");
  switch (Lc_eval.eval(~store, ~att, h_id_id)) {
  | Lc_eval.Value(h') =>
    Alcotest.(check(string))("id id ⇒ id", h_id, h')
  | _ => Alcotest.fail("expected Value")
  };
};

/* ==================== Church-encoding translator ====================

   Translator correctness is a β-normal-form property: the translated
   term, fully β-reduced (under binders), should be the canonical
   Church representation of the arith value. CBV WHNF stops under
   binders, so these tests compare deep-normalized forms via
   `deep_normalize` rather than `Lc_eval.eval`. */

let test_translate_zero = () => {
  let (store, att) = fresh_world();
  let h_zero = Store.ingest_arith(store, Arith_ast.Zero);
  Alcotest.check(
    lc_nf_testable,
    "Zero normalizes to Church zero",
    church_nf_zero,
    translate_to_nf(store, att, h_zero),
  );
};

let test_translate_true = () => {
  let (store, att) = fresh_world();
  let h_true = Store.ingest_arith(store, Arith_ast.True);
  Alcotest.check(
    lc_nf_testable,
    "True normalizes to Church true",
    church_nf_true,
    translate_to_nf(store, att, h_true),
  );
};

let test_translate_false = () => {
  let (store, att) = fresh_world();
  let h_false = Store.ingest_arith(store, Arith_ast.False);
  Alcotest.check(
    lc_nf_testable,
    "False normalizes to Church false",
    church_nf_false,
    translate_to_nf(store, att, h_false),
  );
};

let test_translate_succ_zero = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "succ 0");
  Alcotest.check(
    lc_nf_testable,
    "succ 0 normalizes to Church one",
    church_nf_one,
    translate_to_nf(store, att, h),
  );
};

let test_translate_succ_succ_zero = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "succ (succ 0)");
  Alcotest.check(
    lc_nf_testable,
    "succ (succ 0) normalizes to Church two",
    church_nf_two,
    translate_to_nf(store, att, h),
  );
};

let test_translate_iszero_true_case = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "iszero 0");
  Alcotest.check(
    lc_nf_testable,
    "iszero 0 normalizes to Church true",
    church_nf_true,
    translate_to_nf(store, att, h),
  );
};

let test_translate_iszero_false_case = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "iszero (succ 0)");
  Alcotest.check(
    lc_nf_testable,
    "iszero (succ 0) normalizes to Church false",
    church_nf_false,
    translate_to_nf(store, att, h),
  );
};

let test_translate_pred_succ_zero = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "pred (succ 0)");
  Alcotest.check(
    lc_nf_testable,
    "pred (succ 0) normalizes to Church zero",
    church_nf_zero,
    translate_to_nf(store, att, h),
  );
};

let test_translate_if_true_branch = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "if true then 0 else succ 0");
  Alcotest.check(
    lc_nf_testable,
    "if true then 0 else succ 0 normalizes to Church zero",
    church_nf_zero,
    translate_to_nf(store, att, h),
  );
};

let test_translate_if_false_branch = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "if false then 0 else succ 0");
  Alcotest.check(
    lc_nf_testable,
    "if false then 0 else succ 0 normalizes to Church one",
    church_nf_one,
    translate_to_nf(store, att, h),
  );
};

let test_translate_iszero_nested_in_if = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "if iszero (succ 0) then 0 else succ 0");
  Alcotest.check(
    lc_nf_testable,
    "conditional on a false iszero picks else branch",
    church_nf_one,
    translate_to_nf(store, att, h),
  );
};

/* ==================== Translation cache ==================== */

let test_translate_cache_hit = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "succ (succ 0)");
  let first =
    switch (Arith_to_lc_church.translate(~store, ~att, h)) {
    | Ok((target, was_cached)) =>
      Alcotest.(check(bool))("first call is not cached", false, was_cached);
      target;
    | Error(_) => Alcotest.fail("first translate failed") |> ignore |> (() => "")
    };
  let second =
    switch (Arith_to_lc_church.translate(~store, ~att, h)) {
    | Ok((target, was_cached)) =>
      Alcotest.(check(bool))("second call IS cached", true, was_cached);
      target;
    | Error(_) => Alcotest.fail("second translate failed") |> ignore |> (() => "")
    };
  Alcotest.(check(string))(
    "cached target equals first target",
    first,
    second,
  );
};

let test_translate_aspect_recorded = () => {
  let (store, att) = fresh_world();
  let h = ingest_arith(store, "succ 0");
  let target =
    switch (Arith_to_lc_church.translate(~store, ~att, h)) {
    | Ok((t, _)) => t
    | Error(_) =>
      Alcotest.fail("translate failed") |> ignore |> (() => "")
    };
  switch (
    Attachment.peek(
      att,
      ~target=h,
      ~aspect=Arith_to_lc_church.aspect_id,
      ~procedure=Arith_to_lc_church.procedure_id,
    )
  ) {
  | Some(Attachment.Translation_target(recorded)) =>
    Alcotest.(check(string))(
      "recorded aspect value matches target",
      target,
      recorded,
    )
  | _ => Alcotest.fail("expected Translation_target aspect entry")
  };
};

let test_translate_rejects_lc_source = () => {
  let (store, att) = fresh_world();
  let h_lc = Store.ingest_lc(store, parse_lc("\\x. x"));
  switch (Arith_to_lc_church.translate(~store, ~att, h_lc)) {
  | Error(Arith_to_lc_church.Not_arith(_)) =>
    Alcotest.(check(bool))("not-arith error returned", true, true)
  | _ => Alcotest.fail("expected Not_arith")
  };
};

let test_translations_listing = () => {
  let (store, att) = fresh_world();
  let h1 = ingest_arith(store, "succ 0");
  let h2 = ingest_arith(store, "iszero 0");
  let _ = Arith_to_lc_church.translate(~store, ~att, h1);
  let _ = Arith_to_lc_church.translate(~store, ~att, h2);
  let pairs = Arith_to_lc_church.all_translations(att);
  Alcotest.(check(int))(
    "exactly two translations recorded",
    2,
    List.length(pairs),
  );
};

/* ==================== Property: arith eval agrees with lc eval after translation ==================== */

let rec arith_ast_gen = (depth): QCheck.Gen.t(Arith_ast.t) => {
  open QCheck.Gen;
  let leaves = oneof([return(Arith_ast.True), return(Arith_ast.False), return(Arith_ast.Zero)]);
  if (depth <= 0) {
    leaves;
  } else {
    let sub = arith_ast_gen(depth - 1);
    oneof_weighted([
      (3, leaves),
      (2, map(a => Arith_ast.Succ(a), sub)),
      (2, map(a => Arith_ast.Pred(a), sub)),
      (2, map(a => Arith_ast.IsZero(a), sub)),
      (2, map3((c, t, e) => Arith_ast.If(c, t, e), sub, sub, sub)),
    ]);
  };
};

let arith_ast_arb = QCheck.make(~print=Arith_ast.show, arith_ast_gen(3));

let prop_arith_eval_commutes_with_translation =
  QCheck.Test.make(
    ~count=60,
    ~name="deep_normalize(translate(t)) = deep_normalize(translate(arith_eval(t)))",
    arith_ast_arb,
    t => {
      let (store, att) = fresh_world();
      let h = Store.ingest_arith(store, t);
      switch (Arith_eval.eval(~store, ~att, h)) {
      | Arith_eval.Stuck(_) => true /* only compare on converging arith values */
      | Arith_eval.Value(v_arith) =>
        let nf_whole = translate_to_nf(store, att, h);
        let nf_value = translate_to_nf(store, att, v_arith);
        Lc_ast.equal(nf_whole, nf_value);
      };
    },
  );

/* ==================== Runner ==================== */

let () =
  Alcotest.run(
    "p5-multi-language",
    [
      (
        "disjoint-hashes",
        [
          Alcotest.test_case(
            "language tags keep hashes disjoint",
            `Quick,
            test_language_tags_disjoin_hashes,
          ),
        ],
      ),
      (
        "cross-language-rejection",
        [
          Alcotest.test_case(
            "arith parent rejects lc child",
            `Quick,
            test_cross_language_reject_arith_child,
          ),
          Alcotest.test_case(
            "lc parent rejects arith child",
            `Quick,
            test_cross_language_reject_lc_child,
          ),
        ],
      ),
      (
        "roundtrip",
        [
          Alcotest.test_case("arith roundtrip", `Quick, test_arith_roundtrip),
          Alcotest.test_case("lc roundtrip", `Quick, test_lc_roundtrip),
          Alcotest.test_case(
            "cross-language reconstruct returns None",
            `Quick,
            test_reconstruct_cross_language_returns_none,
          ),
        ],
      ),
      (
        "alpha-equivalence",
        [
          Alcotest.test_case(
            "lc α-equivalence still works",
            `Quick,
            test_alpha_lc_still_works,
          ),
        ],
      ),
      (
        "resolver",
        [
          Alcotest.test_case(
            "arith resolver rejects lc-bound names",
            `Quick,
            test_resolver_language_mismatch_arith_expects_arith,
          ),
          Alcotest.test_case(
            "lc resolver rejects arith-bound names",
            `Quick,
            test_resolver_language_mismatch_lc_expects_lc,
          ),
        ],
      ),
      (
        "eval",
        [
          Alcotest.test_case("arith eval works", `Quick, test_arith_eval_simple),
          Alcotest.test_case("lc eval works", `Quick, test_lc_eval_simple),
        ],
      ),
      (
        "church-translation",
        [
          Alcotest.test_case("Zero → Church zero", `Quick, test_translate_zero),
          Alcotest.test_case("True → Church true", `Quick, test_translate_true),
          Alcotest.test_case("False → Church false", `Quick, test_translate_false),
          Alcotest.test_case("succ 0 → Church one", `Quick, test_translate_succ_zero),
          Alcotest.test_case(
            "succ (succ 0) → Church two",
            `Quick,
            test_translate_succ_succ_zero,
          ),
          Alcotest.test_case(
            "iszero 0 → Church true",
            `Quick,
            test_translate_iszero_true_case,
          ),
          Alcotest.test_case(
            "iszero (succ 0) → Church false",
            `Quick,
            test_translate_iszero_false_case,
          ),
          Alcotest.test_case(
            "pred (succ 0) → Church zero",
            `Quick,
            test_translate_pred_succ_zero,
          ),
          Alcotest.test_case(
            "if true then 0 else succ 0",
            `Quick,
            test_translate_if_true_branch,
          ),
          Alcotest.test_case(
            "if false then 0 else succ 0",
            `Quick,
            test_translate_if_false_branch,
          ),
          Alcotest.test_case(
            "if iszero (succ 0) then 0 else succ 0",
            `Quick,
            test_translate_iszero_nested_in_if,
          ),
        ],
      ),
      (
        "translation-cache",
        [
          Alcotest.test_case(
            "second translate is a cache hit",
            `Quick,
            test_translate_cache_hit,
          ),
          Alcotest.test_case(
            "translation aspect is recorded",
            `Quick,
            test_translate_aspect_recorded,
          ),
          Alcotest.test_case(
            "translator rejects non-arith source",
            `Quick,
            test_translate_rejects_lc_source,
          ),
          Alcotest.test_case(
            "all_translations lists cached pairs",
            `Quick,
            test_translations_listing,
          ),
        ],
      ),
      (
        "translation-property",
        [
          QCheck_alcotest.to_alcotest(prop_arith_eval_commutes_with_translation),
        ],
      ),
    ],
  );
