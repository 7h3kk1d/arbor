/* Opacity-parameterized bidirectional-ish checker. Types are content-addressed,
   so a "type" is a Hash.t and equality is hash equality *modulo* unfolding
   opaques that are in the current open set.

   The store is abstracted behind `env` (closures) so this module does not depend
   on Store. `open_set` is threaded explicitly (not baked into env) because a
   Seal opens its own declared types regardless of the ambient set. */

type env = {
  resolve: Hash.t => Tnode.t, /* type hash -> its Tnode (must exist) */
  mk_type: Tnode.t => Hash.t, /* register a (possibly new) type, return its hash */
  type_of_ref: Hash.t => Hash.t, /* a referenced definition's type hash (must exist) */
  int_h: Hash.t,
  bool_h: Hash.t,
};

exception Type_error(string);

/* Unfold top-level opaques that are currently open, to their witnesses. */
let rec whnf = (env: env, opens: list(Hash.t), h: Hash.t): Hash.t =>
  switch (env.resolve(h)) {
  | Tnode.Opaque({witness, _}) when List.mem(h, opens) =>
    whnf(env, opens, witness)
  | _ => h
  };

/* Type equality under the open set: structural, unfolding opens as encountered.
   With opens = [] this collapses to hash equality. */
let rec equal_ty = (env: env, opens: list(Hash.t), h1: Hash.t, h2: Hash.t): bool => {
  let a = whnf(env, opens, h1);
  let b = whnf(env, opens, h2);
  if (Hash.equal(a, b)) {
    true;
  } else {
    switch (env.resolve(a), env.resolve(b)) {
    | (Tnode.Arrow(d1, c1), Tnode.Arrow(d2, c2)) =>
      equal_ty(env, opens, d1, d2) && equal_ty(env, opens, c1, c2)
    | (Tnode.Product(x1, y1), Tnode.Product(x2, y2)) =>
      equal_ty(env, opens, x1, x2) && equal_ty(env, opens, y1, y2)
    | (_, _) => false
    };
  };
};

/* Rewrite a type, replacing every open opaque by its witness (deep). Used to
   turn a representation-only impl's annotations into witness-typed annotations
   so the impl can be stored as an ordinary (shareable) term. */
let rec normalize_type = (env: env, opens: list(Hash.t), h: Hash.t): Hash.t =>
  switch (env.resolve(h)) {
  | Tnode.Opaque({witness, _}) when List.mem(h, opens) =>
    normalize_type(env, opens, witness)
  | Tnode.Int
  | Tnode.Bool
  | Tnode.Opaque(_) => h
  | Tnode.Arrow(d, c) =>
    env.mk_type(
      Tnode.Arrow(normalize_type(env, opens, d), normalize_type(env, opens, c)),
    )
  | Tnode.Product(x, y) =>
    env.mk_type(
      Tnode.Product(normalize_type(env, opens, x), normalize_type(env, opens, y)),
    )
  };

let rec normalize_term = (env: env, opens: list(Hash.t), node: Node.t): Node.t =>
  switch (node) {
  | Node.Lam(ann, body) =>
    Node.Lam(normalize_type(env, opens, ann), normalize_term(env, opens, body))
  | Node.Var(_)
  | Node.Lit(_)
  | Node.BoolLit(_)
  | Node.Ref(_) => node
  | Node.App(f, x) =>
    Node.App(normalize_term(env, opens, f), normalize_term(env, opens, x))
  | Node.Let(r, bd) =>
    Node.Let(normalize_term(env, opens, r), normalize_term(env, opens, bd))
  | Node.Pair(x, y) =>
    Node.Pair(normalize_term(env, opens, x), normalize_term(env, opens, y))
  | Node.Fst(p) => Node.Fst(normalize_term(env, opens, p))
  | Node.Snd(p) => Node.Snd(normalize_term(env, opens, p))
  | Node.If(c, t, e) =>
    Node.If(
      normalize_term(env, opens, c),
      normalize_term(env, opens, t),
      normalize_term(env, opens, e),
    )
  | Node.Prim(op, args) => Node.Prim(op, List.map(normalize_term(env, opens), args))
  | Node.Seal(_) => node /* do not descend into nested seals in v1 */
  };

