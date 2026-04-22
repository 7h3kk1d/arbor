/* Resolver — eliminates Surface_ast.Name leaves into a name-free Ast.t.

   Edit-time name resolution per docs/design/04-naming-layer.md: names
   are replaced with the reconstructed subtree of the hash they are
   bound to. By the time Store.ingest runs, the AST has no Name leaves.
   Ingest then hash-cons the result; structural sharing means that
   resolving `succ one` and `succ (succ 0)` produce the same top hash
   when `one := succ 0`. */

type error =
  | Unbound_name(string)
  | Missing_hash(string, Hash.t);

let error_to_string =
  fun
  | Unbound_name(n) => "unbound name: " ++ n
  | Missing_hash(n, h) =>
    "namespace binding points at missing hash: " ++ n ++ " -> " ++ Hash.short(h);

let ( let* ) = Result.bind;

let rec resolve =
        (~namespace: Namespace.t, ~store: Store.t, s: Surface_ast.t)
        : result(Ast.t, error) =>
  switch (s) {
  | Surface_ast.True => Ok(Ast.True)
  | Surface_ast.False => Ok(Ast.False)
  | Surface_ast.Zero => Ok(Ast.Zero)
  | Surface_ast.Succ(a) =>
    let* a' = resolve(~namespace, ~store, a);
    Ok(Ast.Succ(a'));
  | Surface_ast.Pred(a) =>
    let* a' = resolve(~namespace, ~store, a);
    Ok(Ast.Pred(a'));
  | Surface_ast.IsZero(a) =>
    let* a' = resolve(~namespace, ~store, a);
    Ok(Ast.IsZero(a'));
  | Surface_ast.If(c, t, e) =>
    let* c' = resolve(~namespace, ~store, c);
    let* t' = resolve(~namespace, ~store, t);
    let* e' = resolve(~namespace, ~store, e);
    Ok(Ast.If(c', t', e'));
  | Surface_ast.Name(name) =>
    switch (Namespace.resolve(namespace, name)) {
    | None => Error(Unbound_name(name))
    | Some(h) =>
      switch (Store.reconstruct(store, h)) {
      | None => Error(Missing_hash(name, h))
      | Some(ast) => Ok(ast)
      }
    }
  };
