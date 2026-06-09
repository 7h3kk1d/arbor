/* Surface -> internal translation with name resolution. Lam/Let bind by name on
   the surface and by de Bruijn index internally. Free names resolve through the
   namespace to Ref(hash); the bound hash must denote the right sort (a term in
   expression position, a type in type position). Surface types resolve to
   *registered* type hashes (so compound types are shared / content-addressed).

   p17 adds three pieces of context beyond `ctx`/`tvs`:

   - `mtypes` — locally-manifest type names (a sig's `type u = T` components, a
     struct's type members, an opened module's manifest members as `M.u`). Each
     entry carries the tvs-depth at which its hash was resolved; uses at deeper
     tvs shift the hash by the difference.
   - `tctx` — the *types* of the term binders, mirroring `ctx`. Needed because
     `open e as M in body` must know `e`'s signature at resolve time (to bind
     `M.t`-style type names at the right TVar ranks). Entries are options: a
     binder whose type could not be synthesized resolves fine until something
     actually needs it.
   - `mint` — a mint source for label minting. Sig declarations and struct
     members are mint sites (authoring a signature or a module implementation
     declares its member names); record literals and projections still require
     already-bound labels. */

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

/* Resolve a record/sig/struct member name to a label hash. An already-bound
   label is reused (intentional sharing by name); an unbound name mints a fresh
   label and binds it *only* when `mint` is supplied (type-declaration and
   struct-member gestures). Elsewhere (record literals, projections) an unbound
   field name is an error — declare the type first. */
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
    | None => Error("unbound field label: " ++ name)
    }
  };

/* manifest-type entries: (surface name, resolved hash, tvs-depth at entry) */
type mtypes = list((string, Hash.t, int));

let find_mtype = (mtypes: mtypes, name: string): option((Hash.t, int)) =>
  List.find_map(
    ((n, h, d)) => n == name ? Some((h, d)) : None,
    mtypes,
  );

let dup_names = (names: list(string)): option(string) => {
  let rec go = seen =>
    fun
    | [] => None
    | [n, ...rest] => List.mem(n, seen) ? Some(n) : go([n, ...seen], rest);
  go([], names);
};

/* `tvs` is the type-variable context (innermost first), pushed by `forall`/`/\`
   and (k at a time, in rank order) by `open ... as M in`. A `Named` bound there
   resolves to a de Bruijn `TVar`; next it is tried as a locally-manifest type
   (`mtypes`); otherwise it is a namespace type. */
