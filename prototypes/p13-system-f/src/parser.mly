%{
  let mk_lam x ty body = Surface.Lam (x, ty, body)
  let mk_let x rhs body = Surface.Let (x, rhs, body)
  let prim op args = Surface.Prim (op, args)
%}

%token LET IN IF THEN ELSE FST SND MUL_KW
%token TY_INT TY_BOOL
%token EQEQ ARROW
%token BACKSLASH DOT LPAREN RPAREN COMMA COLON EQ
%token PLUS MINUS STAR
%token <string> IDENT
%token <int> INT_LIT
%token <bool> BOOL_LIT
%token EOF

%start <Surface.t> main
%start <Surface_ty.t> main_ty

%%

main:
  | e = expr; EOF { e }

main_ty:
  | t = ty; EOF { t }

expr:
  | LET; x = IDENT; EQ; rhs = expr; IN; body = expr { mk_let x rhs body }
  | IF; c = expr; THEN; t = expr; ELSE; e = expr    { Surface.If (c, t, e) }
  | BACKSLASH; x = IDENT; COLON; t = ty; DOT; body = expr { mk_lam x t body }
  | e = eq_expr { e }

eq_expr:
  | l = add_expr; EQEQ; r = add_expr { prim Surface.Eq [l; r] }
  | e = add_expr                      { e }

add_expr:
  | l = add_expr; PLUS;  r = mul_expr { prim Surface.Add [l; r] }
  | l = add_expr; MINUS; r = mul_expr { prim Surface.Sub [l; r] }
  | e = mul_expr                       { e }

mul_expr:
  | l = mul_expr; MUL_KW; r = app_expr { prim Surface.Mul [l; r] }
  | e = app_expr                        { e }

app_expr:
  | f = app_expr; a = atom { Surface.App (f, a) }
  | FST; a = atom          { Surface.Fst a }
  | SND; a = atom          { Surface.Snd a }
  | a = atom               { a }

atom:
  | x = IDENT                                  { Surface.Var x }
  | n = INT_LIT                                { Surface.Lit n }
  | b = BOOL_LIT                               { Surface.Bool b }
  | LPAREN; e = expr; RPAREN                   { e }
  | LPAREN; a = expr; COMMA; b = expr; RPAREN  { Surface.Pair (a, b) }

ty:
  | t = ty_arrow { t }

ty_arrow:
  | a = ty_product; ARROW; b = ty_arrow { Surface_ty.Arrow (a, b) }
  | t = ty_product                       { t }

ty_product:
  | a = ty_atom; STAR; b = ty_product { Surface_ty.Product (a, b) }
  | t = ty_atom                        { t }

ty_atom:
  | TY_INT                  { Surface_ty.Int }
  | TY_BOOL                 { Surface_ty.Bool }
  | n = IDENT               { Surface_ty.Named n }
  | LPAREN; t = ty; RPAREN  { t }
