%token BACKSLASH DOT
%token LPAREN RPAREN
%token <string> IDENT
%token EOF

%start <Surface_ast.t> main

%%

main:
  | e = expr; EOF { e }

expr:
  | BACKSLASH; x = IDENT; DOT; body = expr { Surface_ast.Lam (x, body) }
  | a = app_expr { a }

app_expr:
  | f = app_expr; a = atom { Surface_ast.App (f, a) }
  | a = atom               { a }

atom:
  | x = IDENT                { Surface_ast.Var x }
  | LPAREN; e = expr; RPAREN { e }