let rec resolve_ty =
        (
          ~ns: Namespace.t,
          ~st: Store.t,
          ~tvs: list(string)=[],
          ~mint: option(Mint.source)=None,
          ~mtypes: mtypes=[],
          s: Surface_ty.t,
        )
        : result(Hash.t, string) =>
  switch (s) {
  | Surface_ty.Int => Ok(Store.int_type(st))
  | Surface_ty.Bool => Ok(Store.bool_type(st))
  | Surface_ty.Arrow(a, b) =>
    let* ah = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, a);
    let* bh = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, b);
    Ok(Store.ingest_type(st, Tnode.Arrow(ah, bh)));
  | Surface_ty.Product(a, b) =>
    let* ah = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, a);
    let* bh = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, b);
    Ok(Store.ingest_type(st, Tnode.Product(ah, bh)));
  | Surface_ty.Forall(name, body) =>
    let* bh = resolve_ty(~ns, ~st, ~tvs=[name, ...tvs], ~mint, ~mtypes, body);
    Ok(Store.ingest_type(st, Tnode.Forall(bh)));
  | Surface_ty.List(elem) =>
    let* eh = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, elem);
    Ok(Store.ingest_type(st, Tnode.List(eh)));
  | Surface_ty.Record(fields) =>
    let rec go = (acc, fs) =>
      switch (fs) {
      | [] => Ok(List.rev(acc))
      | [(fname, fsty), ...rest] =>
        let* lh = resolve_label(~ns, ~st, ~mint, fname);
        let* fh = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, fsty);
        go([(lh, fh), ...acc], rest);
      };
    let* fields' = go([], fields);
    Ok(Store.ingest_type(st, Tnode.Record(fields')));
  | Surface_ty.Sig(items) =>
    /* the rank rule: opaque components bind TVars by label-hash sort order, so
       resolve every opaque's label first, then resolve payloads under a tvs
       extended with the opaque names in rank order (position = TVar index).
       Opaques are visible everywhere in the sig (order-insensitive); manifest
       names are declare-before-use and inline (no binding). */
    let item_name = (
      fun
      | Surface_ty.Stype(n)
      | Surface_ty.Stype_eq(n, _)
      | Surface_ty.Sfield(n, _) => n
    );
    switch (dup_names(List.map(item_name, items))) {
    | Some(n) => Error("sig: duplicate component name '" ++ n ++ "'")
    | None =>
      let rec with_labels = (acc, its) =>
        switch (its) {
        | [] => Ok(List.rev(acc))
        | [it, ...rest] =>
          let* lh = resolve_label(~ns, ~st, ~mint, item_name(it));
          with_labels([(it, lh), ...acc], rest);
        };
      let* labeled = with_labels([], items);
      let opaques =
        List.filter_map(
          ((it, lh)) =>
            switch (it) {
            | Surface_ty.Stype(n) => Some((lh, n))
            | _ => None
            },
          labeled,
        );
      let ranked =
        List.sort(((l1, _), (l2, _)) => String.compare(l1, l2), opaques);
      let tvs' = List.map(snd, ranked) @ tvs;
      let rec go = (acc, mts, its) =>
        switch (its) {
        | [] => Ok(List.rev(acc))
        | [(it, lh), ...rest] =>
          switch (it) {
          | Surface_ty.Stype(_) => go([(lh, Tnode.Sopaque), ...acc], mts, rest)
          | Surface_ty.Stype_eq(name, sty) =>
            let* eh = resolve_ty(~ns, ~st, ~tvs=tvs', ~mint, ~mtypes=mts, sty);
            go(
              [(lh, Tnode.Smanifest(eh)), ...acc],
              [(name, eh, List.length(tvs')), ...mts],
              rest,
            );
          | Surface_ty.Sfield(_, sty) =>
            let* fh = resolve_ty(~ns, ~st, ~tvs=tvs', ~mint, ~mtypes=mts, sty);
            go([(lh, Tnode.Sval(fh)), ...acc], mts, rest)
          }
        };
      let* comps = go([], mtypes, labeled);
      Ok(Store.ingest_type(st, Tnode.Sig(comps)));
    };
  | Surface_ty.Named(name) =>
    switch (lookup_ctx(tvs, name)) {
    | Some(i) => Ok(Store.ingest_type(st, Tnode.TVar(i)))
    | None =>
      switch (find_mtype(mtypes, name)) {
      | Some((h, entry_depth)) =>
        let d = List.length(tvs) - entry_depth;
        if (d == 0) {
          Ok(h);
        } else {
          let env = Store.build_env(st);
          Ok(Typecheck.shift_ty(env, d, 0, h));
        };
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
    }
  };

let op_to_node = (op: Surface.prim_op): Node.prim_op =>
  switch (op) {
  | Surface.Add => Node.Add
  | Surface.Sub => Node.Sub
  | Surface.Mul => Node.Mul
  | Surface.Eq => Node.Eq
  };

/* the term binders' types (mirrors ctx); None = could not be synthesized */
type tctx = list(option(Hash.t));

/* Try to synthesize a resolved term's type with the binder types we tracked.
   None when some enclosing binder's type is unknown or synthesis fails. */
let try_synth = (~st: Store.t, ~tctx: tctx, node: Node.t): option(Hash.t) => {
  let rec all_some = (
    fun
    | [] => Some([])
    | [Some(h), ...rest] => Option.map(r => [h, ...r], all_some(rest))
    | [None, ..._] => None
  );
  switch (all_some(tctx)) {
  | None => None
  | Some(ctx_tys) =>
    let env = Store.build_env(st);
    switch (Typecheck.synth(env, [], ctx_tys, node)) {
    | ty => Some(ty)
    | exception (Typecheck.Type_error(_)) => None
    };
  };
};

let last_segment = (name: string): string =>
  switch (String.rindex_opt(name, '.')) {
  | Some(i) => String.sub(name, i + 1, String.length(name) - i - 1)
  | None => name
  };

let rec resolve =
        (
          ~ctx: list(string),
          ~tvs: list(string)=[],
          ~tctx: tctx=[],
          ~mtypes: mtypes=[],
          ~mint: option(Mint.source)=None,
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
      /* dotted `M.field` whose head is a local binder is sugar for projection
         by label (alongside `M#field`) */
      let as_local_projection =
        switch (String.index_opt(name, '.')) {
        | Some(di) =>
          let head = String.sub(name, 0, di);
          let rest = String.sub(name, di + 1, String.length(name) - di - 1);
          switch (lookup_ctx(ctx, head)) {
          | Some(i) =>
            switch (resolve_label(~ns, ~st, ~mint=None, rest)) {
            | Ok(lh) => Some(Ok(Node.Project_field(Node.Var(i), lh)))
            | Error(e) => Some(Error(e))
            }
          | None => None
          };
        | None => None
        };
      switch (as_local_projection) {
      | Some(r) => r
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
      };
    }
  | Surface.Lit(n) => Ok(Node.Lit(n))
  | Surface.Bool(b) => Ok(Node.BoolLit(b))
  | Surface.Lam(x, sty, body) =>
    let* ann = resolve_ty(~ns, ~st, ~tvs, ~mtypes, sty);
    let* b =
      resolve(
        ~ctx=[x, ...ctx],
        ~tvs,
        ~tctx=[Some(ann), ...tctx],
        ~mtypes,
        ~mint,
        ~ns,
        ~st,
        body,
      );
    Ok(Node.Lam(ann, b));
  | Surface.App(f, a) =>
    let* f' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, f);
    let* a' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, a);
    Ok(Node.App(f', a'));
  | Surface.Let(x, rhs, body) =>
    let* r = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, rhs);
    let rty = try_synth(~st, ~tctx, r);
    let* b =
      resolve(
        ~ctx=[x, ...ctx],
        ~tvs,
        ~tctx=[rty, ...tctx],
        ~mtypes,
        ~mint,
        ~ns,
        ~st,
        body,
      );
    Ok(Node.Let(r, b));
  | Surface.If(c, t, e) =>
    let* c' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, c);
    let* t' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, t);
    let* e' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, e);
    Ok(Node.If(c', t', e'));
  | Surface.Pair(a, b) =>
    let* a' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, a);
    let* b' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, b);
    Ok(Node.Pair(a', b'));
  | Surface.Fst(p) =>
    let* p' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, p);
    Ok(Node.Fst(p'));
  | Surface.Snd(p) =>
    let* p' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, p);
    Ok(Node.Snd(p'));
  | Surface.Prim(op, args) =>
    let* args' = resolve_args(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, args);
    Ok(Node.Prim(op_to_node(op), args'));
  | Surface.TyLam(name, body) =>
    let env = Store.build_env(st);
    let tctx' = List.map(Option.map(Typecheck.shift_ty(env, 1, 0)), tctx);
    let* b =
      resolve(~ctx, ~tvs=[name, ...tvs], ~tctx=tctx', ~mtypes, ~mint, ~ns, ~st, body);
    Ok(Node.TyLam(b));
  | Surface.TyApp(e, sty) =>
    let* e' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, e);
    let* th = resolve_ty(~ns, ~st, ~tvs, ~mtypes, sty);
    Ok(Node.TyApp(e', th));
  | Surface.Nil(elem) =>
    let* eh = resolve_ty(~ns, ~st, ~tvs, ~mtypes, elem);
    Ok(Node.Nil(eh));
  | Surface.Cons(h, t) =>
    let* h' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, h);
    let* t' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, t);
    Ok(Node.Cons(h', t'));
  | Surface.Fold(l, z, f) =>
    let* l' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, l);
    let* z' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, z);
    let* f' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, f);
    Ok(Node.Fold(l', z', f'));
  /* `[| e1, ..., en |]` desugars to a cons-spine. The trailing `Nil` needs a
     concrete element type for content-addressing, and the substrate has no
     inference, so we synthesize the head element's type here and pin it. */
  | Surface.ListLit(es) =>
    let* nodes = resolve_args(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, es);
    switch (nodes) {
    | [] => Error("empty list literal: use nil [T]")
    | [head, ..._] =>
      switch (try_synth(~st, ~tctx, head)) {
      | None =>
        Error(
          "cannot infer list element type from the first element; annotate it or use cons/nil",
        )
      | Some(elem_h) =>
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
       first — the type declaration is the mint site) */
    let rec go = (acc, fs) =>
      switch (fs) {
      | [] => Ok(List.rev(acc))
      | [(fname, sval), ...rest] =>
        let* lh = resolve_label(~ns, ~st, ~mint=None, fname);
        let* v = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, sval);
        go([(lh, v), ...acc], rest);
      };
    let* fields' = go([], fields);
    Ok(Node.Record_lit(fields'));
  | Surface.Project(r, fname) =>
    let* r' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, r);
    let* lh = resolve_label(~ns, ~st, ~mint=None, fname);
    Ok(Node.Project_field(r', lh));
  | Surface.Struct(items) =>
    /* a struct is a module-implementation declaration: member names mint (when
       a mint source is supplied), and type members are transparent to the
       members that follow them (declare-before-use). */
    let item_name = (
      fun
      | Surface.Sitype(n, _)
      | Surface.Sival(n, _) => n
    );
    switch (dup_names(List.map(item_name, items))) {
    | Some(n) => Error("struct: duplicate member name '" ++ n ++ "'")
    | None =>
      let rec go = (acc, mts, its) =>
        switch (its) {
        | [] => Ok(List.rev(acc))
        | [Surface.Sitype(name, sty), ...rest] =>
          let* lh = resolve_label(~ns, ~st, ~mint, name);
          let* th = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes=mts, sty);
          go(
            [(lh, Node.Mtype(th)), ...acc],
            [(name, th, List.length(tvs)), ...mts],
            rest,
          );
        | [Surface.Sival(name, se), ...rest] =>
          let* lh = resolve_label(~ns, ~st, ~mint, name);
          let* e' =
            resolve(~ctx, ~tvs, ~tctx, ~mtypes=mts, ~mint, ~ns, ~st, se);
          go([(lh, Node.Mval(e')), ...acc], mts, rest);
        };
      let* members = go([], mtypes, items);
      Ok(Node.Struct(members));
    };
  | Surface.Ascribe(e, sty) =>
    let* e' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, e);
    /* an inline `sig {...}` in ascription position is a declaration gesture —
       let it mint (a `Named` sig is just a lookup either way) */
    let* th = resolve_ty(~ns, ~st, ~tvs, ~mint, ~mtypes, sty);
    Ok(Node.Ascribe({impl: e', sg: th}));
  | Surface.Open_local(scrut, m, body) =>
    let* scrut' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, scrut);
    switch (try_synth(~st, ~tctx, scrut')) {
    | None =>
      Error("open: cannot determine the module's signature at this position")
    | Some(sty) =>
      switch (Store.find(st, sty)) {
      | Some(Definition.Type(Tnode.Sig(comps))) =>
        let env = Store.build_env(st);
        let ranks = Tnode.opaque_ranks(comps);
        let k = List.length(ranks);
        let leaf = lh =>
          switch (Namespace.name_of(ns, lh)) {
          | Some(n) => last_segment(n)
          | None => Hash.short(lh)
          };
        /* rank order = TVar index order = tvs position order */
        let tvs' = List.map(lh => m ++ "." ++ leaf(lh), ranks) @ tvs;
        let mts' =
          List.filter_map(
            ((lh, c)) =>
              switch (c) {
              | Tnode.Smanifest(eh) =>
                /* the equation as stored sits under the sig's k binders, which
                   are exactly the body's k new binders — verbatim carry-over */
                Some((m ++ "." ++ leaf(lh), eh, List.length(tvs')))
              | _ => None
              },
            comps,
          )
          @ mtypes;
        let t_m = Typecheck.open_local_binder(env, comps);
        let tctx' = [
          Some(t_m),
          ...List.map(Option.map(Typecheck.shift_ty(env, k, 0)), tctx),
        ];
        let* body' =
          resolve(
            ~ctx=[m, ...ctx],
            ~tvs=tvs',
            ~tctx=tctx',
            ~mtypes=mts',
            ~mint,
            ~ns,
            ~st,
            body,
          );
        Ok(Node.Open_local({k, scrut: scrut', body: body'}));
      | _ => Error("open: the expression is not a module (its type is not a sig)")
      }
    };
  }

and resolve_args =
    (~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, args: list(Surface.t))
    : result(list(Node.t), string) =>
  switch (args) {
  | [] => Ok([])
  | [a, ...rest] =>
    let* a' = resolve(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, a);
    let* rest' = resolve_args(~ctx, ~tvs, ~tctx, ~mtypes, ~mint, ~ns, ~st, rest);
    Ok([a', ...rest']);
  };
