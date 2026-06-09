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
    | (Tnode.Sig(c1), Tnode.Sig(c2)) =>
      /* canonical order: component kind carries no rank information, so a
         plain label-hash sort suffices for pairwise comparison */
      let sort = List.sort(((l1, _), (l2, _)) => String.compare(l1, l2));
      let (s1, s2) = (sort(c1), sort(c2));
      List.length(s1) == List.length(s2)
      && List.for_all2(
           ((la, ca), (lb, cb)) =>
             Hash.equal(la, lb)
             && (
               switch (ca, cb) {
               | (Tnode.Sopaque, Tnode.Sopaque) => true
               | (Tnode.Smanifest(x), Tnode.Smanifest(y))
               | (Tnode.Sval(x), Tnode.Sval(y)) => equal_ty(env, opens, x, y)
               | (_, _) => false
               }
             ),
           s1,
           s2,
         );
    | (_, _) => false
    };
  };
};

/* Map a function over every Smanifest/Sval payload of a sig (Sopaque has no
   payload). All sig payloads live under the sig's k binders uniformly, so
   callers pass `f` already adjusted for the crossing. */
let map_sig = (f: Hash.t => Hash.t, comps: list((Hash.t, Tnode.sig_comp))) =>
  List.map(
    ((l, c)) =>
      switch (c) {
      | Tnode.Sopaque => (l, Tnode.Sopaque)
      | Tnode.Smanifest(h) => (l, Tnode.Smanifest(f(h)))
      | Tnode.Sval(h) => (l, Tnode.Sval(f(h)))
      },
    comps,
  );

let sig_k = (comps: list((Hash.t, Tnode.sig_comp))): int =>
  List.length(Tnode.opaque_ranks(comps));

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
  | Tnode.List(elem) =>
    env.mk_type(Tnode.List(normalize_type(env, opens, elem)))
  | Tnode.Record(fields) =>
    env.mk_type(
      Tnode.Record(
        List.map(((l, ft)) => (l, normalize_type(env, opens, ft)), fields),
      ),
    )
  | Tnode.Sig(comps) =>
    env.mk_type(Tnode.Sig(map_sig(normalize_type(env, opens), comps)))
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
  | Tnode.List(elem) =>
    env.mk_type(Tnode.List(shift_ty(env, d, cutoff, elem)))
  | Tnode.Record(fields) =>
    env.mk_type(
      Tnode.Record(List.map(((l, ft)) => (l, shift_ty(env, d, cutoff, ft)), fields)),
    )
  | Tnode.Sig(comps) =>
    env.mk_type(
      Tnode.Sig(map_sig(shift_ty(env, d, cutoff + sig_k(comps)), comps)),
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
  | Tnode.List(elem) =>
    env.mk_type(Tnode.List(subst_ty(env, j, repl, elem)))
  | Tnode.Record(fields) =>
    env.mk_type(
      Tnode.Record(List.map(((l, ft)) => (l, subst_ty(env, j, repl, ft)), fields)),
    )
  | Tnode.Sig(comps) =>
    let k = sig_k(comps);
    env.mk_type(
      Tnode.Sig(map_sig(subst_ty(env, j + k, shift_ty(env, k, 0, repl)), comps)),
    );
  };

/* Does the type variable TVar(j) occur free in the type? The avoidance check on
   `open_local`: the body's result type may not mention the hidden types. */
let rec occurs_tvar = (env: env, j: int, h: Hash.t): bool =>
  switch (env.resolve(h)) {
  | Tnode.Int
  | Tnode.Bool
  | Tnode.Opaque(_)
  | Tnode.Abstract(_) => false
  | Tnode.TVar(i) => i == j
  | Tnode.Arrow(a, b)
  | Tnode.Product(a, b) => occurs_tvar(env, j, a) || occurs_tvar(env, j, b)
  | Tnode.Forall(body) => occurs_tvar(env, j + 1, body)
  | Tnode.List(elem) => occurs_tvar(env, j, elem)
  | Tnode.Record(fields) =>
    List.exists(((_, ft)) => occurs_tvar(env, j, ft), fields)
  | Tnode.Sig(comps) =>
    let k = sig_k(comps);
    List.exists(
      ((_, c)) =>
        switch (c) {
        | Tnode.Sopaque => false
        | Tnode.Smanifest(h)
        | Tnode.Sval(h) => occurs_tvar(env, j + k, h)
        },
      comps,
    );
  };

/* `retarget` moves a sig payload between telescopes: a parallel substitution
   replacing the `drop` target binders with the witnesses `ws` (expressed under
   `lift` impl binders) and re-aiming enclosing variables. Used by ascription
   (target sig -> impl sig), open (target sig -> closed Abstracts, lift=0), and
   nothing else. */
