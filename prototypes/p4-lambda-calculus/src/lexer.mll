{
  open Parser

  exception Lex_error of string
}

let ws = [' ' '\t' '\r']+
let ident_start = ['a'-'z' 'A'-'Z' '_']
let ident_cont  = ['a'-'z' 'A'-'Z' '0'-'9' '_']

rule token = parse
  | ws              { token lexbuf }
  | '\n'            { Lexing.new_line lexbuf; token lexbuf }
  | '\\'            { BACKSLASH }
  | '.'             { DOT }
  | '('             { LPAREN }
  | ')'             { RPAREN }
  | ident_start ident_cont* { IDENT (Lexing.lexeme lexbuf) }
  | eof             { EOF }
  | _ as c          { raise (Lex_error (Printf.sprintf "unexpected character: %C" c)) }
