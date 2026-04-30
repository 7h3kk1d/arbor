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
  (* type-name keywords (uppercase) — checked before upper_ident *)
  | "Int"           { TY_INT }
  | "Bool"          { TY_BOOL }
  | "String"        { TY_STRING }
  (* operators / punctuation *)
  | "=="            { EQEQ }
  | "++"            { CONCAT_OP }
  | "->"            { ARROW }
  | "&&"            { ANDAND }
  | "||"            { OROR }
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
  | '/'             { SLASH }
  | '?'             { HOLE }
  (* literals — int + string *)
  | digit+          { INT_LIT (int_of_string (Lexing.lexeme lexbuf)) }
  | '"' ([^ '"' '\\'] | '\\' _)* '"' as raw {
      let inner = String.sub raw 1 (String.length raw - 2) in
      STRING_LIT (unescape_string inner)
    }
  (* identifiers — dotted is greedy, so `math.add` is one IDENT *)
  | lower_ident ('.' lower_ident)+ { IDENT (Lexing.lexeme lexbuf) }
  | lower_ident                    { IDENT (Lexing.lexeme lexbuf) }
  | upper_ident                    { IDENT (Lexing.lexeme lexbuf) }
  | eof             { EOF }
  | _               { HOLE }
