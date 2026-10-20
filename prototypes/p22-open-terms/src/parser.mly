%{
  open Surface
%}

%token <string> IDENT
%token <string> TEXTLIT
%token <int>    NATLIT
%token NAT_TY TEXT_TY
%token LAMBDA ARROW DOT EQUALS COLON LPAREN RPAREN GT
%token EOF

/* One stanza. A scratch buffer is a sequence of these, split by layout in
   Parse -- top-level stanzas start at column 0 and continuation lines are
   indented, which is Unison's rule and is what keeps the grammar itself
   newline-insensitive. */
%start <Surface.stanza> stanza_only
/* A bare expression, for the detail pane and for tests. */
%start <Term.Source.t> expr_only

%%

stanza_only:
  | n = IDENT COLON t = ty    EOF { Signature (n, t) }
  | n = IDENT EQUALS e = expr EOF { Binding (n, e) }
  | GT e = expr               EOF { Watch e }

expr_only:
  | e = expr EOF { e }

/* Types: -> is right-associative. */
ty:
  | a = ty_atom ARROW b = ty { Ty.Arrow (a, b) }
  | a = ty_atom              { a }

ty_atom:
  | NAT_TY               { Ty.Nat }
  | TEXT_TY              { Ty.Text }
  | LPAREN t = ty RPAREN { t }

expr:
  | LAMBDA x = IDENT DOT e = expr { Term.Source.Lam (x, e) }
  | e = ann_expr                  { e }

ann_expr:
  | e = app_expr COLON t = ty { Term.Source.Ann (e, t) }
  | e = app_expr              { e }

app_expr:
  | f = app_expr a = atom { Term.Source.App (f, a) }
  | a = atom              { a }

atom:
  | x = IDENT              { Term.Source.Var x }
  | n = NATLIT             { Term.Source.Nat n }
  | s = TEXTLIT            { Term.Source.Text s }
  | LPAREN e = expr RPAREN { e }
