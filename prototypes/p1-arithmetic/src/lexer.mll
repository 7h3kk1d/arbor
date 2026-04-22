{
  open Parser

  exception Lex_error of string
}

let ws = [' ' '\t' '\r']+

rule token = parse
  | ws              { token lexbuf }
  | '\n'            { Lexing.new_line lexbuf; token lexbuf }
  | "true"          { TRUE }
  | "false"         { FALSE }
  | "0"             { ZERO }
  | "succ"          { SUCC }
  | "pred"          { PRED }
  | "iszero"        { ISZERO }
  | "if"            { IF }
  | "then"          { THEN }
  | "else"          { ELSE }
  | '('             { LPAREN }
  | ')'             { RPAREN }
  | eof             { EOF }
  | _ as c          { raise (Lex_error (Printf.sprintf "unexpected character: %C" c)) }
