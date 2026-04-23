%token S_BACKSLASH S_DOT S_COLON S_ARROW
%token S_LPAREN S_RPAREN
%token S_TRUE S_FALSE S_IF S_THEN S_ELSE S_BOOL
%token <string> S_IDENT
%token S_EOF

%start <Stlc_surface_ast.t> main
%start <Ty.t> main_ty

%%

main:
  | e = expr; S_EOF { e }

main_ty:
  | t = ty; S_EOF { t }

expr:
  | S_BACKSLASH; x = S_IDENT; S_COLON; t = ty; S_DOT; body = expr
      { Stlc_surface_ast.Lam (x, t, body) }
  | S_IF; c = expr; S_THEN; t = expr; S_ELSE; e = expr
      { Stlc_surface_ast.If (c, t, e) }
  | a = app_expr { a }

app_expr:
  | f = app_expr; a = atom { Stlc_surface_ast.App (f, a) }
  | a = atom               { a }

atom:
  | x = S_IDENT                  { Stlc_surface_ast.Var x }
  | S_TRUE                       { Stlc_surface_ast.True }
  | S_FALSE                      { Stlc_surface_ast.False }
  | S_LPAREN; e = expr; S_RPAREN { e }

ty:
  | a = ty_atom; S_ARROW; b = ty { Ty.Arrow (a, b) }
  | a = ty_atom                  { a }

ty_atom:
  | S_BOOL                        { Ty.Bool }
  | S_LPAREN; t = ty; S_RPAREN    { t }
