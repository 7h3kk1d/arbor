%token A_TRUE A_FALSE A_ZERO
%token A_SUCC A_PRED A_ISZERO
%token A_IF A_THEN A_ELSE
%token A_LPAREN A_RPAREN
%token <string> A_IDENT
%token A_EOF

%start <Arith_surface_ast.t> main

%%

main:
  | e = expr; A_EOF { e }

expr:
  | A_IF; c = expr; A_THEN; t = expr; A_ELSE; e = expr { Arith_surface_ast.If (c, t, e) }
  | A_SUCC; a = atom   { Arith_surface_ast.Succ a }
  | A_PRED; a = atom   { Arith_surface_ast.Pred a }
  | A_ISZERO; a = atom { Arith_surface_ast.IsZero a }
  | a = atom           { a }

atom:
  | A_TRUE   { Arith_surface_ast.True }
  | A_FALSE  { Arith_surface_ast.False }
  | A_ZERO   { Arith_surface_ast.Zero }
  | id = A_IDENT { Arith_surface_ast.Name id }
  | A_LPAREN; e = expr; A_RPAREN { e }
