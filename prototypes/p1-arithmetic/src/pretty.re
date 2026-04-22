/* Minimal parenthesization: parens only where the parser would otherwise
   misread. Roundtrips through the parser. */

let rec print =
  fun
  | Ast.True => "true"
  | Ast.False => "false"
  | Ast.Zero => "0"
  | Ast.Succ(a) => "succ " ++ atom(a)
  | Ast.Pred(a) => "pred " ++ atom(a)
  | Ast.IsZero(a) => "iszero " ++ atom(a)
  | Ast.If(c, t, e) =>
    "if " ++ print(c) ++ " then " ++ print(t) ++ " else " ++ print(e)
and atom = t =>
  switch (t) {
  | Ast.True
  | Ast.False
  | Ast.Zero => print(t)
  | _ => "(" ++ print(t) ++ ")"
  };
