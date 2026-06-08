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
    | (Tnode.Forall(b1), Tnode.Forall(b2)) => equal_ty(env, opens, b1, b2)
    | (Tnode.Exists(b1), Tnode.Exists(b2)) => equal_ty(env, opens, b1, b2)
    | (Tnode.List(e1), Tnode.List(e2)) => equal_ty(env, opens, e1, e2)
    | (Tnode.Record(f1), Tnode.Record(f2)) =>
      let sort = List.sort(((l1, _), (l2, _)) => String.compare(l1, l2));
      let (s1, s2) = (sort(f1), sort(f2));
      List.length(s1) == List.length(s2)
      && List.for_all2(
           ((la, ta), (lb, tb)) =>
             Hash.equal(la, lb) && equal_ty(env, opens, ta, tb),
           s1,
           s2,
         );
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
  | Tnode.Opaque(_)
  | Tnode.TVar(_)
  | Tnode.Abstract(_) => h
  | Tnode.Arrow(d, c) =>
    env.mk_type(
      Tnode.Arrow(normalize_type(env, opens, d), normalize_type(env, opens, c)),
    )
  | Tnode.Product(x, y) =>
    env.mk_type(
      Tnode.Product(normalize_type(env, opens, x), normalize_type(env, opens, y)),
    )
  | Tnode.Forall(body) =>
    env.mk_type(Tnode.Forall(normalize_type(env, opens, body)))
  | Tnode.Exists(body) =>
    env.mk_type(Tnode.Exists(normalize_type(env, opens, body)))
  | Tnode.List(elem) =>
    env.mk_type(Tnode.List(normalize_type(env, opens, elem)))
  | Tnode.Record(fields) =>
    env.mk_type(
      Tnode.Record(
        List.map(((l, ft)) => (l, normalize_type(env, opens, ft)), fields),
      ),
    )
  };

/* de Bruijn type substitution over content-addressed types. `shift_ty` raises
   free type vars >= cutoff by d; `subst_ty` replaces TVar(j) with `repl`
   (decrementing higher vars), used to instantiate a Forall on TyApp. Witnesses
   are assumed closed, so Opaque carries no free type vars. */
let rec shift_ty = (env: env, d: int, cutoff: int, h: Hash.t): Hash.t =>
  switch (env.resolve(h)) {
  | Tnode.Int
  | Tnode.Bool
  | Tnode.Opaque(_)
  | Tnode.Abstract(_) => h
  | Tnode.TVar(i) => i >= cutoff ? env.mk_type(Tnode.TVar(i + d)) : h
  | Tnode.Arrow(a, b) =>
    env.mk_type(Tnode.Arrow(shift_ty(env, d, cutoff, a), shift_ty(env, d, cutoff, b)))
  | Tnode.Product(a, b) =>
    env.mk_type(Tnode.Product(shift_ty(env, d, cutoff, a), shift_ty(env, d, cutoff, b)))
  | Tnode.Forall(body) =>
    env.mk_type(Tnode.Forall(shift_ty(env, d, cutoff + 1, body)))
  | Tnode.Exists(body) =>
    env.mk_type(Tnode.Exists(shift_ty(env, d, cutoff + 1, body)))
  | Tnode.List(elem) =>
    env.mk_type(Tnode.List(shift_ty(env, d, cutoff, elem)))
  | Tnode.Record(fields) =>
    env.mk_type(
      Tnode.Record(List.map(((l, ft)) => (l, shift_ty(env, d, cutoff, ft)), fields)),
    )
  };

let rec subst_ty = (env: env, j: int, repl: Hash.t, h: Hash.t): Hash.t =>
  switch (env.resolve(h)) {
  | Tnode.Int
  | Tnode.Bool
  | Tnode.Opaque(_)
  | Tnode.Abstract(_) => h
  | Tnode.TVar(i) =>
    if (i == j) {
      repl;
    } else if (i > j) {
      env.mk_type(Tnode.TVar(i - 1));
    } else {
      h;
    }
  | Tnode.Arrow(a, b) =>
    env.mk_type(Tnode.Arrow(subst_ty(env, j, repl, a), subst_ty(env, j, repl, b)))
  | Tnode.Product(a, b) =>
    env.mk_type(Tnode.Product(subst_ty(env, j, repl, a), subst_ty(env, j, repl, b)))
  | Tnode.Forall(body) =>
    env.mk_type(Tnode.Forall(subst_ty(env, j + 1, shift_ty(env, 1, 0, repl), body)))
  | Tnode.Exists(body) =>
    env.mk_type(Tnode.Exists(subst_ty(env, j + 1, shift_ty(env, 1, 0, repl), body)))
  | Tnode.List(elem) =>
    env.mk_type(Tnode.List(subst_ty(env, j, repl, elem)))
  | Tnode.Record(fields) =>
    env.mk_type(
      Tnode.Record(List.map(((l, ft)) => (l, subst_ty(env, j, repl, ft)), fields)),
    )
  };

/* Does the type variable TVar(j) occur free in the type? The avoidance check on
   `unpack`: the body's result type may not mention the unpacked witness. */
