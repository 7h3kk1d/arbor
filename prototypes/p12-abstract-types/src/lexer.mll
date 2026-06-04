{
  open Parser
  exception Error of string
}

let ws = [' ' '\t' '\r']+
let lower_start = ['a'-'z' '_']
let upper_start = ['A'-'Z']
let ident_cont  = ['a'-'z' 'A'-'Z' '0'-'9' '_']
let any_ident   = (lower_start | upper_start) ident_cont*
let digit = ['0'-'9']

rule token = parse
  | ws              { token lexbuf }
  | '\n'            { Lexing.new_line lexbuf; token lexbuf }
  (* keywords *)
  | "let"           { LET }
  | "in"            { IN }
  | "if"            { IF }
  | "then"          { THEN }
  | "else"          { ELSE }
  | "true"          { BOOL_LIT true }
  | "false"         { BOOL_LIT false }
  | "fst"           { FST }
  | "snd"           { SND }
  | "mul"           { MUL_KW }
  | "Int"           { TY_INT }
  | "Bool"          { TY_BOOL }
  (* operators / punctuation *)
  | "=="            { EQEQ }
  | "->"            { ARROW }
  | '\\'            { BACKSLASH }
  | '.'             { DOT }
  | '('             { LPAREN }
  | ')'             { RPAREN }
  | ','             { COMMA }
  | ':'             { COLON }
  | '='             { EQ }
  | '+'             { PLUS }
  | '-'             { MINUS }
  | '*'             { STAR }
  | digit+          { INT_LIT (int_of_string (Lexing.lexeme lexbuf)) }
  (* dotted is greedy: `Counter.incr` is one IDENT. A lambda's `.` separator
     must therefore be followed by whitespace: `\x: Int. x`, not `\x: Int.x`. *)
  | any_ident ('.' any_ident)+     { IDENT (Lexing.lexeme lexbuf) }
  | any_ident                      { IDENT (Lexing.lexeme lexbuf) }
  | eof             { EOF }
  | _               { raise (Error ("unexpected character: " ^ Lexing.lexeme lexbuf)) }
