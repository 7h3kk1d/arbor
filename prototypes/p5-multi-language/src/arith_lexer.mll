{
  open Arith_parser

  exception Lex_error of string
}

let ws = [' ' '\t' '\r']+
let ident_start = ['a'-'z' 'A'-'Z' '_']
let ident_cont  = ['a'-'z' 'A'-'Z' '0'-'9' '_']

rule token = parse
  | ws              { token lexbuf }
  | '\n'            { Lexing.new_line lexbuf; token lexbuf }
  | "true"          { A_TRUE }
  | "false"         { A_FALSE }
  | "0"             { A_ZERO }
  | "succ"          { A_SUCC }
  | "pred"          { A_PRED }
  | "iszero"        { A_ISZERO }
  | "if"            { A_IF }
  | "then"          { A_THEN }
  | "else"          { A_ELSE }
  | '('             { A_LPAREN }
  | ')'             { A_RPAREN }
  | ident_start ident_cont* { A_IDENT (Lexing.lexeme lexbuf) }
  | eof             { A_EOF }
  | _ as c          { raise (Lex_error (Printf.sprintf "unexpected character: %C" c)) }
