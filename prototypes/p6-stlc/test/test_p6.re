open P6_stlc;

/* ==================== Helpers ==================== */

let fresh_world = () => {
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Lc_eval.descriptor);
  Attachment.register_descriptor(att, Stlc_eval.descriptor);
  Attachment.register_descriptor(att, Stlc_typecheck.descriptor);
  Attachment.register_descriptor(att, Stlc_to_lc_erase_church.descriptor);
  Attachment.register_descriptor(att, Lc_to_stlc_check.descriptor);
  (store, att);
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

let parse_stlc = (s: string): Stlc_ast.t => {
  let lexbuf = Lexing.from_string(s);
  let surface = Stlc_parser.main(Stlc_lexer.token, lexbuf);
  switch (
    Resolver.resolve_stlc(
      ~namespace=Namespace.create(),
      ~store=Store.create(),
      surface,
    )
  ) {
  | Ok(ast) => ast
  | Error(e) => failwith("parse_stlc: " ++ Resolver.error_to_string(e))
  };
};

let ingest_lc = (store, src) => Store.ingest_lc(store, parse_lc(src));
let ingest_stlc = (store, src) =>
  Store.ingest_stlc(store, parse_stlc(src));

/* Deep β-normalizer for pure untyped lc. Inherits from p5. */
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

let lc_nf_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Lc_ast.show(t)),
    Lc_ast.equal,
  );

let stlc_ast_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Stlc_ast.show(t)),
    Stlc_ast.equal,
  );

let ty_testable =
  Alcotest.testable(
    (fmt, t) => Format.fprintf(fmt, "%s", Ty.print(t)),
    Ty.equal,
  );

let church_nf_true: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(1)));
let church_nf_false: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(0)));

let erase_and_normalize = (store, att, h_stlc): Lc_ast.t =>
  switch (Stlc_to_lc_erase_church.translate(~store, ~att, h_stlc)) {
  | Error(e) =>
    failwith(
      "erase translate: " ++ Stlc_to_lc_erase_church.error_to_string(e),
    )
  | Ok((target, _)) =>
    switch (Store.reconstruct_lc(store, target)) {
    | None => failwith("reconstruct_lc returned None")
    | Some(ast) => deep_normalize(ast)
    }
  };

/* ==================== Disjoint hash spaces ==================== */

let test_language_tags_disjoint = () => {
  let store = Store.create();
  let h_stlc = Store.ingest_stlc(store, Stlc_ast.True);
  let h_lc = Store.ingest_lc(store, Lc_ast.Var(0));
  Alcotest.(check(bool))(
    "stlc and lc hashes distinct",
    true,
    !Hash.equal(h_stlc, h_lc),
  );
  Alcotest.(check(option(string)))(
    "stlc hash reports 'stlc'",
    Some("stlc"),
    Store.language_of(store, h_stlc),
  );
  Alcotest.(check(option(string)))(
    "lc hash reports 'lc'",
    Some("lc"),
    Store.language_of(store, h_lc),
  );
};

/* ==================== Cross-language reference rejection ==================== */

let test_reject_stlc_parent_with_lc_child = () => {
  let store = Store.create();
  let h_lc = Store.ingest_lc(store, Lc_ast.Lam(Lc_ast.Var(0)));
  let threw =
    switch (
      Store.register_stlc_node(store, Stlc_node.Lam(Ty.Bool, h_lc))
    ) {
    | exception (Store.Language_mismatch(_, _, _)) => true
    | _ => false
    };
  Alcotest.(check(bool))("stlc parent rejects lc child", true, threw);
};

let test_reject_lc_parent_with_stlc_child = () => {
  let store = Store.create();
  let h_stlc = Store.ingest_stlc(store, Stlc_ast.True);
  let threw =
    switch (Store.register_lc_node(store, Lc_node.Lam(h_stlc))) {
    | exception (Store.Language_mismatch(_, _, _)) => true
    | _ => false
    };
  Alcotest.(check(bool))("lc parent rejects stlc child", true, threw);
};

