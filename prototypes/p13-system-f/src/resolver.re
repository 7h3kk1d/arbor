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

let rec resolve_ty =
        (~ns: Namespace.t, ~st: Store.t, s: Surface_ty.t): result(Hash.t, string) =>
  switch (s) {
  | Surface_ty.Int => Ok(Store.int_type(st))
  | Surface_ty.Bool => Ok(Store.bool_type(st))
  | Surface_ty.Arrow(a, b) =>
    let* ah = resolve_ty(~ns, ~st, a);
    let* bh = resolve_ty(~ns, ~st, b);
    Ok(Store.ingest_type(st, Tnode.Arrow(ah, bh)));
  | Surface_ty.Product(a, b) =>
    let* ah = resolve_ty(~ns, ~st, a);
    let* bh = resolve_ty(~ns, ~st, b);
    Ok(Store.ingest_type(st, Tnode.Product(ah, bh)));
  | Surface_ty.Named(name) =>
    switch (Namespace.resolve(ns, name)) {
    | None => Error("unbound type name: " ++ name)
    | Some(h) =>
      switch (Store.find(st, h)) {
      | Some(Definition.Type(_)) => Ok(h)
      | Some(Definition.Term(_)) => Error("'" ++ name ++ "' is a term, not a type")
      | None => Error("dangling type binding: " ++ name)
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
        (~ctx: list(string), ~ns: Namespace.t, ~st: Store.t, e: Surface.t)
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
    let* ann = resolve_ty(~ns, ~st, sty);
    let* b = resolve(~ctx=[x, ...ctx], ~ns, ~st, body);
    Ok(Node.Lam(ann, b));
  | Surface.App(f, a) =>
    let* f' = resolve(~ctx, ~ns, ~st, f);
    let* a' = resolve(~ctx, ~ns, ~st, a);
    Ok(Node.App(f', a'));
  | Surface.Let(x, rhs, body) =>
    let* r = resolve(~ctx, ~ns, ~st, rhs);
    let* b = resolve(~ctx=[x, ...ctx], ~ns, ~st, body);
    Ok(Node.Let(r, b));
  | Surface.If(c, t, e) =>
    let* c' = resolve(~ctx, ~ns, ~st, c);
    let* t' = resolve(~ctx, ~ns, ~st, t);
    let* e' = resolve(~ctx, ~ns, ~st, e);
    Ok(Node.If(c', t', e'));
  | Surface.Pair(a, b) =>
    let* a' = resolve(~ctx, ~ns, ~st, a);
    let* b' = resolve(~ctx, ~ns, ~st, b);
    Ok(Node.Pair(a', b'));
  | Surface.Fst(p) =>
    let* p' = resolve(~ctx, ~ns, ~st, p);
    Ok(Node.Fst(p'));
  | Surface.Snd(p) =>
    let* p' = resolve(~ctx, ~ns, ~st, p);
    Ok(Node.Snd(p'));
  | Surface.Prim(op, args) =>
    let* args' = resolve_args(~ctx, ~ns, ~st, args);
    Ok(Node.Prim(op_to_node(op), args'));
  }

and resolve_args =
    (~ctx, ~ns, ~st, args: list(Surface.t)): result(list(Node.t), string) =>
  switch (args) {
  | [] => Ok([])
  | [a, ...rest] =>
    let* a' = resolve(~ctx, ~ns, ~st, a);
    let* rest' = resolve_args(~ctx, ~ns, ~st, rest);
    Ok([a', ...rest']);
  };
