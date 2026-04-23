/* Resolver — eliminates surface-level name leaves by looking up the
   namespace and inlining the stored subtree. Two entry points, one per
   language; each shares the same "look up, reconstruct, inline" pattern
   but calls the language-appropriate Store.reconstruct_*.

   Language guard: when a Name resolves to a hash, the Store's
   Definition language must match the expected language. A mismatch
   surfaces as Language_mismatch — the "no cross-language references"
   rule from docs/design/03-content-addressing.md made visible at the
   edit-time boundary, not just at Store registration.

   New in p6: resolve_stlc additionally type-checks the resolved
   Stlc_ast before returning Ok — Store invariant is "every stored stlc
   definition type-checks." Ill-typed input surfaces as Type_error.

   Bound-variable semantics (lc and stlc) are carried from p4: innermost
   binder wins, unshadowed names fall through to the namespace. Stored
   definitions are closed, so inlining at any binder depth needs no
   index shifting. */

type error =
  | Unbound_name(string)
  | Missing_hash(string, Hash.t)
  | Language_mismatch(string /* name */, string /* expected */, string /* actual */)
  | Type_error(string);

let error_to_string =
  fun
  | Unbound_name(n) => "unbound name: " ++ n
  | Missing_hash(n, h) =>
    "namespace binding points at missing hash: "
    ++ n
    ++ " -> "
    ++ Hash.short(h)
  | Language_mismatch(n, expected, actual) =>
    "name '"
    ++ n
    ++ "' is bound to a "
    ++ actual
    ++ " definition; expected "
    ++ expected
    ++ " here"
  | Type_error(msg) => "type error: " ++ msg;

let ( let* ) = Result.bind;

/* ==================== Lc resolver ==================== */

let rec lookup_ctx = (ctx: list(string), name: string): option(int) =>
  switch (ctx) {
  | [] => None
  | [h, ...rest] =>
    if (h == name) {
      Some(0);
    } else {
      Option.map(i => i + 1, lookup_ctx(rest, name));
    }
  };

let rec resolve_lc_ctx =
        (
          ~context: list(string),
          ~namespace: Namespace.t,
          ~store: Store.t,
          s: Lc_surface_ast.t,
        )
        : result(Lc_ast.t, error) =>
  switch (s) {
  | Lc_surface_ast.Var(name) =>
    switch (lookup_ctx(context, name)) {
    | Some(i) => Ok(Lc_ast.Var(i))
    | None =>
      switch (Namespace.resolve(namespace, name)) {
      | None => Error(Unbound_name(name))
      | Some(h) =>
        switch (Store.language_of(store, h)) {
        | None => Error(Missing_hash(name, h))
        | Some(lang) when lang != "lc" =>
          Error(Language_mismatch(name, "lc", lang))
        | Some(_) =>
          switch (Store.reconstruct_lc(store, h)) {
          | None => Error(Missing_hash(name, h))
          | Some(ast) => Ok(ast)
          }
        }
      }
    }
  | Lc_surface_ast.Lam(x, body) =>
    let* body' =
      resolve_lc_ctx(~context=[x, ...context], ~namespace, ~store, body);
    Ok(Lc_ast.Lam(body'));
  | Lc_surface_ast.App(f, a) =>
    let* f' = resolve_lc_ctx(~context, ~namespace, ~store, f);
    let* a' = resolve_lc_ctx(~context, ~namespace, ~store, a);
    Ok(Lc_ast.App(f', a'));
  };

let resolve_lc = (~namespace, ~store, s): result(Lc_ast.t, error) =>
  resolve_lc_ctx(~context=[], ~namespace, ~store, s);

/* ==================== Stlc resolver ====================

   Surface names disappear in two ways: bound variables become de Bruijn
   indices; unshadowed namespace names resolve to stored hashes and are
   inlined by reconstruction. Type annotations on Lam binders are copied
   through verbatim (they have no scoping concerns).

   After name resolution, Stlc_typecheck.infer runs on the closed form.
   A type error aborts ingest (Store invariant: stlc definitions in the
   Store are always well-typed). */

let rec resolve_stlc_ctx =
        (
          ~context: list(string),
          ~namespace: Namespace.t,
          ~store: Store.t,
          s: Stlc_surface_ast.t,
        )
        : result(Stlc_ast.t, error) =>
  switch (s) {
  | Stlc_surface_ast.Var(name) =>
    switch (lookup_ctx(context, name)) {
    | Some(i) => Ok(Stlc_ast.Var(i))
    | None =>
      switch (Namespace.resolve(namespace, name)) {
      | None => Error(Unbound_name(name))
      | Some(h) =>
        switch (Store.language_of(store, h)) {
        | None => Error(Missing_hash(name, h))
        | Some(lang) when lang != "stlc" =>
          Error(Language_mismatch(name, "stlc", lang))
        | Some(_) =>
          switch (Store.reconstruct_stlc(store, h)) {
          | None => Error(Missing_hash(name, h))
          | Some(ast) => Ok(ast)
          }
        }
      }
    }
  | Stlc_surface_ast.True => Ok(Stlc_ast.True)
  | Stlc_surface_ast.False => Ok(Stlc_ast.False)
  | Stlc_surface_ast.Lam(x, ty, body) =>
    let* body' =
      resolve_stlc_ctx(~context=[x, ...context], ~namespace, ~store, body);
    Ok(Stlc_ast.Lam(ty, body'));
  | Stlc_surface_ast.App(f, a) =>
    let* f' = resolve_stlc_ctx(~context, ~namespace, ~store, f);
    let* a' = resolve_stlc_ctx(~context, ~namespace, ~store, a);
    Ok(Stlc_ast.App(f', a'));
  | Stlc_surface_ast.If(c, t, e) =>
    let* c' = resolve_stlc_ctx(~context, ~namespace, ~store, c);
    let* t' = resolve_stlc_ctx(~context, ~namespace, ~store, t);
    let* e' = resolve_stlc_ctx(~context, ~namespace, ~store, e);
    Ok(Stlc_ast.If(c', t', e'));
  };

let resolve_stlc = (~namespace, ~store, s): result(Stlc_ast.t, error) => {
  let* ast = resolve_stlc_ctx(~context=[], ~namespace, ~store, s);
  switch (Stlc_typecheck.infer(~ctx=[], ast)) {
  | Ok(_) => Ok(ast)
  | Error(msg) => Error(Type_error(msg))
  };
};
