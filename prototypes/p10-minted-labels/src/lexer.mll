{
  open Parser

  (* Total lexer in p9: never raises.
     - '?' emits a HOLE token (explicit user-entered hole).
     - Any unknown byte emits a HOLE token and advances one position.
     The recovery layer (Parse_recover) deals with HOLE tokens that
     land in positions the grammar can't accept. *)

  let unescape_string s =
    let buf = Buffer.create (String.length s) in
    let n = String.length s in
    let i = ref 0 in
    while !i < n do
      let c = s.[!i] in
      if c = '\\' && !i + 1 < n then begin
        let next = s.[!i + 1] in
        let out =
          match next with
          | 'n' -> '\n'
          | 't' -> '\t'
          | 'r' -> '\r'
          | '\\' -> '\\'
          | '"' -> '"'
          | _ -> next
        in
        Buffer.add_char buf out;
        i := !i + 2
      end else begin
        Buffer.add_char buf c;
        incr i
      end
    done;
    Buffer.contents buf
}

let ws = [' ' '\t' '\r']+
let lower_start = ['a'-'z' '_']
let upper_start = ['A'-'Z']
let ident_cont  = ['a'-'z' 'A'-'Z' '0'-'9' '_']
let lower_ident = lower_start ident_cont*
let upper_ident = upper_start ident_cont*
(* p10 disambiguation: a Capitalized leading segment makes the whole
   dotted name one IDENT (namespace path; can have lowercase tail
   segments like `Math.add` or further capitalized like
   `Geom.Point`). A lowercase leading segment does NOT fuse across
   dots, so `p.x` lexes as IDENT-DOT-IDENT (field projection) and
   `p.0` lexes as IDENT-DOT-INT_LIT (tuple index). *)
let any_ident   = (lower_start | upper_start) ident_cont*
let digit = ['0'-'9']

rule token = parse
  | ws              { token lexbuf }
  | '\n'            { Lexing.new_line lexbuf; token lexbuf }
  (* keywords (lowercase) — checked before lower_ident *)
  | "let"           { LET }
  | "in"            { IN }
  | "if"            { IF }
  | "then"          { THEN }
  | "else"          { ELSE }
  | "true"          { BOOL_LIT true }
  | "false"         { BOOL_LIT false }
  | "fst"           { FST }
  | "snd"           { SND }
  | "not"           { NOT }
  | "mul"           { MUL_KW }
  | "mod"           { MOD_KW }
  | "with"          { WITH }
  (* type-name keywords (uppercase) — checked before upper_ident *)
  | "Int"           { TY_INT }
  | "Bool"          { TY_BOOL }
  | "String"        { TY_STRING }
  | "List"          { TY_LIST }
  (* operators / punctuation *)
  | "=="            { EQEQ }
  | "++"            { CONCAT_OP }
  | "=>"            { FATARROW }
  | "->"            { ARROW }
  | "&&"            { ANDAND }
  | "||"            { OROR }
  | '\\'            { BACKSLASH }
  | '.'             { DOT }
  | '('             { LPAREN }
  | ')'             { RPAREN }
  | '['             { LBRACKET }
  | ']'             { RBRACKET }
  | '{'             { LBRACE }
  | '}'             { RBRACE }
  | ','             { COMMA }
  | ':'             { COLON }
  | '='             { EQ }
  | '+'             { PLUS }
  | '-'             { MINUS }
  | '*'             { STAR }
  | '/'             { SLASH }
  | '?'             { HOLE }
  (* literals — int + string *)
  | digit+          { INT_LIT (int_of_string (Lexing.lexeme lexbuf)) }
  | '"' ([^ '"' '\\'] | '\\' _)* '"' as raw {
      let inner = String.sub raw 1 (String.length raw - 2) in
      STRING_LIT (unescape_string inner)
    }
  (* Identifiers. Two cases:
     - Capitalized-leading dotted name fuses into one IDENT: `Math.add`,
       `Geom.Point`, `Geom.Point.x` (namespace paths).
     - Lowercase-leading does NOT fuse across dots: `p.x` produces
       IDENT(p) DOT IDENT(x). This lets the parser route `p.x` to
       field projection on the value `p`. Lowercase standalone idents
       (`x`, `add`) are normal IDENTs. *)
  | upper_ident ('.' any_ident)+   { IDENT (Lexing.lexeme lexbuf) }
  | lower_ident                    { IDENT (Lexing.lexeme lexbuf) }
  | upper_ident                    { IDENT (Lexing.lexeme lexbuf) }
  | eof             { EOF }
  | _               { HOLE }
