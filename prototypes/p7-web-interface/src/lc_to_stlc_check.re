/* lc → stlc translator, partial: check an untyped lc term against a
   user-supplied target type.

   Because types in p6 are monomorphic (Bool | →, no type variables),
   there is no principal type to "infer" in the Hindley–Milner sense.
   Instead, the user supplies a target type and the translator checks
   the term at it using constraint-based unification over fresh
   metavariables. The translator succeeds iff every binder's type
   grounds to a concrete Ty.t built from Bool and Arrow — any remaining
   metavariable at the end signals ambiguity and the translator refuses.

   This is p6's substrate exercise of "partial translator" per
   docs/design/05-translation.md. Refusals are cached as
   Translation_untypable(string); successes as
   Translation_target(Hash.t). Both short-circuit on replay.

   Cache-key twist: since the translator takes an extra input (the
   expected type), that input is folded into the procedure identity —
   procedure_id_for(ty) = "lc-to-stlc:check:v1[ty=<hex8>]" where hex8
   is the first 8 hex chars of Ty.hash(canonicalize(ty)). Different
   expected types yield distinct procedure ids and distinct cache
   entries under the same aspect. Callers list-by-type via
   Attachment.entries_for for that specific procedure id.

   Aspect id: `translation-to-stlc`. Registered languages: ["lc"] — the
   output lives on the lc source, not the stlc target. */

let aspect_id: Attachment.aspect_id = "translation-to-stlc";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["lc"],
};

let procedure_id_for = (ty: Ty.t): Attachment.procedure_id => {
  let h = Ty.hash(Ty.canonicalize(ty));
  let hex8 = String.sub(h, 0, 8);
  "lc-to-stlc:check:v1[ty=" ++ hex8 ++ "]";
};

type error =
  | Not_lc(Hash.t)
  | Dangling(Hash.t);

let error_to_string =
  fun
  | Not_lc(h) =>
    "translation requires an lc definition, but "
    ++ Hash.short(h)
    ++ " is not lc"
  | Dangling(h) =>
    "translation source is not in the store: " ++ Hash.short(h);

/* ==================== Type-variables with union-find ==================== */

type tyv =
  | TvBool
  | TvArrow(tyv, tyv)
  | TvMeta(int, ref(option(tyv)));

let counter = ref(0);
let fresh_meta = (): tyv => {
  let id = counter^;
  counter := id + 1;
  TvMeta(id, ref(None));
};

let rec follow = (tv: tyv): tyv =>
  switch (tv) {
  | TvMeta(_, r) =>
    switch (r^) {
    | Some(t) => follow(t)
    | None => tv
    }
  | _ => tv
  };

let rec show_tyv = (tv: tyv): string =>
  switch (follow(tv)) {
  | TvBool => "Bool"
  | TvArrow(a, b) =>
    "("
    ++ show_tyv(a)
    ++ " -> "
    ++ show_tyv(b)
    ++ ")"
  | TvMeta(id, _) => "'" ++ string_of_int(id)
  };

