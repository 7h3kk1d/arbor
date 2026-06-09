%{
  let mk_lam x ty body = Surface.Lam (x, ty, body)
  let mk_let x rhs body = Surface.Let (x, rhs, body)
  let prim op args = Surface.Prim (op, args)
%}

%token LET IN IF THEN ELSE FST SND MUL_KW FORALL AS
%token SIG STRUCT TYPE_KW OPEN
%token NIL CONS FOLD LIST
%token TY_INT TY_BOOL
%token EQEQ ARROW TYLAM COLONGT
%token BACKSLASH DOT LPAREN RPAREN COMMA COLON EQ LBRACK RBRACK
%token LBRACKBAR BARRBRACK LBRACE RBRACE HASH
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
  | TYLAM; x = IDENT; DOT; body = expr { Surface.TyLam (x, body) }
  | OPEN; e = expr; AS; m = IDENT; IN; body = expr { Surface.Open_local (e, m, body) }
  | e = asc_expr { e }

asc_expr:
  | e = eq_expr; COLONGT; t = ty { Surface.Ascribe (e, t) }
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
  | f = app_expr; LBRACK; t = ty; RBRACK { Surface.TyApp (f, t) }
  | FST; a = atom          { Surface.Fst a }
  | SND; a = atom          { Surface.Snd a }
  | CONS; h = atom; t = atom { Surface.Cons (h, t) }
  | FOLD; l = atom; z = atom; f = atom { Surface.Fold (l, z, f) }
  | a = atom               { a }

atom:
  | a = atom; HASH; f = IDENT                  { Surface.Project (a, f) }
  | x = IDENT                                  { Surface.Var x }
  | n = INT_LIT                                { Surface.Lit n }
  | b = BOOL_LIT                               { Surface.Bool b }
  | NIL; LBRACK; t = ty; RBRACK                { Surface.Nil t }
  | LBRACKBAR; es = expr_list; BARRBRACK       { Surface.ListLit es }
  | LBRACE; fs = lit_field_list; RBRACE        { Surface.Record_lit fs }
  | STRUCT; LBRACE; ms = struct_item_list; RBRACE { Surface.Struct ms }
  | LPAREN; e = expr; RPAREN                   { e }
  | LPAREN; a = expr; COMMA; b = expr; RPAREN  { Surface.Pair (a, b) }

struct_item:
  | TYPE_KW; n = IDENT; EQ; t = ty { Surface.Sitype (n, t) }
  | n = IDENT; EQ; e = expr        { Surface.Sival (n, e) }

struct_item_list:
  | m = struct_item                               { [m] }
  | m = struct_item; COMMA; rest = struct_item_list { m :: rest }

expr_list:
  | e = expr                          { [e] }
  | e = expr; COMMA; rest = expr_list { e :: rest }

lit_field:
  | n = IDENT; EQ; e = expr { (n, e) }

lit_field_list:
  | f = lit_field                          { [f] }
  | f = lit_field; COMMA; rest = lit_field_list { f :: rest }

ty:
  | FORALL; x = IDENT; DOT; t = ty { Surface_ty.Forall (x, t) }
  | t = ty_arrow { t }

ty_arrow:
  | a = ty_product; ARROW; b = ty { Surface_ty.Arrow (a, b) }
  | t = ty_product                { t }

ty_product:
  | a = ty_atom; STAR; b = ty_product { Surface_ty.Product (a, b) }
  | t = ty_atom                        { t }

ty_atom:
  | TY_INT                  { Surface_ty.Int }
  | TY_BOOL                 { Surface_ty.Bool }
  | LIST; t = ty_atom       { Surface_ty.List t }
  | n = IDENT               { Surface_ty.Named n }
  | LBRACE; fs = ty_field_list; RBRACE { Surface_ty.Record fs }
  | SIG; LBRACE; items = sig_item_list; RBRACE { Surface_ty.Sig items }
  | LPAREN; t = ty; RPAREN  { t }

sig_item:
  | TYPE_KW; n = IDENT             { Surface_ty.Stype n }
  | TYPE_KW; n = IDENT; EQ; t = ty { Surface_ty.Stype_eq (n, t) }
  | n = IDENT; COLON; t = ty       { Surface_ty.Sfield (n, t) }

sig_item_list:
  | i = sig_item                              { [i] }
  | i = sig_item; COMMA; rest = sig_item_list { i :: rest }

ty_field:
  | n = IDENT; COLON; t = ty { (n, t) }

ty_field_list:
  | f = ty_field                          { [f] }
  | f = ty_field; COMMA; rest = ty_field_list { f :: rest }