let rec synth =
        (env: env, opens: list(Hash.t), ctx: list(Hash.t), node: Node.t): Hash.t =>
  switch (node) {
  | Node.Var(i) =>
    switch (List.nth_opt(ctx, i)) {
    | Some(h) => h
    | None => raise(Type_error("unbound index " ++ string_of_int(i)))
    }
  | Node.Lit(_) => env.int_h
  | Node.BoolLit(_) => env.bool_h
  | Node.Lam(ann, body) =>
    let cod = synth(env, opens, [ann, ...ctx], body);
    env.mk_type(Tnode.Arrow(ann, cod));
  | Node.App(f, x) =>
    let fh = synth(env, opens, ctx, f);
    switch (env.resolve(whnf(env, opens, fh))) {
    | Tnode.Arrow(dom, cod) =>
      let xh = synth(env, opens, ctx, x);
      if (equal_ty(env, opens, xh, dom)) {
        cod;
      } else {
        raise(Type_error("application: argument type mismatch"));
      };
    | _ => raise(Type_error("application: function is not an arrow"))
    }
  | Node.Let(rhs, body) =>
    let rh = synth(env, opens, ctx, rhs);
    synth(env, opens, [rh, ...ctx], body);
  | Node.Pair(a, b) =>
    let ah = synth(env, opens, ctx, a);
    let bh = synth(env, opens, ctx, b);
    env.mk_type(Tnode.Product(ah, bh));
  | Node.Fst(p) =>
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, p)))) {
    | Tnode.Product(a, _) => a
    | _ => raise(Type_error("fst: not a product"))
    }
  | Node.Snd(p) =>
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, p)))) {
    | Tnode.Product(_, b) => b
    | _ => raise(Type_error("snd: not a product"))
    }
  | Node.If(c, t, e) =>
    check(env, opens, ctx, c, env.bool_h);
    let th = synth(env, opens, ctx, t);
    let eh = synth(env, opens, ctx, e);
    if (equal_ty(env, opens, th, eh)) {
      th;
    } else {
      raise(Type_error("if: branch types differ"));
    };
  | Node.Prim(op, args) => synth_prim(env, opens, ctx, op, args)
  | Node.Ref(h) => env.type_of_ref(h)
  | Node.Seal({opens: seal_opens, ty, impl}) =>
    let impl_ty = env.type_of_ref(impl);
    let inner = seal_opens @ opens;
    if (equal_ty(env, inner, impl_ty, ty)) {
      ty;
    } else {
      raise(Type_error("seal: impl type does not match external type"));
    };
  }

and synth_prim =
    (env: env, opens, ctx, op: Node.prim_op, args: list(Node.t)): Hash.t =>
  switch (op, args) {
  | (Node.Add | Node.Sub | Node.Mul, [a, b]) =>
    check(env, opens, ctx, a, env.int_h);
    check(env, opens, ctx, b, env.int_h);
    env.int_h;
  | (Node.Eq, [a, b]) =>
    let ah = synth(env, opens, ctx, a);
    check(env, opens, ctx, b, ah);
    env.bool_h;
  | (_, _) => raise(Type_error("primitive: arity mismatch"))
  }

and check =
    (env: env, opens, ctx, node: Node.t, expected: Hash.t): unit => {
  let got = synth(env, opens, ctx, node);
  if (!equal_ty(env, opens, got, expected)) {
    raise(Type_error("type mismatch"));
  };
}

let synth_top = (env: env, opens: list(Hash.t), node: Node.t): result(Hash.t, string) =>
  try(Ok(synth(env, opens, [], node))) {
  | Type_error(m) => Error(m)
  };
