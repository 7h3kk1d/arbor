/* Surface -> internal translation with name resolution. Lam/Let bind by name on
   the surface and by de Bruijn index internally. Free names resolve through the
   namespace to Ref(hash); the bound hash must denote the right sort (a term in
   expression position, a type in type position). Surface types resolve to
   *registered* type hashes (so compound types are shared / content-addressed). */

let (let*) = Result.bind;

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

/* `tvs` is the type-variable context (innermost first), pushed by `forall`/`/\`.
   A `Named` whose name is bound there resolves to a de Bruijn `TVar`; otherwise
   it is a namespace type. */
let rec resolve_ty =
        (~ns: Namespace.t, ~st: Store.t, ~tvs: list(string)=[], s: Surface_ty.t)
        : result(Hash.t, string) =>
  switch (s) {
  | Surface_ty.Int => Ok(Store.int_type(st))
  | Surface_ty.Bool => Ok(Store.bool_type(st))
  | Surface_ty.Arrow(a, b) =>
    let* ah = resolve_ty(~ns, ~st, ~tvs, a);
    let* bh = resolve_ty(~ns, ~st, ~tvs, b);
    Ok(Store.ingest_type(st, Tnode.Arrow(ah, bh)));
  | Surface_ty.Product(a, b) =>
    let* ah = resolve_ty(~ns, ~st, ~tvs, a);
    let* bh = resolve_ty(~ns, ~st, ~tvs, b);
    Ok(Store.ingest_type(st, Tnode.Product(ah, bh)));
  | Surface_ty.Forall(name, body) =>
    let* bh = resolve_ty(~ns, ~st, ~tvs=[name, ...tvs], body);
    Ok(Store.ingest_type(st, Tnode.Forall(bh)));
  | Surface_ty.Exists(name, body) =>
    let* bh = resolve_ty(~ns, ~st, ~tvs=[name, ...tvs], body);
    Ok(Store.ingest_type(st, Tnode.Exists(bh)));
  | Surface_ty.Named(name) =>
    switch (lookup_ctx(tvs, name)) {
    | Some(i) => Ok(Store.ingest_type(st, Tnode.TVar(i)))
    | None =>
      switch (Namespace.resolve(ns, name)) {
      | None => Error("unbound type name: " ++ name)
      | Some(h) =>
        switch (Store.find(st, h)) {
        | Some(Definition.Type(_)) => Ok(h)
        | Some(Definition.Term(_)) => Error("'" ++ name ++ "' is a term, not a type")
        | None => Error("dangling type binding: " ++ name)
        }
      }
    }
  };

let op_to_node = (op: Surface.prim_op): Node.prim_op =>
  switch (op) {
  | Surface.Add => Node.Add
  | Surface.Sub => Node.Sub
  | Surface.Mul => Node.Mul
  | Surface.Eq => Node.Eq
  };

let rec resolve =
        (
          ~ctx: list(string),
          ~tvs: list(string)=[],
          ~ns: Namespace.t,
          ~st: Store.t,
          e: Surface.t,
        )
        : result(Node.t, string) =>
  switch (e) {
  | Surface.Var(name) =>
    switch (lookup_ctx(ctx, name)) {
    | Some(i) => Ok(Node.Var(i))
    | None =>
      switch (Namespace.resolve(ns, name)) {
      | None => Error("unbound name: " ++ name)
      | Some(h) =>
        switch (Store.find(st, h)) {
        | Some(Definition.Term(_)) => Ok(Node.Ref(h))
        | Some(Definition.Type(_)) => Error("'" ++ name ++ "' is a type, not a term")
        | None => Error("dangling binding: " ++ name)
        }
      }
    }
  | Surface.Lit(n) => Ok(Node.Lit(n))
  | Surface.Bool(b) => Ok(Node.BoolLit(b))
  | Surface.Lam(x, sty, body) =>
    let* ann = resolve_ty(~ns, ~st, ~tvs, sty);
    let* b = resolve(~ctx=[x, ...ctx], ~tvs, ~ns, ~st, body);
    Ok(Node.Lam(ann, b));
  | Surface.App(f, a) =>
    let* f' = resolve(~ctx, ~tvs, ~ns, ~st, f);
    let* a' = resolve(~ctx, ~tvs, ~ns, ~st, a);
    Ok(Node.App(f', a'));
  | Surface.Let(x, rhs, body) =>
    let* r = resolve(~ctx, ~tvs, ~ns, ~st, rhs);
    let* b = resolve(~ctx=[x, ...ctx], ~tvs, ~ns, ~st, body);
    Ok(Node.Let(r, b));
  | Surface.If(c, t, e) =>
    let* c' = resolve(~ctx, ~tvs, ~ns, ~st, c);
    let* t' = resolve(~ctx, ~tvs, ~ns, ~st, t);
    let* e' = resolve(~ctx, ~tvs, ~ns, ~st, e);
    Ok(Node.If(c', t', e'));
  | Surface.Pair(a, b) =>
    let* a' = resolve(~ctx, ~tvs, ~ns, ~st, a);
    let* b' = resolve(~ctx, ~tvs, ~ns, ~st, b);
    Ok(Node.Pair(a', b'));
  | Surface.Fst(p) =>
    let* p' = resolve(~ctx, ~tvs, ~ns, ~st, p);
    Ok(Node.Fst(p'));
  | Surface.Snd(p) =>
    let* p' = resolve(~ctx, ~tvs, ~ns, ~st, p);
    Ok(Node.Snd(p'));
  | Surface.Prim(op, args) =>
    let* args' = resolve_args(~ctx, ~tvs, ~ns, ~st, args);
    Ok(Node.Prim(op_to_node(op), args'));
  | Surface.TyLam(name, body) =>
    let* b = resolve(~ctx, ~tvs=[name, ...tvs], ~ns, ~st, body);
    Ok(Node.TyLam(b));
  | Surface.TyApp(e, sty) =>
    let* e' = resolve(~ctx, ~tvs, ~ns, ~st, e);
    let* th = resolve_ty(~ns, ~st, ~tvs, sty);
    Ok(Node.TyApp(e', th));
  | Surface.Pack(wty, e, ety) =>
    let* w = resolve_ty(~ns, ~st, ~tvs, wty);
    let* e' = resolve(~ctx, ~tvs, ~ns, ~st, e);
    let* t = resolve_ty(~ns, ~st, ~tvs, ety);
    Ok(Node.Pack({witness: w, body: e', ty: t}));
  | Surface.Unpack(tv, x, e, body) =>
    let* e' = resolve(~ctx, ~tvs, ~ns, ~st, e);
    let* body' =
      resolve(~ctx=[x, ...ctx], ~tvs=[tv, ...tvs], ~ns, ~st, body);
    Ok(Node.Unpack(e', body'));
  }

and resolve_args =
    (~ctx, ~tvs, ~ns, ~st, args: list(Surface.t)): result(list(Node.t), string) =>
  switch (args) {
  | [] => Ok([])
  | [a, ...rest] =>
    let* a' = resolve(~ctx, ~tvs, ~ns, ~st, a);
    let* rest' = resolve_args(~ctx, ~tvs, ~ns, ~st, rest);
    Ok([a', ...rest']);
  };
