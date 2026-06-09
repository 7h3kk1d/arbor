/* Name-aware rendering of a type hash. A bound name short-circuits (so an opaque
   type prints as `Counter.t`); otherwise the type is rendered structurally,
   recursing into component hashes (which may themselves be named). An opaque
   type with no name falls back to `opaque(m_xxxx)` — the witness is never shown
   (it would leak the representation outside an opening context). */

let rec ty_to_string =
        (~ns: Namespace.t, ~st: Store.t, ~prec: int, h: Hash.t): string =>
  switch (Namespace.name_of(ns, h)) {
  | Some(name) => name
  | None =>
    switch (Store.find(st, h)) {
    | Some(Definition.Type(Tnode.Int)) => "Int"
    | Some(Definition.Type(Tnode.Bool)) => "Bool"
    | Some(Definition.Type(Tnode.Arrow(a, b))) =>
      let s =
        ty_to_string(~ns, ~st, ~prec=1, a)
        ++ " -> "
        ++ ty_to_string(~ns, ~st, ~prec=0, b);
      if (prec >= 1) {
        "(" ++ s ++ ")";
      } else {
        s;
      };
    | Some(Definition.Type(Tnode.Product(a, b))) =>
      let s =
        ty_to_string(~ns, ~st, ~prec=2, a)
        ++ " * "
        ++ ty_to_string(~ns, ~st, ~prec=2, b);
      if (prec >= 2) {
        "(" ++ s ++ ")";
      } else {
        s;
      };
    | Some(Definition.Type(Tnode.Opaque({mint, _}))) =>
      "opaque(" ++ Mint.short(mint) ++ ")"
    | Some(Definition.Term(_)) => "<term>"
    | None => Hash.short(h)
    }
  };

let ty = (~ns, ~st, h) => ty_to_string(~ns, ~st, ~prec=0, h);

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

let prim_sym = (op: Node.prim_op): string =>
  switch (op) {
  | Node.Add => "+"
  | Node.Sub => "-"
  | Node.Mul => "mul"
  | Node.Eq => "=="
  };

/* Precedence: lambda/let/if = 0, == = 1, +/- = 2, mul = 3, application = 4,
   atoms = 5. A subterm wraps in parens when its level is below the context. */
let rec term_prec =
        (~ns: Namespace.t, ~st: Store.t, ~ctx: list(string), ~prec: int, node: Node.t)
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
      ++ ty(~ns, ~st, ann)
      ++ ". "
      ++ term_prec(~ns, ~st, ~ctx=[name, ...ctx], ~prec=0, body),
    );
  | Node.Let(rhs, body) =>
    let name = binder_name(List.length(ctx));
    paren(
      prec > 0,
      "let "
      ++ name
      ++ " = "
      ++ term_prec(~ns, ~st, ~ctx, ~prec=0, rhs)
      ++ " in "
      ++ term_prec(~ns, ~st, ~ctx=[name, ...ctx], ~prec=0, body),
    );
  | Node.If(c, t, e) =>
    paren(
      prec > 0,
      "if "
      ++ term_prec(~ns, ~st, ~ctx, ~prec=0, c)
      ++ " then "
      ++ term_prec(~ns, ~st, ~ctx, ~prec=0, t)
      ++ " else "
      ++ term_prec(~ns, ~st, ~ctx, ~prec=0, e),
    )
  | Node.App(f, x) =>
    paren(
      prec > 4,
      term_prec(~ns, ~st, ~ctx, ~prec=4, f)
      ++ " "
      ++ term_prec(~ns, ~st, ~ctx, ~prec=5, x),
    )
  | Node.Fst(p) => paren(prec > 4, "fst " ++ term_prec(~ns, ~st, ~ctx, ~prec=5, p))
  | Node.Snd(p) => paren(prec > 4, "snd " ++ term_prec(~ns, ~st, ~ctx, ~prec=5, p))
  | Node.Pair(a, b) =>
    "("
    ++ term_prec(~ns, ~st, ~ctx, ~prec=0, a)
    ++ ", "
    ++ term_prec(~ns, ~st, ~ctx, ~prec=0, b)
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
      term_prec(~ns, ~st, ~ctx, ~prec=lp, a)
      ++ " "
      ++ prim_sym(op)
      ++ " "
      ++ term_prec(~ns, ~st, ~ctx, ~prec=rp, b),
    );
  | Node.Prim(op, args) =>
    paren(
      prec > 4,
      prim_sym(op)
      ++ " "
      ++ String.concat(" ", List.map(term_prec(~ns, ~st, ~ctx, ~prec=5), args)),
    )
  | Node.Seal({ty: t, _}) => "<sealed: " ++ ty(~ns, ~st, t) ++ ">"
  };

let term = (~ns, ~st, node) => term_prec(~ns, ~st, ~ctx=[], ~prec=0, node);