let retarget =
    (env: env, ~ws: array(Hash.t), ~drop: int, ~lift: int, h: Hash.t): Hash.t => {
  let rec go = (c: int, h: Hash.t): Hash.t =>
    switch (env.resolve(h)) {
    | Tnode.Int
    | Tnode.Bool
    | Tnode.Opaque(_)
    | Tnode.Abstract(_) => h
    | Tnode.TVar(i) =>
      if (i < c) {
        h;
      } else if (i < c + drop) {
        shift_ty(env, c, 0, ws[i - c]);
      } else {
        env.mk_type(Tnode.TVar(i - drop + lift));
      }
    | Tnode.Arrow(a, b) => env.mk_type(Tnode.Arrow(go(c, a), go(c, b)))
    | Tnode.Product(a, b) => env.mk_type(Tnode.Product(go(c, a), go(c, b)))
    | Tnode.Forall(body) => env.mk_type(Tnode.Forall(go(c + 1, body)))
    | Tnode.List(elem) => env.mk_type(Tnode.List(go(c, elem)))
    | Tnode.Record(fields) =>
      env.mk_type(Tnode.Record(List.map(((l, ft)) => (l, go(c, ft)), fields)))
    | Tnode.Sig(comps) =>
      env.mk_type(Tnode.Sig(map_sig(go(c + sig_k(comps)), comps)))
    };
  go(0, h);
};

/* Rank lookup: the TVar index of an opaque label within its sig. */
let rank_of = (ranks: list(Hash.t), l: Hash.t): int => {
  let rec go = (i, ls) =>
    switch (ls) {
    | [] => raise(Type_error("internal: label is not an opaque component"))
    | [x, ...rest] => Hash.equal(x, l) ? i : go(i + 1, rest)
    };
  go(0, ranks);
};

/* The module type as seen inside an `Open_local` body: every opaque component
   made manifest at its rank TVar (the body's k new binders). Payloads carry
   over verbatim — sig payloads already place rank i at TVar(i) and enclosing
   vars at >= k. Shared by the checker and the resolver. */
