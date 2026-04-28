%token BACKSLASH DOT
%token LPAREN RPAREN
%token <string> IDENT
%token HOLE
%token EOF

%start <Surface_ast.t> main

%%

main:
  | e = expr; EOF { e }

(* Partial-lambda productions let the grammar "fall forward" when a
   lambda is truncated at EOF or RPAREN: each reduces to a Hole-bearing
   Lam instead of forcing the recovery driver to bail out. FOLLOW(expr)
   is {EOF, RPAREN}, so these reductions fire only at those terminal
   lookaheads — mid-expression noise (e.g. `\x:. y`) still routes
   through the driver's drop-offender path. A missing binder name is
   synthesized as "?", a string the lexer cannot produce as an IDENT,
   so no user-typed variable reference can ever resolve to it. *)

expr:
  | BACKSLASH; b = binder; DOT; body = expr { Surface_ast.Lam (b, body) }
  | BACKSLASH; b = binder; DOT              { Surface_ast.Lam (b, Surface_ast.Hole) }
  | BACKSLASH; b = binder                   { Surface_ast.Lam (b, Surface_ast.Hole) }
  | BACKSLASH                               { Surface_ast.Lam ("?", Surface_ast.Hole) }
  | a = app_expr { a }

binder:
  | x = IDENT { x }
  | HOLE      { "?" }

app_expr:
  | f = app_expr; a = atom { Surface_ast.App (f, a) }
  | a = atom               { a }

atom:
  | x = IDENT                { Surface_ast.Var x }
  | HOLE                     { Surface_ast.Hole }
  | LPAREN; e = expr; RPAREN { e }
