/* Resolver — eliminates surface-level name leaves by looking up the
   namespace and inlining the stored subtree. Two entry points, one per
   language; each shares the same "look up, reconstruct, inline" pattern
   but calls the language-appropriate Store.reconstruct_*.

   Language guard (new in p5): when a Name resolves to a hash, the
   Store's Definition language must match the expected language. A
   mismatch surfaces as Language_mismatch — the "no cross-language
   references" rule from docs/design/03-content-addressing.md made
   visible at the edit-time boundary, not just at Store registration.

   Bound-variable semantics (lc only) are carried from p4: innermost
   binder wins, unshadowed names fall through to the namespace. Stored
   lc definitions are closed, so inlining at any binder depth needs no
   index shifting. */

type error =
  | Unbound_name(string)
  | Missing_hash(string, Hash.t)
  | Language_mismatch(string /* name */, string /* expected */, string /* actual */);

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
    ++ " here";

let ( let* ) = Result.bind;

/* ==================== Arith resolver ==================== */

let rec resolve_arith =
        (~namespace: Namespace.t, ~store: Store.t, s: Arith_surface_ast.t)
        : result(Arith_ast.t, error) =>
  switch (s) {
  | Arith_surface_ast.True => Ok(Arith_ast.True)
  | Arith_surface_ast.False => Ok(Arith_ast.False)
  | Arith_surface_ast.Zero => Ok(Arith_ast.Zero)
  | Arith_surface_ast.Succ(a) =>
    let* a' = resolve_arith(~namespace, ~store, a);
    Ok(Arith_ast.Succ(a'));
  | Arith_surface_ast.Pred(a) =>
    let* a' = resolve_arith(~namespace, ~store, a);
    Ok(Arith_ast.Pred(a'));
  | Arith_surface_ast.IsZero(a) =>
    let* a' = resolve_arith(~namespace, ~store, a);
    Ok(Arith_ast.IsZero(a'));
  | Arith_surface_ast.If(c, t, e) =>
    let* c' = resolve_arith(~namespace, ~store, c);
    let* t' = resolve_arith(~namespace, ~store, t);
    let* e' = resolve_arith(~namespace, ~store, e);
    Ok(Arith_ast.If(c', t', e'));
  | Arith_surface_ast.Name(name) =>
    switch (Namespace.resolve(namespace, name)) {
    | None => Error(Unbound_name(name))
    | Some(h) =>
      switch (Store.language_of(store, h)) {
      | None => Error(Missing_hash(name, h))
      | Some(lang) when lang != "arith" =>
        Error(Language_mismatch(name, "arith", lang))
      | Some(_) =>
        switch (Store.reconstruct_arith(store, h)) {
        | None => Error(Missing_hash(name, h))
        | Some(ast) => Ok(ast)
        }
      }
    }
  };

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
