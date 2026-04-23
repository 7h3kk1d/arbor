{
  open Stlc_parser

  exception Lex_error of string
}

let ws = [' ' '\t' '\r']+
let ident_start = ['a'-'z' 'A'-'Z' '_']
let ident_cont  = ['a'-'z' 'A'-'Z' '0'-'9' '_']

rule token = parse
  | ws              { token lexbuf }
  | '\n'            { Lexing.new_line lexbuf; token lexbuf }
  | '\\'            { S_BACKSLASH }
  | '.'             { S_DOT }
  | ':'             { S_COLON }
  | "->"            { S_ARROW }
  | '('             { S_LPAREN }
  | ')'             { S_RPAREN }
  | "true"          { S_TRUE }
  | "false"         { S_FALSE }
  | "if"            { S_IF }
  | "then"          { S_THEN }
  | "else"          { S_ELSE }
  | "Bool"          { S_BOOL }
  | ident_start ident_cont* { S_IDENT (Lexing.lexeme lexbuf) }
  | eof             { S_EOF }
  | _ as c          { raise (Lex_error (Printf.sprintf "unexpected character: %C" c)) }
