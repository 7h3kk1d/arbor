/* Name-aware rendering of a type hash. A bound name short-circuits (so an opaque
   type prints as `Counter.t`); otherwise the type is rendered structurally,
   recursing into component hashes (which may themselves be named). An opaque
   type with no name falls back to `opaque(m_xxxx)` — the witness is never shown
   (it would leak the representation outside an opening context). */

/* Type-variable binder names (distinct from term binders x,y,z). */
let tyvar_name = (k: int): string => {
  let base =
    switch (k mod 5) {
    | 0 => "t"
    | 1 => "u"
    | 2 => "s"
    | 3 => "r"
    | _ => "p"
    };
  k < 5 ? base : base ++ string_of_int(k / 5);
};

/* `tenv` is the type-variable binder names, innermost first: `TVar(i)` renders
   as `List.nth(tenv, i)`. `forall`/`/\` push one generated name; a `Sig` pushes
   its k opaque *label names* in rank order (rank i = position i), so a sig's
   payloads render `incr: t -> t` with the component's own name. */
let rec ty_to_string =
        (~ns: Namespace.t, ~st: Store.t, ~prec: int, ~tenv: list(string), h: Hash.t)
        : string =>
  switch (Namespace.name_of(ns, h)) {
  | Some(name) => name
  | None =>
    switch (Store.find(st, h)) {
    | Some(Definition.Type(Tnode.Int)) => "Int"
    | Some(Definition.Type(Tnode.Bool)) => "Bool"
    | Some(Definition.Type(Tnode.Arrow(a, b))) =>
      let s =
        ty_to_string(~ns, ~st, ~prec=1, ~tenv, a)
        ++ " -> "
        ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, b);
      prec >= 1 ? "(" ++ s ++ ")" : s;
    | Some(Definition.Type(Tnode.Product(a, b))) =>
      let s =
        ty_to_string(~ns, ~st, ~prec=2, ~tenv, a)
        ++ " * "
        ++ ty_to_string(~ns, ~st, ~prec=2, ~tenv, b);
      prec >= 2 ? "(" ++ s ++ ")" : s;
    | Some(Definition.Type(Tnode.Opaque({mint, _}))) =>
      "opaque(" ++ Mint.short(mint) ++ ")"
    | Some(Definition.Type(Tnode.TVar(i))) =>
      switch (List.nth_opt(tenv, i)) {
      | Some(n) => n
      | None => "t" ++ string_of_int(i)
      }
    | Some(Definition.Type(Tnode.Forall(body))) =>
      let name = tyvar_name(List.length(tenv));
      let s =
        "forall "
        ++ name
        ++ ". "
        ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv=[name, ...tenv], body);
      prec >= 1 ? "(" ++ s ++ ")" : s;
    | Some(Definition.Type(Tnode.Abstract(mint))) =>
      "abstract(" ++ Mint.short(mint) ++ ")"
    | Some(Definition.Type(Tnode.List(elem))) =>
      let s = "List " ++ ty_to_string(~ns, ~st, ~prec=2, ~tenv, elem);
      prec >= 5 ? "(" ++ s ++ ")" : s;
    | Some(Definition.Type(Tnode.Record(fields))) =>
      let label_name = lh =>
        switch (Namespace.name_of(ns, lh)) {
        | Some(n) => n
        | None => Hash.short(lh)
        };
      let parts =
        List.map(
          ((lh, ft)) =>
            label_name(lh) ++ ": " ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, ft),
          List.sort(((a, _), (b, _)) => String.compare(label_name(a), label_name(b)), fields),
        );
      "{ " ++ String.concat(", ", parts) ++ " }";
    | Some(Definition.Type(Tnode.Sig(comps))) =>
      let label_name = lh =>
        switch (Namespace.name_of(ns, lh)) {
        | Some(n) => n
        | None => Hash.short(lh)
        };
      let ranks = Tnode.opaque_ranks(comps);
      let tenv' = List.map(label_name, ranks) @ tenv;
      let by_name = ((a, _), (b, _)) =>
        String.compare(label_name(a), label_name(b));
      let pick = p => List.sort(by_name, List.filter(((_, c)) => p(c), comps));
      let opaques =
        pick(
          fun
          | Tnode.Sopaque => true
          | _ => false,
        );
      let manifests =
        pick(
          fun
          | Tnode.Smanifest(_) => true
          | _ => false,
        );
      let vals =
        pick(
          fun
          | Tnode.Sval(_) => true
          | _ => false,
        );
      let part = ((lh, c)) =>
        switch (c) {
        | Tnode.Sopaque => "type " ++ label_name(lh)
        | Tnode.Smanifest(e) =>
          "type "
          ++ label_name(lh)
          ++ " = "
          ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv=tenv', e)
        | Tnode.Sval(t) =>
          label_name(lh) ++ ": " ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv=tenv', t)
        };
      "sig { "
      ++ String.concat(", ", List.map(part, opaques @ manifests @ vals))
      ++ " }";
    | Some(Definition.Term(_)) => "<term>"
    | Some(Definition.Label(_)) => Hash.short(h)
    | None => Hash.short(h)
    }
  };