let open_local_binder =
    (env: env, comps: list((Hash.t, Tnode.sig_comp))): Hash.t => {
  let ranks = Tnode.opaque_ranks(comps);
  let comps' =
    List.map(
      ((l, c)) =>
        switch (c) {
        | Tnode.Sopaque =>
          (l, Tnode.Smanifest(env.mk_type(Tnode.TVar(rank_of(ranks, l)))))
        | other => (l, other)
        },
      comps,
    );
  env.mk_type(Tnode.Sig(comps'));
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
  | Node.Struct(members) =>
    Node.Struct(
      List.map(
        ((l, m)) =>
          switch (m) {
          | Node.Mtype(h) => (l, Node.Mtype(normalize_type(env, opens, h)))
          | Node.Mval(e) => (l, Node.Mval(normalize_term(env, opens, e)))
          },
        members,
      ),
    )
  | Node.Ascribe({impl, sg}) =>
    Node.Ascribe({
      impl: normalize_term(env, opens, impl),
      sg: normalize_type(env, opens, sg),
    })
  | Node.Open({pkg, mints}) =>
    Node.Open({pkg: normalize_term(env, opens, pkg), mints})
  | Node.Open_local({k, scrut, body}) =>
    Node.Open_local({
      k,
      scrut: normalize_term(env, opens, scrut),
      body: normalize_term(env, opens, body),
    })
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
  | Node.Struct(members) =>
    /* the implementation: a fully-manifest (k = 0) sig */
    let comps =
      List.map(
        ((l, m)) =>
          switch (m) {
          | Node.Mtype(h) => (l, Tnode.Smanifest(h))
          | Node.Mval(e) => (l, Tnode.Sval(synth(env, opens, ctx, e)))
          },
        members,
      );
    let labels = List.map(fst, comps);
    if (List.length(List.sort_uniq(String.compare, labels))
        != List.length(labels)) {
      raise(Type_error("struct: duplicate member label"));
    };
    env.mk_type(Tnode.Sig(comps));
  | Node.Ascribe({impl, sg}) =>
    /* M :> S — the new pack. Witness each opaque component of S with the
       impl's type member; manifest components match exactly; value component
       types match exactly after the witnesses are substituted (retarget);
       impl members not named in S are ignored (width / private members). */
    switch (env.resolve(whnf(env, opens, sg))) {
    | Tnode.Sig(c_t) =>
      switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, impl)))) {
      | Tnode.Sig(c_i) =>
        let ranks_t = Tnode.opaque_ranks(c_t);
        let ranks_i = Tnode.opaque_ranks(c_i);
        let find = (comps, l) =>
          List.find_opt(((l2, _)) => Hash.equal(l, l2), comps);
        let ws =
          ranks_t
          |> List.map(l =>
               switch (find(c_i, l)) {
               | Some((_, Tnode.Smanifest(w))) => w
               | Some((_, Tnode.Sopaque)) =>
                 env.mk_type(Tnode.TVar(rank_of(ranks_i, l)))
               | _ =>
                 raise(
                   Type_error(
                     "ascription: no type member witnesses an opaque component",
                   ),
                 )
               }
             )
          |> Array.of_list;
        let rt =
          retarget(
            env,
            ~ws,
            ~drop=List.length(ranks_t),
            ~lift=List.length(ranks_i),
          );
        List.iter(
          ((l, comp)) =>
            switch (comp) {
            | Tnode.Sopaque => () /* witnessed above */
            | Tnode.Smanifest(e) =>
              switch (find(c_i, l)) {
              | Some((_, Tnode.Smanifest(e_i))) =>
                if (!equal_ty(env, opens, rt(e), e_i)) {
                  raise(Type_error("ascription: manifest equation mismatch"));
                }
              | _ =>
                raise(
                  Type_error(
                    "ascription: manifest component not matched by a type member",
                  ),
                )
              }
            | Tnode.Sval(t) =>
              switch (find(c_i, l)) {
              | Some((_, Tnode.Sval(t_i))) =>
                if (!equal_ty(env, opens, rt(t), t_i)) {
                  raise(Type_error("ascription: value member type mismatch"));
                }
              | _ => raise(Type_error("ascription: missing value member"))
              }
            },
          c_t,
        );
        sg;
      | _ => raise(Type_error("ascription: term is not a module"))
      }
    | _ => raise(Type_error("ascription: target type is not a sig"))
    }
  | Node.Open({pkg, mints}) =>
    /* generative n-ary top-level open: every opaque component becomes manifest
       at a fresh witness-less Abstract (mints in the node bytes) */
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, pkg)))) {
    | Tnode.Sig(comps) =>
      let ranks = Tnode.opaque_ranks(comps);
      let k = List.length(ranks);
      if (List.length(mints) != k) {
        raise(Type_error("open: mint count does not match opaque components"));
      };
      let ws =
        mints
        |> List.map(m => env.mk_type(Tnode.Abstract(m)))
        |> Array.of_list;
      let rt = retarget(env, ~ws, ~drop=k, ~lift=0);
      let comps' =
        List.map(
          ((l, c)) =>
            switch (c) {
            | Tnode.Sopaque => (l, Tnode.Smanifest(ws[rank_of(ranks, l)]))
            | Tnode.Smanifest(e) => (l, Tnode.Smanifest(rt(e)))
            | Tnode.Sval(t) => (l, Tnode.Sval(rt(t)))
            },
          comps,
        );
      env.mk_type(Tnode.Sig(comps'));
    | _ => raise(Type_error("open: not a module"))
    }
  | Node.Open_local({k, scrut, body}) =>
    /* scoped open (the p15 open-question answered): the k opaque components
       become k de Bruijn type vars over the body — no mint, so structural
       sharing survives. The module value is the body's Var(0). Avoidance:
       none of the k vars may appear in the body's result type. */
    switch (env.resolve(whnf(env, opens, synth(env, opens, ctx, scrut)))) {
    | Tnode.Sig(comps) =>
      let ranks = Tnode.opaque_ranks(comps);
      if (List.length(ranks) != k) {
        raise(Type_error("open: binder count does not match the module type"));
      };
      let t_m = open_local_binder(env, comps);
      let ctx' = [t_m, ...List.map(shift_ty(env, k, 0), ctx)];
      let r = synth(env, opens, ctx', body);
      let escapes = ref(false);
      for (j in 0 to k - 1) {
        if (occurs_tvar(env, j, r)) {
          escapes := true;
        };
      };
      if (escapes^) {
        raise(Type_error("open: a hidden type would escape its scope"));
      };
      shift_ty(env, - k, k, r);
    | _ => raise(Type_error("open: not a module"))
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
    | Tnode.Sig(comps) =>
      /* projecting a value member out of a module. With k > 0 (an unsealed
         abstract module) only members whose types avoid the hidden types
         project; with k = 0 (structs, opened modules) this is the identity. */
      let k = sig_k(comps);
      switch (List.find_opt(((l, _)) => Hash.equal(l, label), comps)) {
      | Some((_, Tnode.Sval(t))) =>
        let escapes = ref(false);
        for (j in 0 to k - 1) {
          if (occurs_tvar(env, j, t)) {
            escapes := true;
          };
        };
        if (escapes^) {
          raise(
            Type_error("projection: member type mentions a hidden type"),
          );
        };
        shift_ty(env, - k, k, t);
      | Some(_) => raise(Type_error("projection: not a value member"))
      | None => raise(Type_error("projection: module has no such member"))
      };
    | _ => raise(Type_error("projection: not a record or module"))
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