/* ==================== Roundtrips ==================== */

let test_lc_roundtrip = () => {
  let store = Store.create();
  let ast = parse_lc("\\f. \\x. f (f x)");
  let h = Store.ingest_lc(store, ast);
  switch (Store.reconstruct_lc(store, h)) {
  | Some(t') => Alcotest.check(lc_nf_testable, "lc roundtrip", ast, t')
  | None => Alcotest.fail("reconstruct_lc returned None")
  };
};

let test_stlc_roundtrip = () => {
  let store = Store.create();
  let ast = parse_stlc("\\x:Bool. if x then false else true");
  let h = Store.ingest_stlc(store, ast);
  switch (Store.reconstruct_stlc(store, h)) {
  | Some(t') =>
    Alcotest.check(stlc_ast_testable, "stlc roundtrip", ast, t')
  | None => Alcotest.fail("reconstruct_stlc returned None")
  };
};

/* ==================== α-equivalence (lc, carried from p4) ==================== */

let test_alpha_lc = () => {
  let store = Store.create();
  let h1 = Store.ingest_lc(store, parse_lc("\\x. x"));
  let h2 = Store.ingest_lc(store, parse_lc("\\y. y"));
  Alcotest.(check(string))("\\x. x and \\y. y share a hash", h1, h2);
};

let test_alpha_stlc = () => {
  let store = Store.create();
  let h1 = Store.ingest_stlc(store, parse_stlc("\\x:Bool. x"));
  let h2 = Store.ingest_stlc(store, parse_stlc("\\y:Bool. y"));
  Alcotest.(check(string))(
    "\\x:Bool. x and \\y:Bool. y share a hash",
    h1,
    h2,
  );
};

let test_stlc_different_annotations_different_hashes = () => {
  let store = Store.create();
  let h1 = Store.ingest_stlc(store, parse_stlc("\\x:Bool. x"));
  let h2 =
    Store.ingest_stlc(store, parse_stlc("\\x:Bool -> Bool. x"));
  Alcotest.(check(bool))(
    "binder annotation participates in hash",
    true,
    !Hash.equal(h1, h2),
  );
};

/* ==================== Stlc resolver enforces well-typedness ==================== */

let test_stlc_ingest_rejects_bad_if = () => {
  /* `if (\x:Bool. x) then true else false` — guard is a function, not Bool. */
  let lexbuf =
    Lexing.from_string("if (\\x:Bool. x) then true else false");
  let surface = Stlc_parser.main(Stlc_lexer.token, lexbuf);
  switch (
    Resolver.resolve_stlc(
      ~namespace=Namespace.create(),
      ~store=Store.create(),
      surface,
    )
  ) {
  | Error(Resolver.Type_error(_)) =>
    Alcotest.(check(bool))("type error reported", true, true)
  | _ => Alcotest.fail("expected Type_error")
  };
};

let test_stlc_ingest_rejects_branch_mismatch = () => {
  let lexbuf =
    Lexing.from_string("if true then true else (\\x:Bool. x)");
  let surface = Stlc_parser.main(Stlc_lexer.token, lexbuf);
  switch (
    Resolver.resolve_stlc(
      ~namespace=Namespace.create(),
      ~store=Store.create(),
      surface,
    )
  ) {
  | Error(Resolver.Type_error(_)) =>
    Alcotest.(check(bool))("type error reported", true, true)
  | _ => Alcotest.fail("expected Type_error")
  };
};

/* ==================== Lc/Stlc eval ==================== */

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

let test_stlc_eval_not_true = () => {
  let (store, att) = fresh_world();
  /* not = \x:Bool. if x then false else true */
  let h_not =
    ingest_stlc(store, "\\x:Bool. if x then false else true");
  let h_true = ingest_stlc(store, "true");
  let h_false = ingest_stlc(store, "false");
  let h_app = Store.ingest_stlc(store, Stlc_ast.App(Stlc_ast.Var(0), Stlc_ast.Var(0))) |> ignore;
  let _ = h_app;
  /* Evaluate `not true` by constructing the app in ast form. */
  let app_ast =
    switch (Store.reconstruct_stlc(store, h_not)) {
    | Some(not_ast) =>
      Stlc_ast.App(not_ast, Stlc_ast.True)
    | None => failwith("reconstruct not failed")
    };
  let h_app = Store.ingest_stlc(store, app_ast);
  switch (Stlc_eval.eval(~store, ~att, h_app)) {
  | Stlc_eval.Value(h') =>
    Alcotest.(check(string))("not true ⇒ false", h_false, h')
  | _ => Alcotest.fail("expected Value")
  };
  let _ = h_true;
};

let test_stlc_eval_and_true_false = () => {
  let (store, att) = fresh_world();
  /* \x:Bool. \y:Bool. if x then y else false */
  let and_str = "\\x:Bool. \\y:Bool. if x then y else false";
  let h_and = ingest_stlc(store, and_str);
  let h_false = ingest_stlc(store, "false");
  let and_ast =
    switch (Store.reconstruct_stlc(store, h_and)) {
    | Some(a) => a
    | None => failwith("reconstruct and failed")
    };
  let app_ast =
    Stlc_ast.App(
      Stlc_ast.App(and_ast, Stlc_ast.True),
      Stlc_ast.False,
    );
  let h_app = Store.ingest_stlc(store, app_ast);
  switch (Stlc_eval.eval(~store, ~att, h_app)) {
  | Stlc_eval.Value(h') =>
    Alcotest.(check(string))("and true false ⇒ false", h_false, h')
  | _ => Alcotest.fail("expected Value")
  };
};

/* ==================== Type-check aspect ==================== */

let test_typecheck_identity = () => {
  let (store, att) = fresh_world();
  let h = ingest_stlc(store, "\\x:Bool. x");
  switch (Stlc_typecheck.check(~store, ~att, h)) {
  | Ok((ty, was_cached)) =>
    Alcotest.(check(bool))("first call not cached", false, was_cached);
    Alcotest.check(
      ty_testable,
      "identity: Bool -> Bool",
      Ty.Arrow(Ty.Bool, Ty.Bool),
      ty,
    );
  | Error(e) =>
    Alcotest.fail("typecheck failed: " ++ Stlc_typecheck.error_to_string(e))
  };
  switch (Stlc_typecheck.check(~store, ~att, h)) {
  | Ok((_, was_cached)) =>
    Alcotest.(check(bool))("second call cached", true, was_cached)
  | Error(_) => Alcotest.fail("second typecheck failed")
  };
};

let test_typecheck_not = () => {
  let (store, att) = fresh_world();
  let h =
    ingest_stlc(store, "\\x:Bool. if x then false else true");
  switch (Stlc_typecheck.check(~store, ~att, h)) {
  | Ok((ty, _)) =>
    Alcotest.check(
      ty_testable,
      "not: Bool -> Bool",
      Ty.Arrow(Ty.Bool, Ty.Bool),
      ty,
    )
  | Error(_) => Alcotest.fail("typecheck failed")
  };
};

let test_typecheck_const = () => {
  let (store, att) = fresh_world();
  /* const = \x:Bool. \y:Bool. x : Bool -> Bool -> Bool */
  let h = ingest_stlc(store, "\\x:Bool. \\y:Bool. x");
  switch (Stlc_typecheck.check(~store, ~att, h)) {
  | Ok((ty, _)) =>
    Alcotest.check(
      ty_testable,
      "const: Bool -> Bool -> Bool",
      Ty.Arrow(Ty.Bool, Ty.Arrow(Ty.Bool, Ty.Bool)),
      ty,
    )
  | Error(_) => Alcotest.fail("typecheck failed")
  };
};

/* ==================== Stlc → Lc: erase + Church ==================== */

let test_erase_true = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest_stlc(store, Stlc_ast.True);
  Alcotest.check(
    lc_nf_testable,
    "true erases to Church true",
    church_nf_true,
    erase_and_normalize(store, att, h),
  );
};

let test_erase_false = () => {
  let (store, att) = fresh_world();
  let h = Store.ingest_stlc(store, Stlc_ast.False);
  Alcotest.check(
    lc_nf_testable,
    "false erases to Church false",
    church_nf_false,
    erase_and_normalize(store, att, h),
  );
};

let test_erase_not_true = () => {
  let (store, att) = fresh_world();
  /* (not true) — evaluate in stlc to False, erase directly. Check that
     the erased+normalized form is church_false, proving erase preserves
     meaning on a native-if computation. */
  let h_not =
    ingest_stlc(store, "\\x:Bool. if x then false else true");
  let not_ast =
    switch (Store.reconstruct_stlc(store, h_not)) {
    | Some(a) => a
    | None => failwith("reconstruct")
    };
  let h_app =
    Store.ingest_stlc(
      store,
      Stlc_ast.App(not_ast, Stlc_ast.True),
    );
  Alcotest.check(
    lc_nf_testable,
    "erase(not true) β-normalizes to Church false",
    church_nf_false,
    erase_and_normalize(store, att, h_app),
  );
};

let test_erase_cache_hit = () => {
  let (store, att) = fresh_world();
  let h = ingest_stlc(store, "\\x:Bool. x");
  switch (Stlc_to_lc_erase_church.translate(~store, ~att, h)) {
  | Ok((_, was_cached)) =>
    Alcotest.(check(bool))("first call not cached", false, was_cached)
  | Error(_) => Alcotest.fail("first translate failed")
  };
  switch (Stlc_to_lc_erase_church.translate(~store, ~att, h)) {
  | Ok((_, was_cached)) =>
    Alcotest.(check(bool))("second call cached", true, was_cached)
  | Error(_) => Alcotest.fail("second translate failed")
  };
};

/* ==================== Lc → Stlc: check at type ==================== */

let test_lc_to_stlc_identity_at_bool_arrow_bool = () => {
  let (store, att) = fresh_world();
  let h_id = ingest_lc(store, "\\x. x");
  switch (
    Lc_to_stlc_check.translate(~store, ~att, h_id, Ty.Arrow(Ty.Bool, Ty.Bool))
  ) {
  | Ok((Lc_to_stlc_check.Translated(target), _)) =>
    /* Confirm target's reconstructed AST is `\x:Bool. x`. */
    switch (Store.reconstruct_stlc(store, target)) {
    | Some(Stlc_ast.Lam(Ty.Bool, Stlc_ast.Var(0))) =>
      Alcotest.(check(bool))("got \\x:Bool. x", true, true)
    | Some(other) =>
      Alcotest.fail(
        "unexpected stlc shape: " ++ Stlc_ast.show(other),
      )
    | None => Alcotest.fail("reconstruct failed")
    }
  | Ok((Lc_to_stlc_check.Untypable(msg), _)) =>
    Alcotest.fail("expected success, got untypable: " ++ msg)
  | Error(_) => Alcotest.fail("translator error")
  };
};

let test_lc_to_stlc_identity_at_higher = () => {
  let (store, att) = fresh_world();
  let h_id = ingest_lc(store, "\\x. x");
  let higher = Ty.Arrow(Ty.Arrow(Ty.Bool, Ty.Bool), Ty.Arrow(Ty.Bool, Ty.Bool));
  switch (Lc_to_stlc_check.translate(~store, ~att, h_id, higher)) {
  | Ok((Lc_to_stlc_check.Translated(target), _)) =>
    switch (Store.reconstruct_stlc(store, target)) {
    | Some(
        Stlc_ast.Lam(Ty.Arrow(Ty.Bool, Ty.Bool), Stlc_ast.Var(0)),
      ) =>
      Alcotest.(check(bool))("got \\x:Bool->Bool. x", true, true)
    | Some(other) =>
      Alcotest.fail(
        "unexpected shape: " ++ Stlc_ast.show(other),
      )
    | None => Alcotest.fail("reconstruct failed")
    }
  | _ => Alcotest.fail("expected Translated")
  };
};

let test_lc_to_stlc_identity_at_bool_fails = () => {
  let (store, att) = fresh_world();
  let h_id = ingest_lc(store, "\\x. x");
  switch (Lc_to_stlc_check.translate(~store, ~att, h_id, Ty.Bool)) {
  | Ok((Lc_to_stlc_check.Untypable(_), _)) =>
    Alcotest.(check(bool))("translator refused", true, true)
  | Ok((Lc_to_stlc_check.Translated(_), _)) =>
    Alcotest.fail("\\x. x should not check at Bool")
  | Error(_) => Alcotest.fail("translator error")
  };
};

let test_lc_to_stlc_self_app_fails = () => {
  let (store, att) = fresh_world();
  let h = ingest_lc(store, "\\x. x x");
  switch (
    Lc_to_stlc_check.translate(~store, ~att, h, Ty.Arrow(Ty.Bool, Ty.Bool))
  ) {
  | Ok((Lc_to_stlc_check.Untypable(_), _)) =>
    Alcotest.(check(bool))("self-app refused", true, true)
  | Ok((Lc_to_stlc_check.Translated(_), _)) =>
    Alcotest.fail("\\x. x x is not simply typable")
  | Error(_) => Alcotest.fail("translator error")
  };
};

let test_lc_to_stlc_cache_hit = () => {
  let (store, att) = fresh_world();
  let h = ingest_lc(store, "\\x. x");
  switch (
    Lc_to_stlc_check.translate(~store, ~att, h, Ty.Arrow(Ty.Bool, Ty.Bool))
  ) {
  | Ok((Lc_to_stlc_check.Translated(_), was_cached)) =>
    Alcotest.(check(bool))("first call not cached", false, was_cached)
  | _ => Alcotest.fail("first translate failed")
  };
  switch (
    Lc_to_stlc_check.translate(~store, ~att, h, Ty.Arrow(Ty.Bool, Ty.Bool))
  ) {
  | Ok((Lc_to_stlc_check.Translated(_), was_cached)) =>
    Alcotest.(check(bool))("second call cached", true, was_cached)
  | _ => Alcotest.fail("second translate failed")
  };
};

let test_lc_to_stlc_cache_failure = () => {
  let (store, att) = fresh_world();
  let h = ingest_lc(store, "\\x. x x");
  let ty = Ty.Arrow(Ty.Bool, Ty.Bool);
  switch (Lc_to_stlc_check.translate(~store, ~att, h, ty)) {
  | Ok((Lc_to_stlc_check.Untypable(_), was_cached)) =>
    Alcotest.(check(bool))("first refusal not cached", false, was_cached)
  | _ => Alcotest.fail("first translate should refuse")
  };
  switch (Lc_to_stlc_check.translate(~store, ~att, h, ty)) {
  | Ok((Lc_to_stlc_check.Untypable(_), was_cached)) =>
    Alcotest.(check(bool))("second refusal cached", true, was_cached)
  | _ => Alcotest.fail("second translate should refuse")
  };
};

let test_lc_to_stlc_different_types_different_cache = () => {
  let (store, att) = fresh_world();
  let h = ingest_lc(store, "\\x. x");
  let t1 = Ty.Arrow(Ty.Bool, Ty.Bool);
  let t2 = Ty.Arrow(Ty.Arrow(Ty.Bool, Ty.Bool), Ty.Arrow(Ty.Bool, Ty.Bool));
  let (r1, r2) =
    switch (
      Lc_to_stlc_check.translate(~store, ~att, h, t1),
      Lc_to_stlc_check.translate(~store, ~att, h, t2),
    ) {
    | (
        Ok((Lc_to_stlc_check.Translated(a), _)),
        Ok((Lc_to_stlc_check.Translated(b), _)),
      ) => (a, b)
    | _ => failwith("translate failed")
    };
  Alcotest.(check(bool))(
    "different target types produce different targets",
    true,
    !Hash.equal(r1, r2),
  );
  /* Also verify that the entries-listing mechanism picks up both. */
  let entries = Lc_to_stlc_check.all_entries(att);
  Alcotest.(check(int))(
    "two distinct cache entries under lc-to-stlc:check:v1[ty=*]",
    2,
    List.length(entries),
  );
};

/* ==================== Round-trip: erase preserves semantics under Church decoding ==================== */

let test_roundtrip_not_true_via_church = () => {
  let (store, att) = fresh_world();
  let h_not =
    ingest_stlc(store, "\\x:Bool. if x then false else true");
  let not_ast =
    switch (Store.reconstruct_stlc(store, h_not)) {
    | Some(a) => a
    | None => failwith("reconstruct")
    };
  let h_notfalse =
    Store.ingest_stlc(
      store,
      Stlc_ast.App(not_ast, Stlc_ast.False),
    );
  /* Native stlc: not false ⇒ true ; erased+Church: expects church_true */
  Alcotest.check(
    lc_nf_testable,
    "erase(not false) deep-normalizes to Church true",
    church_nf_true,
    erase_and_normalize(store, att, h_notfalse),
  );
};

/* ==================== Property: stlc eval on boolean programs matches
   erased lc eval up to Church decoding ==================== */

let rec stlc_bool_gen = (depth): QCheck.Gen.t(Stlc_ast.t) => {
  open QCheck.Gen;
  let leaves = oneof([return(Stlc_ast.True), return(Stlc_ast.False)]);
  if (depth <= 0) {
    leaves;
  } else {
    let sub = stlc_bool_gen(depth - 1);
    oneof_weighted([
      (3, leaves),
      (2, map3((c, t, e) => Stlc_ast.If(c, t, e), sub, sub, sub)),
    ]);
  };
};

let stlc_bool_arb =
  QCheck.make(~print=Stlc_ast.show, stlc_bool_gen(3));

let prop_erase_preserves_boolean_value =
  QCheck.Test.make(
    ~count=50,
    ~name="erase+normalize(t) = church_of(stlc_eval(t)) for Bool-valued closed t",
    stlc_bool_arb,
    t => {
      let (store, att) = fresh_world();
      let h = Store.ingest_stlc(store, Stlc_canonicalize.canonicalize(t));
      let native =
        switch (Stlc_eval.eval(~store, ~att, h)) {
        | Stlc_eval.Value(h') => h'
        | _ => failwith("stlc eval did not produce a value")
        };
      let church_expected =
        switch (Store.lookup(store, native)) {
        | Some(Definition.Stlc(Stlc_node.True)) => church_nf_true
        | Some(Definition.Stlc(Stlc_node.False)) => church_nf_false
        | _ => failwith("expected True/False")
        };
      let erased = erase_and_normalize(store, att, h);
      Lc_ast.equal(church_expected, erased);
    },
  );

/* ==================== Runner ==================== */

let () =
  Alcotest.run(
    "p6-stlc",
    [
      (
        "disjoint-hashes",
        [
          Alcotest.test_case(
            "language tags keep lc/stlc disjoint",
            `Quick,
            test_language_tags_disjoint,
          ),
        ],
      ),
      (
        "cross-language-rejection",
        [
          Alcotest.test_case(
            "stlc parent rejects lc child",
            `Quick,
            test_reject_stlc_parent_with_lc_child,
          ),
          Alcotest.test_case(
            "lc parent rejects stlc child",
            `Quick,
            test_reject_lc_parent_with_stlc_child,
          ),
        ],
      ),
      (
        "roundtrip",
        [
          Alcotest.test_case("lc roundtrip", `Quick, test_lc_roundtrip),
          Alcotest.test_case("stlc roundtrip", `Quick, test_stlc_roundtrip),
        ],
      ),
      (
        "alpha-equivalence",
        [
          Alcotest.test_case("lc α-equivalence", `Quick, test_alpha_lc),
          Alcotest.test_case("stlc α-equivalence", `Quick, test_alpha_stlc),
          Alcotest.test_case(
            "stlc binder annotation in hash",
            `Quick,
            test_stlc_different_annotations_different_hashes,
          ),
        ],
      ),
      (
        "resolver-well-typedness",
        [
          Alcotest.test_case(
            "stlc resolver rejects non-Bool guard",
            `Quick,
            test_stlc_ingest_rejects_bad_if,
          ),
          Alcotest.test_case(
            "stlc resolver rejects branch type mismatch",
            `Quick,
            test_stlc_ingest_rejects_branch_mismatch,
          ),
        ],
      ),
      (
        "eval",
        [
          Alcotest.test_case("lc eval works", `Quick, test_lc_eval_simple),
          Alcotest.test_case(
            "stlc: not true ⇒ false",
            `Quick,
            test_stlc_eval_not_true,
          ),
          Alcotest.test_case(
            "stlc: and true false ⇒ false",
            `Quick,
            test_stlc_eval_and_true_false,
          ),
        ],
      ),
      (
        "typecheck-aspect",
        [
          Alcotest.test_case(
            "identity : Bool -> Bool, cached on second call",
            `Quick,
            test_typecheck_identity,
          ),
          Alcotest.test_case(
            "not : Bool -> Bool",
            `Quick,
            test_typecheck_not,
          ),
          Alcotest.test_case(
            "const : Bool -> Bool -> Bool",
            `Quick,
            test_typecheck_const,
          ),
        ],
      ),
      (
        "stlc-to-lc",
        [
          Alcotest.test_case(
            "true erases to Church true",
            `Quick,
            test_erase_true,
          ),
          Alcotest.test_case(
            "false erases to Church false",
            `Quick,
            test_erase_false,
          ),
          Alcotest.test_case(
            "erase(not true) β-normalizes to Church false",
            `Quick,
            test_erase_not_true,
          ),
          Alcotest.test_case(
            "second erase is a cache hit",
            `Quick,
            test_erase_cache_hit,
          ),
        ],
      ),
      (
        "lc-to-stlc",
        [
          Alcotest.test_case(
            "\\x. x at Bool->Bool succeeds",
            `Quick,
            test_lc_to_stlc_identity_at_bool_arrow_bool,
          ),
          Alcotest.test_case(
            "\\x. x at (Bool->Bool)->(Bool->Bool) succeeds",
            `Quick,
            test_lc_to_stlc_identity_at_higher,
          ),
          Alcotest.test_case(
            "\\x. x at Bool fails",
            `Quick,
            test_lc_to_stlc_identity_at_bool_fails,
          ),
          Alcotest.test_case(
            "\\x. x x occurs-check fails",
            `Quick,
            test_lc_to_stlc_self_app_fails,
          ),
          Alcotest.test_case(
            "second translate is a cache hit",
            `Quick,
            test_lc_to_stlc_cache_hit,
          ),
          Alcotest.test_case(
            "failure is cached by procedure identity",
            `Quick,
            test_lc_to_stlc_cache_failure,
          ),
          Alcotest.test_case(
            "different target types cache independently",
            `Quick,
            test_lc_to_stlc_different_types_different_cache,
          ),
        ],
      ),
      (
        "roundtrip-erase",
        [
          Alcotest.test_case(
            "erase(not false) = Church true",
            `Quick,
            test_roundtrip_not_true_via_church,
          ),
        ],
      ),
      (
        "property-erase",
        [
          QCheck_alcotest.to_alcotest(prop_erase_preserves_boolean_value),
        ],
      ),
    ],
  );
