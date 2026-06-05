/* Fail-fast wrappers over the Menhir monolithic parser. */

let parse_expr = (s: string): result(Surface.t, string) => {
  let lexbuf = Lexing.from_string(s);
  try(Ok(Parser.main(Lexer.token, lexbuf))) {
  | Lexer.Error(m) => Error(m)
  | Parser.Error => Error("syntax error")
  };
};

let parse_ty = (s: string): result(Surface_ty.t, string) => {
  let lexbuf = Lexing.from_string(s);
  try(Ok(Parser.main_ty(Lexer.token, lexbuf))) {
  | Lexer.Error(m) => Error(m)
  | Parser.Error => Error("syntax error in type")
  };
};
