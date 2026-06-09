/* Call-by-value evaluator. Abstraction is erased at runtime: a `Seal` is
   transparent (evaluate its impl), and an abstract-type value is just its
   underlying representation value. So `bump2 Counter.empty` reduces through the
   seals to a plain Int. `Ref`/`Seal.impl` dereference into the store and
   evaluate the (closed) definition in the empty environment. */

type value =
  | VInt(int)
  | VBool(bool)
  | VPair(value, value)
  | VClosure(list(value), Node.t); /* captured env, lambda body */

exception Stuck(string);

let rec value_eq = (a: value, b: value): bool =>
  switch (a, b) {
  | (VInt(x), VInt(y)) => x == y
  | (VBool(x), VBool(y)) => x == y
  | (VPair(a1, a2), VPair(b1, b2)) => value_eq(a1, b1) && value_eq(a2, b2)
  | (_, _) => false
  };

let rec eval = (st: Store.t, env: list(value), node: Node.t): value =>
  switch (node) {
  | Node.Var(i) =>
    switch (List.nth_opt(env, i)) {
    | Some(v) => v
    | None => raise(Stuck("unbound variable " ++ string_of_int(i)))
    }
  | Node.Lit(n) => VInt(n)
  | Node.BoolLit(b) => VBool(b)
  | Node.Lam(_, body) => VClosure(env, body)
  | Node.App(f, x) =>
    let fv = eval(st, env, f);
    let xv = eval(st, env, x);
    switch (fv) {
    | VClosure(cenv, body) => eval(st, [xv, ...cenv], body)
    | _ => raise(Stuck("application of a non-function"))
    };
  | Node.Let(rhs, body) =>
    let rv = eval(st, env, rhs);
    eval(st, [rv, ...env], body);
  | Node.Pair(a, b) => VPair(eval(st, env, a), eval(st, env, b))
  | Node.Fst(p) =>
    switch (eval(st, env, p)) {
    | VPair(a, _) => a
    | _ => raise(Stuck("fst of a non-pair"))
    }
  | Node.Snd(p) =>
    switch (eval(st, env, p)) {
    | VPair(_, b) => b
    | _ => raise(Stuck("snd of a non-pair"))
    }
  | Node.If(c, t, e) =>
    switch (eval(st, env, c)) {
    | VBool(true) => eval(st, env, t)
    | VBool(false) => eval(st, env, e)
    | _ => raise(Stuck("if on a non-bool"))
    }
  | Node.Prim(op, args) => eval_prim(st, env, op, args)
  | Node.Ref(h) => eval_ref(st, h)
  | Node.Seal({impl, _}) => eval_ref(st, impl) /* seal is transparent at runtime */
  | Node.TyLam(body) => eval(st, env, body) /* type abstraction erases */
  | Node.TyApp(f, _) => eval(st, env, f) /* type application erases */
  }

and eval_ref = (st: Store.t, h: Hash.t): value =>
  switch (Store.find(st, h)) {
  | Some(Definition.Term(node)) => eval(st, [], node)
  | Some(Definition.Type(_)) => raise(Stuck("reference to a type in term position"))
  | None => raise(Stuck("dangling reference"))
  }

and eval_prim = (st: Store.t, env, op: Node.prim_op, args: list(Node.t)): value =>
  switch (op, args) {
  | (Node.Add, [a, b]) => int_bin(st, env, a, b, (x, y) => x + y)
  | (Node.Sub, [a, b]) => int_bin(st, env, a, b, (x, y) => x - y)
  | (Node.Mul, [a, b]) => int_bin(st, env, a, b, (x, y) => x * y)
  | (Node.Eq, [a, b]) => VBool(value_eq(eval(st, env, a), eval(st, env, b)))
  | (_, _) => raise(Stuck("primitive arity mismatch"))
  }

and int_bin = (st: Store.t, env, a, b, f: (int, int) => int): value =>
  switch (eval(st, env, a), eval(st, env, b)) {
  | (VInt(x), VInt(y)) => VInt(f(x, y))
  | (_, _) => raise(Stuck("arithmetic on non-ints"))
  };

let rec to_string = (v: value): string =>
  switch (v) {
  | VInt(n) => string_of_int(n)
  | VBool(b) => b ? "true" : "false"
  | VPair(a, b) => "(" ++ to_string(a) ++ ", " ++ to_string(b) ++ ")"
  | VClosure(_, _) => "<closure>"
  };

let eval_top = (st: Store.t, node: Node.t): result(value, string) =>
  try(Ok(eval(st, [], node))) {
  | Stuck(m) => Error(m)
  };
