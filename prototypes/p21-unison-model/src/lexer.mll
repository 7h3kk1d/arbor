{
  exception Error of string

  let keyword_or_ident s =
    match s with
    | "Nat"  -> Parser.NAT_TY
    | "Text" -> Parser.TEXT_TY
    | _      -> Parser.IDENT s
}

let digit  = ['0'-'9']
let lower  = ['a'-'z' '_']
let upper  = ['A'-'Z']
let idchar = ['a'-'z' 'A'-'Z' '_' '0'-'9' '\'']
let opchar = ['+' '*' '-' '/' '<' '>' '=' '!' '&' '|' '^' '%' '~' '?' '@' '$']

(* A qualified name has a Capitalized first segment: Nat.+, Text.size. An
   unqualified one is lowercase and cannot contain a dot, so `\x. x` and
   `\x.x` both lex as a binder, a dot and a variable -- the dotted-identifier
   clash that bit p16 does not arise here. *)
let uname = lower idchar*
let qname = upper idchar* ('.' (lower idchar* | opchar+ | upper idchar*))+

rule token = parse
  | "--" [^ '\n']*        { token lexbuf }
  | '\n'                  { Lexing.new_line lexbuf; token lexbuf }
  | [' ' '\t' '\r']+      { token lexbuf }
  | '\\'                  { Parser.LAMBDA }
  | "->"                  { Parser.ARROW }
  | '.'                   { Parser.DOT }
  | '='                   { Parser.EQUALS }
  | ':'                   { Parser.COLON }
  | '('                   { Parser.LPAREN }
  | ')'                   { Parser.RPAREN }
  | '>'                   { Parser.GT }
  | digit+ as n           { Parser.NATLIT (int_of_string n) }
  | '"'                   { Parser.TEXTLIT (string_lit (Buffer.create 16) lexbuf) }
  | qname as s            { Parser.IDENT s }
  | uname as s            { keyword_or_ident s }
  | upper idchar* as s    { keyword_or_ident s }
  | eof                   { Parser.EOF }
  | _ as c                { raise (Error (Printf.sprintf "unexpected character %C" c)) }

and string_lit buf = parse
  | '"'                   { Buffer.contents buf }
  | '\\' 'n'              { Buffer.add_char buf '\n'; string_lit buf lexbuf }
  | '\\' 't'              { Buffer.add_char buf '\t'; string_lit buf lexbuf }
  | '\\' '"'              { Buffer.add_char buf '"';  string_lit buf lexbuf }
  | '\\' '\\'             { Buffer.add_char buf '\\'; string_lit buf lexbuf }
  | eof                   { raise (Error "unterminated text literal") }
  | _ as c                { Buffer.add_char buf c; string_lit buf lexbuf }
