%token TRUE FALSE ZERO
%token SUCC PRED ISZERO
%token IF THEN ELSE
%token LPAREN RPAREN
%token <string> IDENT
%token EOF

%start <Surface_ast.t> main

%%

main:
  | e = expr; EOF { e }

expr:
  | IF; c = expr; THEN; t = expr; ELSE; e = expr { Surface_ast.If (c, t, e) }
  | SUCC; a = atom   { Surface_ast.Succ a }
  | PRED; a = atom   { Surface_ast.Pred a }
  | ISZERO; a = atom { Surface_ast.IsZero a }
  | a = atom         { a }

atom:
  | TRUE   { Surface_ast.True }
  | FALSE  { Surface_ast.False }
  | ZERO   { Surface_ast.Zero }
  | id = IDENT { Surface_ast.Name id }
  | LPAREN; e = expr; RPAREN { e }
