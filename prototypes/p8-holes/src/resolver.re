/* Resolver — walks Surface_ast.t into a de Bruijn Ast.t.

   Threads a context (list of binder names, most-recently-bound first)
   while recursing. A Var(name) lookup checks the context first: the
   innermost binder wins. If the name isn't bound in the context, we
   fall through to the definition-level Namespace; if the Namespace has
   a hash for it, we reconstruct the stored subtree and inline it.
   Stored definitions are closed LC terms, so inlining needs no index
   shifting — a closed Ast.t has the same meaning at any binder depth.

   If neither the context nor the namespace knows the name, that's an
   Unbound_name error. A namespace binding that points at a hash not in
   the Store is a Missing_hash error (normally unreachable, but
   expressed honestly). */

type error =
  | Unbound_name(string)
  | Missing_hash(string, Hash.t);

let error_to_string =
  fun
  | Unbound_name(n) => "unbound name: " ++ n
  | Missing_hash(n, h) =>
    "namespace binding points at missing hash: " ++ n ++ " -> " ++ Hash.short(h);

/* Find the index of `name` in `ctx`, 0 = innermost. */
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

let ( let* ) = Result.bind;

let rec resolve_ctx =
        (
          ~context: list(string),
          ~namespace: Namespace.t,
          ~store: Store.t,
          s: Surface_ast.t,
        )
        : result(Ast.t, error) =>
  switch (s) {
  | Surface_ast.Var(name) =>
    switch (lookup_ctx(context, name)) {
    | Some(i) => Ok(Ast.Var(i))
    | None =>
      switch (Namespace.resolve(namespace, name)) {
      | None => Error(Unbound_name(name))
      | Some(h) =>
        switch (Store.reconstruct(store, h)) {
        | None => Error(Missing_hash(name, h))
        | Some(ast) => Ok(ast)
        }
      }
    }
  | Surface_ast.Lam(x, body) =>
    let* body' =
      resolve_ctx(~context=[x, ...context], ~namespace, ~store, body);
    Ok(Ast.Lam(body'));
  | Surface_ast.App(f, a) =>
    let* f' = resolve_ctx(~context, ~namespace, ~store, f);
    let* a' = resolve_ctx(~context, ~namespace, ~store, a);
    Ok(Ast.App(f', a'));
  | Surface_ast.Hole => Ok(Ast.Hole)
  };

let resolve = (~namespace, ~store, s): result(Ast.t, error) =>
  resolve_ctx(~context=[], ~namespace, ~store, s);
