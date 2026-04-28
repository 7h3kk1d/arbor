{
  open Parser

  (* The lexer is total in p8: no exceptions leave this module.
     - '?' emits a HOLE token (the explicit user-entered hole).
     - Any unknown byte emits a HOLE token and advances one position.
     The recovery layer (Parse_recover) deals with HOLE tokens that
     land in positions the grammar can't accept. *)
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
  | '?'             { HOLE }
  | ident_start ident_cont* { IDENT (Lexing.lexeme lexbuf) }
  | eof             { EOF }
  | _               { HOLE }