let rec occurs_tvar = (env: env, j: int, h: Hash.t): bool =>
  switch (env.resolve(h)) {
  | Tnode.Int
  | Tnode.Bool
  | Tnode.Opaque(_)
  | Tnode.Abstract(_) => false
  | Tnode.TVar(i) => i == j
  | Tnode.Arrow(a, b)
  | Tnode.Product(a, b) => occurs_tvar(env, j, a) || occurs_tvar(env, j, b)
  | Tnode.Forall(body)
  | Tnode.Exists(body) => occurs_tvar(env, j + 1, body)
  | Tnode.List(elem) => occurs_tvar(env, j, elem)
  | Tnode.Record(fields) =>
    List.exists(((_, ft)) => occurs_tvar(env, j, ft), fields)
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
  | Node.TyLam(body) => Node.TyLam(normalize_term(env, opens, body))
  | Node.TyApp(f, ty) =>
    Node.TyApp(normalize_term(env, opens, f), normalize_type(env, opens, ty))
  | Node.Pack({witness, body, ty}) =>
    Node.Pack({
      witness: normalize_type(env, opens, witness),
      body: normalize_term(env, opens, body),
      ty: normalize_type(env, opens, ty),
    })
  | Node.Unpack(scrut, body) =>
    Node.Unpack(normalize_term(env, opens, scrut), normalize_term(env, opens, body))
  | Node.Open({pkg, mint}) =>
    Node.Open({pkg: normalize_term(env, opens, pkg), mint})
  | Node.Nil(elem) => Node.Nil(normalize_type(env, opens, elem))
  | Node.Cons(h, t) =>
    Node.Cons(normalize_term(env, opens, h), normalize_term(env, opens, t))
  | Node.Fold(lst, z, f) =>
    Node.Fold(
      normalize_term(env, opens, lst),
      normalize_term(env, opens, z),
      normalize_term(env, opens, f),
    )
  | Node.Record_lit(fields) =>
    Node.Record_lit(List.map(((l, v)) => (l, normalize_term(env, opens, v)), fields))
  | Node.Project_field(r, l) => Node.Project_field(normalize_term(env, opens, r), l)
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
  | Node.TyLam(body) =>
    /* a fresh type var enters scope; shift the term context's free type vars */
    let ctx' = List.map(shift_ty(env, 1, 0), ctx);
    env.mk_type(Tnode.Forall(synth(env, opens, ctx', body)));
  | Node.TyApp(f, arg) =>
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, f)))) {
    | Tnode.Forall(body) => subst_ty(env, 0, arg, body)
    | _ => raise(Type_error("type application: not a forall"))
    }
  | Node.Pack({witness, body, ty}) =>
    switch (env.resolve(whnf(env, opens, ty))) {
    | Tnode.Exists(ebody) =>
      check(env, opens, ctx, body, subst_ty(env, 0, witness, ebody));
      ty;
    | _ => raise(Type_error("pack: target type is not an existential"))
    }
  | Node.Unpack(scrut, body) =>
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, scrut)))) {
    | Tnode.Exists(ebody) =>
      /* a fresh witness type var enters scope (TVar 0); shift the term context
         and bind x : ebody */
      let ctx' = [ebody, ...List.map(shift_ty(env, 1, 0), ctx)];
      let r = synth(env, opens, ctx', body);
      if (occurs_tvar(env, 0, r)) {
        raise(Type_error("unpack: the witness type would escape its scope"));
      } else {
        shift_ty(env, (-1), 1, r);
      };
    | _ => raise(Type_error("unpack: scrutinee is not an existential"))
    }
  | Node.Open({pkg, mint}) =>
    /* generative open: the package's body type over a fresh Abstract(mint) */
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, pkg)))) {
    | Tnode.Exists(ebody) =>
      subst_ty(env, 0, env.mk_type(Tnode.Abstract(mint)), ebody)
    | _ => raise(Type_error("open: not an existential"))
    }
  | Node.Nil(elem) => env.mk_type(Tnode.List(elem))
  | Node.Cons(h, t) =>
    let eh = synth(env, opens, ctx, h);
    check(env, opens, ctx, t, env.mk_type(Tnode.List(eh)));
    env.mk_type(Tnode.List(eh));
  | Node.Fold(lst, z, f) =>
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, lst)))) {
    | Tnode.List(elem) =>
      let acc = synth(env, opens, ctx, z);
      check(
        env,
        opens,
        ctx,
        f,
        env.mk_type(Tnode.Arrow(elem, env.mk_type(Tnode.Arrow(acc, acc)))),
      );
      acc;
    | _ => raise(Type_error("fold: not a list"))
    }
  | Node.Record_lit(fields) =>
    let ftys = List.map(((l, v)) => (l, synth(env, opens, ctx, v)), fields);
    env.mk_type(Tnode.Record(ftys));
  | Node.Project_field(r, label) =>
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, r)))) {
    | Tnode.Record(fields) =>
      switch (List.find_opt(((l, _)) => Hash.equal(l, label), fields)) {
      | Some((_, fty)) => fty
      | None => raise(Type_error("projection: record has no such field"))
      }
    | _ => raise(Type_error("projection: not a record"))
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
