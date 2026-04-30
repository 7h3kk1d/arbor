%{
  let mk_lam x ty body = Surface_ast.Lam (x, ty, body)
  let mk_let x rhs body = Surface_ast.Let (x, rhs, body)
  let mk_if c t e = Surface_ast.If (c, t, e)
  let mk_pair a b = Surface_ast.Pair (a, b)
  let mk_app f a = Surface_ast.App (f, a)
  let prim op args = Surface_ast.Prim (op, args)
  let default_ty = Ty.Int
%}

%token LET IN IF THEN ELSE FST SND NOT MUL_KW MOD_KW
%token TY_INT TY_BOOL TY_STRING
%token EQEQ CONCAT_OP ARROW ANDAND OROR
%token BACKSLASH DOT LPAREN RPAREN COMMA COLON EQ
%token PLUS MINUS STAR SLASH
%token <string> IDENT
%token <int> INT_LIT
%token <bool> BOOL_LIT
%token <string> STRING_LIT
%token HOLE
%token EOF

%start <Surface_ast.t> main

%%

main:
  | e = expr; EOF { e }

(* Top-level expression productions. The "partial-form" productions
   (truncated let/if/lambda) let the grammar fall forward when a binder
   construct is unfinished at EOF or RPAREN — the parser produces a
   Hole-bearing AST instead of forcing the recovery driver to bail out.
   The driver's rewind-and-inject-HOLE strategy handles every other
   shape of malformed input. *)

expr:
  | LET; x = binder; EQ; rhs = expr; IN; body = expr
      { mk_let x rhs body }
  | LET; x = binder; EQ; rhs = expr
      { mk_let x rhs Surface_ast.Hole }
  | LET; x = binder; EQ
      { mk_let x Surface_ast.Hole Surface_ast.Hole }
  | LET; x = binder
      { mk_let x Surface_ast.Hole Surface_ast.Hole }
  | LET
      { mk_let "?" Surface_ast.Hole Surface_ast.Hole }
  | IF; c = expr; THEN; t = expr; ELSE; e = expr
      { mk_if c t e }
  | IF; c = expr; THEN; t = expr
      { mk_if c t Surface_ast.Hole }
  | IF; c = expr; THEN
      { mk_if c Surface_ast.Hole Surface_ast.Hole }
  | IF; c = expr
      { mk_if c Surface_ast.Hole Surface_ast.Hole }
  | IF
      { mk_if Surface_ast.Hole Surface_ast.Hole Surface_ast.Hole }
  | BACKSLASH; x = binder; COLON; ty = ty; DOT; body = expr
      { mk_lam x ty body }
  | BACKSLASH; x = binder; COLON; ty = ty; DOT
      { mk_lam x ty Surface_ast.Hole }
  | BACKSLASH; x = binder; COLON; ty = ty
      { mk_lam x ty Surface_ast.Hole }
  | BACKSLASH; x = binder; COLON
      { mk_lam x default_ty Surface_ast.Hole }
  | BACKSLASH; x = binder; DOT
      { mk_lam x default_ty Surface_ast.Hole }
  | BACKSLASH; x = binder
      { mk_lam x default_ty Surface_ast.Hole }
  | BACKSLASH
      { mk_lam "?" default_ty Surface_ast.Hole }
  | e = or_expr { e }

binder:
  | x = IDENT { x }
  | HOLE      { "?" }

(* Boolean / equality / arithmetic / string-concat operators, layered by
   precedence (low to high). Each layer's left-recursion gives natural
   left-associativity for `1 + 2 + 3`. *)

or_expr:
  | l = or_expr; OROR; r = and_expr   { prim Surface_ast.Or  [l; r] }
  | e = and_expr                       { e }

and_expr:
  | l = and_expr; ANDAND; r = eq_expr { prim Surface_ast.And [l; r] }
  | e = eq_expr                        { e }

eq_expr:
  | l = concat_expr; EQEQ; r = concat_expr { prim Surface_ast.Eq [l; r] }
  | e = concat_expr                          { e }

concat_expr:
  | l = add_expr; CONCAT_OP; r = concat_expr { prim Surface_ast.Concat [l; r] }
  | e = add_expr                              { e }

add_expr:
  | l = add_expr; PLUS;  r = mul_expr { prim Surface_ast.Add [l; r] }
  | l = add_expr; MINUS; r = mul_expr { prim Surface_ast.Sub [l; r] }
  | e = mul_expr                       { e }

mul_expr:
  | l = mul_expr; MUL_KW; r = prefix_expr { prim Surface_ast.Mul [l; r] }
  | l = mul_expr; SLASH;  r = prefix_expr { prim Surface_ast.Div [l; r] }
  | l = mul_expr; MOD_KW; r = prefix_expr { prim Surface_ast.Mod [l; r] }
  | e = prefix_expr                        { e }

prefix_expr:
  | NOT; e = prefix_expr { prim Surface_ast.Not [e] }
  | e = app_expr         { e }

(* Application is left-associative. Fst/Snd take a single atom argument. *)
app_expr:
  | f = app_expr; a = atom { mk_app f a }
  | FST; a = atom          { Surface_ast.Fst a }
  | SND; a = atom          { Surface_ast.Snd a }
  | a = atom               { a }

atom:
  | x = IDENT                          { Surface_ast.Var x }
  | n = INT_LIT                        { Surface_ast.Int_lit n }
  | b = BOOL_LIT                       { Surface_ast.Bool_lit b }
  | s = STRING_LIT                     { Surface_ast.String_lit s }
  | HOLE                               { Surface_ast.Hole }
  | LPAREN; e = expr; RPAREN           { e }
  | LPAREN; a = expr; COMMA; b = expr; RPAREN { mk_pair a b }

(* TYPES. Right-associative arrows; product binds tighter than arrow. *)

ty:
  | t = ty_arrow { t }

ty_arrow:
  | a = ty_product; ARROW; b = ty_arrow { Ty.Arrow (a, b) }
  | t = ty_product                       { t }

ty_product:
  | a = ty_atom; STAR; b = ty_product { Ty.Product (a, b) }
  | t = ty_atom                        { t }

ty_atom:
  | TY_INT                       { Ty.Int }
  | TY_BOOL                      { Ty.Bool }
  | TY_STRING                    { Ty.String }
  | LPAREN; t = ty; RPAREN       { t }
  | HOLE                         { default_ty }
