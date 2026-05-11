%{
  let mk_lam x ty body = Surface_ast.Lam (x, ty, body)
  let mk_let x rhs body = Surface_ast.Let (x, rhs, body)
  let mk_if c t e = Surface_ast.If (c, t, e)
  let mk_pair a b = Surface_ast.Pair (a, b)
  let mk_app f a = Surface_ast.App (f, a)
  let prim op args = Surface_ast.Prim (op, args)
  (* Default annotation in partial-form lambdas. Hole-in-type-position
     resolves to Ty.Int at the Resolver, matching pre-named-types
     behavior. *)
  let default_ty = Surface_ty.Hole
%}

%token LET IN IF THEN ELSE FST SND NOT MUL_KW MOD_KW WITH
%token TY_INT TY_BOOL TY_STRING TY_LIST
%token EQEQ CONCAT_OP ARROW FATARROW ANDAND OROR
%token BACKSLASH DOT LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE COMMA COLON EQ
%token PLUS MINUS STAR SLASH
%token <string> IDENT
%token <int> INT_LIT
%token <bool> BOOL_LIT
%token <string> STRING_LIT
%token HOLE
%token EOF

%start <Surface_ast.t> main
%start <Surface_ty.t> main_ty

%%

main:
  | e = expr; EOF { e }

(* Type-only entry — used by the dedicated type editor pane. The same
   `ty` non-terminal as inside lambda annotations, just promoted to a
   start symbol so the recovery driver can drive it independently. *)
main_ty:
  | t = ty; EOF { t }

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
  | BACKSLASH; x = binder; COLON; ty = ty; FATARROW; body = expr
      { mk_lam x ty body }
  | BACKSLASH; x = binder; COLON; ty = ty; FATARROW
      { mk_lam x ty Surface_ast.Hole }
  | BACKSLASH; x = binder; COLON; ty = ty
      { mk_lam x ty Surface_ast.Hole }
  | BACKSLASH; x = binder; COLON
      { mk_lam x default_ty Surface_ast.Hole }
  | BACKSLASH; x = binder; FATARROW
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

(* Application is left-associative. Fst/Snd take a single proj_expr
   argument. Projection (`p.x`, `p.0`) binds tighter than application:
   `f p.x` parses as `f (p.x)`. *)
app_expr:
  | f = app_expr; a = proj_expr { mk_app f a }
  | FST; a = proj_expr          { Surface_ast.Fst a }
  | SND; a = proj_expr          { Surface_ast.Snd a }
  | a = proj_expr               { a }

proj_expr:
  | e = proj_expr; DOT; n = IDENT
      { Surface_ast.Project_field (e, n) }
  | e = proj_expr; DOT; i = INT_LIT
      { Surface_ast.Project_index (e, i) }
  | a = atom { a }

atom:
  | x = IDENT                          { Surface_ast.Var x }
  | n = INT_LIT                        { Surface_ast.Int_lit n }
  | b = BOOL_LIT                       { Surface_ast.Bool_lit b }
  | s = STRING_LIT                     { Surface_ast.String_lit s }
  | HOLE                               { Surface_ast.Hole }
  | LPAREN; e = expr; tail = paren_tail; RPAREN
      { match tail with
        | [] -> e                            (* (e) — grouping *)
        | [b] -> mk_pair e b                 (* (a, b) — pair *)
        | more -> Surface_ast.Tuple (e :: more) (* (a, b, c, …) — tuple *) }
  | LBRACKET; items = list_items; RBRACKET
      { Surface_ast.List_lit items }
  | LBRACE; fields = record_body; RBRACE
      { fields }

(* Trailing items in a parenthesized expression: zero or more `,` expr. *)
paren_tail:
  | (* empty *) { [] }
  | COMMA; e = expr; rest = paren_tail { e :: rest }

(* Comma-separated expression list inside `[ ... ]`. Empty allowed. *)
list_items:
  | (* empty *) { [] }
  | e = expr { [e] }
  | e = expr; COMMA; rest = list_items { e :: rest }

(* Record body. A `with` after the first expression turns the body
   into a Record_update; otherwise it's a Record_lit. The "first
   expression" for an update is the target record value. *)
record_body:
  | fields = record_fields
      { Surface_ast.Record_lit fields }
  | target = expr; WITH; fields = record_fields
      { Surface_ast.Record_update (target, fields) }

(* name = expr, name = expr, … — one or more bindings. *)
record_fields:
  | x = IDENT; EQ; v = expr
      { [(x, v)] }
  | x = IDENT; EQ; v = expr; COMMA; rest = record_fields
      { (x, v) :: rest }

(* TYPES. Right-associative arrows; product binds tighter than arrow.
   `IDENT` in atom position is a named type — resolved to a Ty.t via
   the namespace at Resolver time. Hole-in-type-position is a real
   Surface_ty.Hole; the Resolver decides what concrete Ty.t to
   substitute. *)

ty:
  | t = ty_arrow { t }

ty_arrow:
  | a = ty_product; ARROW; b = ty_arrow { Surface_ty.Arrow (a, b) }
  | t = ty_product                       { t }

ty_product:
  | a = ty_atom; STAR; b = ty_product { Surface_ty.Product (a, b) }
  | t = ty_atom                        { t }

ty_atom:
  | TY_INT                       { Surface_ty.Int }
  | TY_BOOL                      { Surface_ty.Bool }
  | TY_STRING                    { Surface_ty.String }
  | TY_LIST; t = ty_atom         { Surface_ty.List t }
  | n = ty_dotted_name           { Surface_ty.Named n }
  | LPAREN; t = ty; tail = ty_paren_tail; RPAREN
      { match tail with
        | [] -> t                                (* (T) — grouping *)
        | more -> Surface_ty.Tuple (t :: more)   (* (T, U, …) — tuple type *) }
  | LBRACE; fields = record_ty_fields; RBRACE
      { Surface_ty.Record_decl fields }
  | HOLE                         { Surface_ty.Hole }

ty_paren_tail:
  | (* empty *) { [] }
  | COMMA; t = ty; rest = ty_paren_tail { t :: rest }

(* In type position there's no field projection, so we re-fuse
   dotted names (lowercase or mixed) into a single Named string. *)
ty_dotted_name:
  | n = IDENT { n }
  | n = IDENT; DOT; rest = ty_dotted_name { n ^ "." ^ rest }

record_ty_fields:
  | x = IDENT; COLON; t = ty
      { [(x, t)] }
  | x = IDENT; COLON; t = ty; COMMA; rest = record_ty_fields
      { (x, t) :: rest }
