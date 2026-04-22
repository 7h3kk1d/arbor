%token TRUE FALSE ZERO
%token SUCC PRED ISZERO
%token IF THEN ELSE
%token LPAREN RPAREN
%token EOF

%start <Ast.t> main

%%

main:
  | e = expr; EOF { e }

expr:
  | IF; c = expr; THEN; t = expr; ELSE; e = expr { Ast.If (c, t, e) }
  | SUCC; a = atom   { Ast.Succ a }
  | PRED; a = atom   { Ast.Pred a }
  | ISZERO; a = atom { Ast.IsZero a }
  | a = atom         { a }

atom:
  | TRUE   { Ast.True }
  | FALSE  { Ast.False }
  | ZERO   { Ast.Zero }
  | LPAREN; e = expr; RPAREN { e }