let rec occurs = (id: int, tv: tyv): bool =>
  switch (follow(tv)) {
  | TvBool => false
  | TvArrow(a, b) => occurs(id, a) || occurs(id, b)
  | TvMeta(id', _) => id == id'
  };

let rec unify = (a: tyv, b: tyv): result(unit, string) => {
  let a = follow(a);
  let b = follow(b);
  switch (a, b) {
  | (TvBool, TvBool) => Ok()
  | (TvArrow(a1, b1), TvArrow(a2, b2)) =>
    switch (unify(a1, a2)) {
    | Error(_) as e => e
    | Ok () => unify(b1, b2)
    }
  | (TvMeta(id, _), TvMeta(id', _)) when id == id' => Ok()
  | (TvMeta(id, r), other)
  | (other, TvMeta(id, r)) =>
    if (occurs(id, other)) {
      Error("occurs check: '" ++ string_of_int(id) ++ " = " ++ show_tyv(other));
    } else {
      r := Some(other);
      Ok();
    }
  | (_, _) =>
    Error("cannot unify " ++ show_tyv(a) ++ " with " ++ show_tyv(b))
  };
};

let rec tv_of_ty = (t: Ty.t): tyv =>
  switch (t) {
  | Ty.Bool => TvBool
  | Ty.Arrow(a, b) => TvArrow(tv_of_ty(a), tv_of_ty(b))
  };

let rec ty_of_tv = (tv: tyv): result(Ty.t, string) =>
  switch (follow(tv)) {
  | TvBool => Ok(Ty.Bool)
  | TvArrow(a, b) =>
    switch (ty_of_tv(a)) {
    | Error(_) as e => e
    | Ok(a') =>
      switch (ty_of_tv(b)) {
      | Error(_) as e => e
      | Ok(b') => Ok(Ty.Arrow(a', b'))
      }
    }
  | TvMeta(id, _) =>
    Error(
      "ambiguous typing: unresolved type variable '" ++ string_of_int(id),
    )
  };

/* ==================== Constraint generation ====================

   `build` walks the untyped term, associating each Lam binder with a
   fresh metavariable for its parameter type and unifying against the
   expected type as constraints are discovered. The "pre-AST" returned
   carries those metavariables in Lam positions; zonking at the end
   concretizes them or signals ambiguity. */

type pre_ast =
  | PVar(int)
  | PLam(tyv /* binder type */, pre_ast)
  | PApp(pre_ast, pre_ast);

let ( let* ) = Result.bind;

let rec build =
        (~ctx: list(tyv), term: Lc_ast.t, expected: tyv)
        : result(pre_ast, string) =>
  switch (term) {
  | Lc_ast.Var(k) =>
    switch (List.nth_opt(ctx, k)) {
    | None =>
      Error(
        "de-Bruijn index " ++ string_of_int(k) ++ " out of context (open term)",
      )
    | Some(tv) =>
      let* () = unify(tv, expected);
      Ok(PVar(k));
    }
  | Lc_ast.Lam(body) =>
    let arg_tv = fresh_meta();
    let body_tv = fresh_meta();
    let* () = unify(expected, TvArrow(arg_tv, body_tv));
    let* body' = build(~ctx=[arg_tv, ...ctx], body, body_tv);
    Ok(PLam(arg_tv, body'));
  | Lc_ast.App(f, a) =>
    let arg_tv = fresh_meta();
    let* f' = build(~ctx, f, TvArrow(arg_tv, expected));
    let* a' = build(~ctx, a, arg_tv);
    Ok(PApp(f', a'));
  };

let rec zonk = (pre: pre_ast): result(Stlc_ast.t, string) =>
  switch (pre) {
  | PVar(k) => Ok(Stlc_ast.Var(k))
  | PLam(tv, body) =>
    let* ty = ty_of_tv(tv);
    let* body' = zonk(body);
    Ok(Stlc_ast.Lam(ty, body'));
  | PApp(f, a) =>
    let* f' = zonk(f);
    let* a' = zonk(a);
    Ok(Stlc_ast.App(f', a'));
  };

/* The translator proper. Counter-reset at entry keeps metavariable ids
   deterministic per call (useful for error messages); union-find state
   is entirely local to refs created in this call. */

let check_at_type =
    (term: Lc_ast.t, expected: Ty.t): result(Stlc_ast.t, string) => {
  counter := 0;
  let expected_tv = tv_of_ty(expected);
  let* pre = build(~ctx=[], term, expected_tv);
  zonk(pre);
};

/* ==================== Caching + aspect machinery ==================== */

let peek_cache =
    (att: Attachment.t, source: Hash.t, ty: Ty.t)
    : option(Attachment.aspect_value) =>
  Attachment.peek(
    att,
    ~target=source,
    ~aspect=aspect_id,
    ~procedure=procedure_id_for(ty),
  );

type outcome =
  | Translated(Hash.t)
  | Untypable(string);

let translate =
    (~store: Store.t, ~att: Attachment.t, source: Hash.t, expected: Ty.t)
    : result((outcome, bool /*was_cached*/), error) => {
  let procedure = procedure_id_for(expected);
  switch (
    Attachment.peek(
      att,
      ~target=source,
      ~aspect=aspect_id,
      ~procedure,
    )
  ) {
  | Some(Attachment.Translation_target(target)) =>
    Ok((Translated(target), true))
  | Some(Attachment.Translation_untypable(msg)) =>
    Ok((Untypable(msg), true))
  | Some(_)
  | None =>
    switch (Store.lookup(store, source)) {
    | None => Error(Dangling(source))
    | Some(Definition.Stlc(_)) => Error(Not_lc(source))
    | Some(Definition.Lc(_)) =>
      switch (Store.reconstruct_lc(store, source)) {
      | None => Error(Dangling(source))
      | Some(lc_ast) =>
        switch (check_at_type(lc_ast, expected)) {
        | Ok(stlc_ast) =>
          let target =
            Store.ingest_stlc(store, Stlc_canonicalize.canonicalize(stlc_ast));
          Attachment.attach(
            att,
            ~target=source,
            ~aspect=aspect_id,
            ~procedure,
            Attachment.Translation_target(target),
          );
          Ok((Translated(target), false));
        | Error(msg) =>
          Attachment.attach(
            att,
            ~target=source,
            ~aspect=aspect_id,
            ~procedure,
            Attachment.Translation_untypable(msg),
          );
          Ok((Untypable(msg), false));
        }
      }
    }
  };
};

/* Enumerate all cached (source, value) pairs across every procedure
   variant that starts with "lc-to-stlc:check:v1[ty=". The prefix match
   collects every expected-type the user has tried. Sorted by source
   hash then by procedure id for determinism. */
let all_entries =
    (att: Attachment.t)
    : list((Hash.t, string /*procedure*/, Attachment.aspect_value)) => {
  let prefix = "lc-to-stlc:check:v1[ty=";
  let plen = String.length(prefix);
  let entries = ref([]);
  /* Iterate over the raw Attachment store. We don't have a direct
     prefix query, but `entries_for` expects a specific procedure id.
     Instead, fold the store directly — this mirrors `entries_for`'s
     implementation but matches on prefix. */
  Hashtbl.iter(
    (k: Attachment.key, v: Attachment.aspect_value) =>
      if (String.equal(k.aspect, aspect_id)
          && String.length(k.procedure) >= plen
          && String.sub(k.procedure, 0, plen) == prefix) {
        entries := [(k.target, k.procedure, v), ...entries^];
      },
    att.Attachment.store,
  );
  List.sort(
    ((a, pa, _), (b, pb, _)) => {
      let c = String.compare(a, b);
      if (c == 0) {
        String.compare(pa, pb);
      } else {
        c;
      };
    },
    entries^,
  );
};
