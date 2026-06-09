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

/* Resolve a record field name to a label hash. An already-bound label is reused
   (intentional sharing by name); an unbound name mints a fresh label and binds it
   *only* when `mint` is supplied (the record-type-declaration gesture, decision
   1). Elsewhere (term position, plain annotations) an unbound field name is an
   error — declare the record type first. */
let resolve_label =
    (~ns: Namespace.t, ~st: Store.t, ~mint: option(Mint.source), name: string)
    : result(Hash.t, string) =>
  switch (Namespace.resolve(ns, name)) {
  | Some(h) =>
    switch (Store.find(st, h)) {
    | Some(Definition.Label(_)) => Ok(h)
    | _ => Error("'" ++ name ++ "' is bound but is not a label")
    }
  | None =>
    switch (mint) {
    | Some(src) =>
      let lh = Store.ingest_label(st, Label.create(Mint.fresh(src)));
      (try(Namespace.rebind(ns, ~name, lh)) {
       | _ => ()
       });
      Ok(lh);
    | None => Error("unbound record field label: " ++ name)
    }
  };

/* `tvs` is the type-variable context (innermost first), pushed by `forall`/`/\`.
   A `Named` whose name is bound there resolves to a de Bruijn `TVar`; otherwise
   it is a namespace type. `mint`, when supplied, lets record field labels be
   minted (the record-type-declaration gesture); it is None everywhere else. */
let rec resolve_ty =
        (
          ~ns: Namespace.t,
          ~st: Store.t,
          ~tvs: list(string)=[],
          ~mint: option(Mint.source)=None,
          s: Surface_ty.t,
        )
        : result(Hash.t, string) =>
  switch (s) {
  | Surface_ty.Int => Ok(Store.int_type(st))
  | Surface_ty.Bool => Ok(Store.bool_type(st))
  | Surface_ty.Arrow(a, b) =>
    let* ah = resolve_ty(~ns, ~st, ~tvs, ~mint, a);
    let* bh = resolve_ty(~ns, ~st, ~tvs, ~mint, b);
    Ok(Store.ingest_type(st, Tnode.Arrow(ah, bh)));
  | Surface_ty.Product(a, b) =>
    let* ah = resolve_ty(~ns, ~st, ~tvs, ~mint, a);
    let* bh = resolve_ty(~ns, ~st, ~tvs, ~mint, b);
    Ok(Store.ingest_type(st, Tnode.Product(ah, bh)));
  | Surface_ty.Forall(name, body) =>
    let* bh = resolve_ty(~ns, ~st, ~tvs=[name, ...tvs], ~mint, body);
    Ok(Store.ingest_type(st, Tnode.Forall(bh)));
  | Surface_ty.Exists(name, body) =>
    let* bh = resolve_ty(~ns, ~st, ~tvs=[name, ...tvs], ~mint, body);
    Ok(Store.ingest_type(st, Tnode.Exists(bh)));
  | Surface_ty.List(elem) =>
    let* eh = resolve_ty(~ns, ~st, ~tvs, ~mint, elem);
    Ok(Store.ingest_type(st, Tnode.List(eh)));
  | Surface_ty.Record(fields) =>
    let rec go = (acc, fs) =>
      switch (fs) {
      | [] => Ok(List.rev(acc))
      | [(fname, fsty), ...rest] =>
        let* lh = resolve_label(~ns, ~st, ~mint, fname);
        let* fh = resolve_ty(~ns, ~st, ~tvs, ~mint, fsty);
        go([(lh, fh), ...acc], rest);
      };
    let* fields' = go([], fields);
    Ok(Store.ingest_type(st, Tnode.Record(fields')));
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
        | Some(Definition.Label(_)) => Error("'" ++ name ++ "' is a label, not a type")
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
        | Some(Definition.Label(_)) => Error("'" ++ name ++ "' is a label, not a term")
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
  | Surface.Nil(elem) =>
    let* eh = resolve_ty(~ns, ~st, ~tvs, elem);
    Ok(Node.Nil(eh));
  | Surface.Cons(h, t) =>
    let* h' = resolve(~ctx, ~tvs, ~ns, ~st, h);
    let* t' = resolve(~ctx, ~tvs, ~ns, ~st, t);
    Ok(Node.Cons(h', t'));
  | Surface.Fold(l, z, f) =>
    let* l' = resolve(~ctx, ~tvs, ~ns, ~st, l);
    let* z' = resolve(~ctx, ~tvs, ~ns, ~st, z);
    let* f' = resolve(~ctx, ~tvs, ~ns, ~st, f);
    Ok(Node.Fold(l', z', f'));
  /* `[| e1, ..., en |]` desugars to a cons-spine. The trailing `Nil` needs a
     concrete element type for content-addressing, and the substrate has no
     inference, so we synthesize the head element's type here and pin it. This
     succeeds when the head's type is knowable without a typing context (literals,
     refs, primitives over them); a head that is a locally-bound variable can't be
     synthesized with an empty context — annotate or use cons/nil. */
  | Surface.ListLit(es) =>
    let* nodes = resolve_args(~ctx, ~tvs, ~ns, ~st, es);
    switch (nodes) {
    | [] => Error("empty list literal: use nil [T]")
    | [head, ..._] =>
      let env = Store.build_env(st);
      switch (Typecheck.synth_top(env, [], head)) {
      | Error(_) =>
        Error(
          "cannot infer list element type from the first element; annotate it or use cons/nil",
        )
      | Ok(elem_h) =>
        let rec build = (xs: list(Node.t)): Node.t =>
          switch (xs) {
          | [] => Node.Nil(elem_h)
          | [h, ...t] => Node.Cons(h, build(t))
          };
        Ok(build(nodes));
      };
    }
  | Surface.Record_lit(fields) =>
    /* field names must already resolve to labels (declare the record type
       first — the type declaration is the mint site, decision 1) */
    let rec go = (acc, fs) =>
      switch (fs) {
      | [] => Ok(List.rev(acc))
      | [(fname, sval), ...rest] =>
        let* lh = resolve_label(~ns, ~st, ~mint=None, fname);
        let* v = resolve(~ctx, ~tvs, ~ns, ~st, sval);
        go([(lh, v), ...acc], rest);
      };
    let* fields' = go([], fields);
    Ok(Node.Record_lit(fields'));
  | Surface.Project(r, fname) =>
    let* r' = resolve(~ctx, ~tvs, ~ns, ~st, r);
    let* lh = resolve_label(~ns, ~st, ~mint=None, fname);
    Ok(Node.Project_field(r', lh));
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