let ty = (~ns, ~st, h) => ty_to_string(~ns, ~st, ~prec=0, ~tenv=[], h);

/* Term rendering. Stored terms are name-free (de Bruijn); binder names are
   regenerated (`x`, `y`, ...), `Ref(hash)` renders via the namespace (falling
   back to a short hash), and operators are printed infix with precedence. */

let binder_name = (d: int): string => {
  let base =
    switch (d mod 6) {
    | 0 => "x"
    | 1 => "y"
    | 2 => "z"
    | 3 => "u"
    | 4 => "v"
    | _ => "w"
    };
  d < 6 ? base : base ++ string_of_int(d / 6);
};

let paren = (cond, s) => cond ? "(" ++ s ++ ")" : s;

/* A cons-spine ending in Nil is a literal list; collect its elements so it can
   render as `[| ... |]`. A spine ending in anything else (e.g. `cons x xs`)
   returns None and falls back to `cons h t`. */
let rec collect_list = (node: Node.t): option(list(Node.t)) =>
  switch (node) {
  | Node.Nil(_) => Some([])
  | Node.Cons(h, t) =>
    Option.map(rest => [h, ...rest], collect_list(t))
  | _ => None
  };

let prim_sym = (op: Node.prim_op): string =>
  switch (op) {
  | Node.Add => "+"
  | Node.Sub => "-"
  | Node.Mul => "mul"
  | Node.Eq => "=="
  };

/* Precedence: lambda/let/if/forall-abs = 0, == = 1, +/- = 2, mul = 3,
   application/type-application = 4, atoms = 5. `tenv` tracks enclosing type
   binders (`/\`, and `open`'s k hidden types) so annotations and `[T]`
   arguments name type variables. Known limitation: an `Open_local` body's
   hidden types render with generated names (the scrutinee's sig — and with it
   the labels — is not recoverable here without typing context). */
