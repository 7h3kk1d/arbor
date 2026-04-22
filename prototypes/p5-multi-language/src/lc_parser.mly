%token L_BACKSLASH L_DOT
%token L_LPAREN L_RPAREN
%token <string> L_IDENT
%token L_EOF

%start <Lc_surface_ast.t> main

%%

main:
  | e = expr; L_EOF { e }

expr:
  | L_BACKSLASH; x = L_IDENT; L_DOT; body = expr { Lc_surface_ast.Lam (x, body) }
  | a = app_expr { a }

app_expr:
  | f = app_expr; a = atom { Lc_surface_ast.App (f, a) }
  | a = atom               { a }

atom:
  | x = L_IDENT                { Lc_surface_ast.Var x }
  | L_LPAREN; e = expr; L_RPAREN { e }
