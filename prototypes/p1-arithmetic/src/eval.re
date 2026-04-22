/* Big-step evaluator for TAPL Ch. 3 untyped arithmetic.
   Normal forms that aren't values (e.g. `succ true`, `if 0 then ..`)
   surface as Stuck, carrying the innermost stuck term for diagnostics. */

type result =
  | Value(Ast.t)
  | Stuck(Ast.t);

let rec eval = (t: Ast.t): result =>
  switch (t) {
  | Ast.True
  | Ast.False
  | Ast.Zero => Value(t)
  | Ast.Succ(a) =>
    switch (eval(a)) {
    | Value(v) when Ast.is_numeric(v) => Value(Ast.Succ(v))
    | Value(_) => Stuck(t)
    | Stuck(_) as s => s
    }
  | Ast.Pred(a) =>
    switch (eval(a)) {
    | Value(Ast.Zero) => Value(Ast.Zero)
    | Value(Ast.Succ(nv)) when Ast.is_numeric(nv) => Value(nv)
    | Value(_) => Stuck(t)
    | Stuck(_) as s => s
    }
  | Ast.IsZero(a) =>
    switch (eval(a)) {
    | Value(Ast.Zero) => Value(Ast.True)
    | Value(Ast.Succ(nv)) when Ast.is_numeric(nv) => Value(Ast.False)
    | Value(_) => Stuck(t)
    | Stuck(_) as s => s
    }
  | Ast.If(c, thn, els) =>
    switch (eval(c)) {
    | Value(Ast.True) => eval(thn)
    | Value(Ast.False) => eval(els)
    | Value(_) => Stuck(t)
    | Stuck(_) as s => s
    }
  };

let print_result =
  fun
  | Value(v) => Pretty.print(v)
  | Stuck(t) => "⟂ stuck at: " ++ Pretty.print(t);