let rec term_prec =
        (
          ~ns: Namespace.t,
          ~st: Store.t,
          ~ctx: list(string),
          ~tenv: list(string),
          ~prec: int,
          node: Node.t,
        )
        : string =>
  switch (node) {
  | Node.Var(i) =>
    switch (List.nth_opt(ctx, i)) {
    | Some(n) => n
    | None => "$" ++ string_of_int(i)
    }
  | Node.Lit(n) => string_of_int(n)
  | Node.BoolLit(b) => b ? "true" : "false"
  | Node.Ref(h) =>
    switch (Namespace.name_of(ns, h)) {
    | Some(n) => n
    | None => Hash.short(h)
    }
  | Node.Lam(ann, body) =>
    let name = binder_name(List.length(ctx));
    paren(
      prec > 0,
      "\\"
      ++ name
      ++ ": "
      ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, ann)
      ++ ". "
      ++ term_prec(~ns, ~st, ~ctx=[name, ...ctx], ~tenv, ~prec=0, body),
    );
  | Node.Let(rhs, body) =>
    let name = binder_name(List.length(ctx));
    paren(
      prec > 0,
      "let "
      ++ name
      ++ " = "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, rhs)
      ++ " in "
      ++ term_prec(~ns, ~st, ~ctx=[name, ...ctx], ~tenv, ~prec=0, body),
    );
  | Node.If(c, t, e) =>
    paren(
      prec > 0,
      "if "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, c)
      ++ " then "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, t)
      ++ " else "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, e),
    )
  | Node.App(f, x) =>
    paren(
      prec > 4,
      term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=4, f)
      ++ " "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, x),
    )
  | Node.Fst(p) =>
    paren(prec > 4, "fst " ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, p))
  | Node.Snd(p) =>
    paren(prec > 4, "snd " ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, p))
  | Node.Pair(a, b) =>
    "("
    ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, a)
    ++ ", "
    ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, b)
    ++ ")"
  | Node.Prim(op, [a, b]) =>
    let (lvl, lp, rp) =
      switch (op) {
      | Node.Mul => (3, 3, 4)
      | Node.Add
      | Node.Sub => (2, 2, 3)
      | Node.Eq => (1, 2, 2)
      };
    paren(
      prec > lvl,
      term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=lp, a)
      ++ " "
      ++ prim_sym(op)
      ++ " "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=rp, b),
    );
  | Node.Prim(op, args) =>
    paren(
      prec > 4,
      prim_sym(op)
      ++ " "
      ++ String.concat(
           " ",
           List.map(term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5), args),
         ),
    )
  | Node.Seal({ty: t, _}) =>
    "<sealed: " ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, t) ++ ">"
  | Node.TyLam(body) =>
    let name = tyvar_name(List.length(tenv));
    paren(
      prec > 0,
      "/\\"
      ++ name
      ++ ". "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv=[name, ...tenv], ~prec=0, body),
    );
  | Node.TyApp(f, t) =>
    paren(
      prec > 4,
      term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=4, f)
      ++ " ["
      ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, t)
      ++ "]",
    )
  | Node.Struct(members) =>
    let label_name = lh =>
      switch (Namespace.name_of(ns, lh)) {
      | Some(n) => n
      | None => Hash.short(lh)
      };
    let by_name = ((a, _), (b, _)) =>
      String.compare(label_name(a), label_name(b));
    let types =
      List.sort(
        by_name,
        List.filter(
          ((_, m)) =>
            switch (m) {
            | Node.Mtype(_) => true
            | _ => false
            },
          members,
        ),
      );
    let vals =
      List.sort(
        by_name,
        List.filter(
          ((_, m)) =>
            switch (m) {
            | Node.Mval(_) => true
            | _ => false
            },
          members,
        ),
      );
    let part = ((lh, m)) =>
      switch (m) {
      | Node.Mtype(h) =>
        "type "
        ++ label_name(lh)
        ++ " = "
        ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, h)
      | Node.Mval(e) =>
        label_name(lh) ++ " = " ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, e)
      };
    "struct { "
    ++ String.concat(", ", List.map(part, types @ vals))
    ++ " }";
  | Node.Ascribe({impl, sg}) =>
    paren(
      prec > 0,
      term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=1, impl)
      ++ " :> "
      ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, sg),
    )
  | Node.Open({pkg, _}) =>
    paren(prec > 4, "open " ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, pkg))
  | Node.Open_local({k, scrut, body}) =>
    let mname = binder_name(List.length(ctx));
    let fresh = List.init(k, i => tyvar_name(List.length(tenv) + i));
    paren(
      prec > 0,
      "open "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=1, scrut)
      ++ " as "
      ++ mname
      ++ " in "
      ++ term_prec(
           ~ns,
           ~st,
           ~ctx=[mname, ...ctx],
           ~tenv=fresh @ tenv,
           ~prec=0,
           body,
         ),
    );
  | Node.Nil(elem) => "nil [" ++ ty_to_string(~ns, ~st, ~prec=0, ~tenv, elem) ++ "]"
  | Node.Cons(h, t) =>
    switch (collect_list(node)) {
    | Some(elems) =>
      "[| "
      ++ String.concat(
           ", ",
           List.map(term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0), elems),
         )
      ++ " |]"
    | None =>
      paren(
        prec > 4,
        "cons "
        ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, h)
        ++ " "
        ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, t),
      )
    }
  | Node.Fold(lst, z, f) =>
    paren(
      prec > 4,
      "fold "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, lst)
      ++ " "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, z)
      ++ " "
      ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=5, f),
    )
  | Node.Record_lit(fields) =>
    let label_name = lh =>
      switch (Namespace.name_of(ns, lh)) {
      | Some(n) => n
      | None => Hash.short(lh)
      };
    let parts =
      List.map(
        ((lh, v)) =>
          label_name(lh) ++ " = " ++ term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=0, v),
        List.sort(((a, _), (b, _)) => String.compare(label_name(a), label_name(b)), fields),
      );
    "{ " ++ String.concat(", ", parts) ++ " }";
  | Node.Project_field(r, lh) =>
    let label_name =
      switch (Namespace.name_of(ns, lh)) {
      | Some(n) => n
      | None => Hash.short(lh)
      };
    paren(prec > 5, term_prec(~ns, ~st, ~ctx, ~tenv, ~prec=6, r) ++ "#" ++ label_name);
  };

let term = (~ns, ~st, node) => term_prec(~ns, ~st, ~ctx=[], ~tenv=[], ~prec=0, node);
