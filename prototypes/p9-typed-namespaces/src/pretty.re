/* Rendering: stored hashes and Surface_ast.t back to readable text.

   Two printers:
   - print_surface — prints a Surface_ast.t directly. Used by the UI's
     recovered-AST panel (which feeds it the parser's post-recovery
     output, names and all). Holes render as "?".
   - print_named — reconstructs a Surface_ast.t from a stored hash and
     prints it. Picks fresh display names for each binder, collapses
     non-top child hashes whose hash has a namespace binding to a
     Var(name) leaf (closed-only, like p3). */

/* ===== Fresh-name generation ===== */

let alphabet = [
  "x", "y", "z", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
  "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
];

let fresh_name = (~in_scope: list(string)): string => {
  let taken = s =>
    List.exists(n => n == s, in_scope) || Namespace.is_reserved_segment(s);
  let rec try_round = (round: int) => {
    let candidates =
      List.map(
        base => round == 0 ? base : base ++ string_of_int(round),
        alphabet,
      );
    switch (List.find_opt(c => !taken(c), candidates)) {
    | Some(n) => n
    | None => try_round(round + 1)
    };
  };
  try_round(0);
};

/* ===== Closedness ===== */

let is_closed_hash = (store: Store.t, h: Hash.t): bool =>
  switch (Store.reconstruct(store, h)) {
  | None => false
  | Some(ast) => Ast.is_closed(ast)
  };

/* ===== Hash → Surface_ast.t (name-aware) ===== */

let rec surface_of_hash_ctx =
        (
          ~namespace: Namespace.t,
          ~store: Store.t,
          ~in_scope: list(string),
          ~top: bool,
          h: Hash.t,
        )
        : Surface_ast.t => {
  let collapse =
    !top
    && {
      switch (Namespace.names_of(namespace, h), Store.lookup(store, h)) {
      | ([_, ..._], Some(node)) =>
        switch (node) {
        /* Don't collapse Var, the simple literals, or Hole — their
           context-dependent meaning (or hash-equal-everywhere status for
           Hole) makes a name misleading. */
        | Node.Var(_)
        | Node.Int_lit(_)
        | Node.Bool_lit(_)
        | Node.String_lit(_)
        | Node.Hole => false
        | _ => is_closed_hash(store, h)
        }
      | _ => false
      };
    };
  if (collapse) {
    Surface_ast.Var(List.hd(Namespace.names_of(namespace, h)));
  } else {
    switch (Store.lookup(store, h)) {
    | None => Surface_ast.Var("<missing " ++ Hash.short(h) ++ ">")
    | Some(Node.Var(k)) =>
      if (k < List.length(in_scope)) {
        Surface_ast.Var(List.nth(in_scope, k));
      } else {
        Surface_ast.Var("$" ++ string_of_int(k));
      }
    | Some(Node.Int_lit(n)) => Surface_ast.Int_lit(n)
    | Some(Node.Bool_lit(b)) => Surface_ast.Bool_lit(b)
    | Some(Node.String_lit(s)) => Surface_ast.String_lit(s)
    | Some(Node.Hole) => Surface_ast.Hole
    | Some(Node.Lam(ty, body_hash)) =>
      let x = fresh_name(~in_scope);
      let body =
        surface_of_hash_ctx(
          ~namespace,
          ~store,
          ~in_scope=[x, ...in_scope],
          ~top=false,
          body_hash,
        );
      Surface_ast.Lam(x, ty, body);
    | Some(Node.App(f, a)) =>
      Surface_ast.App(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, f),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
      )
    | Some(Node.Let(rhs, body_hash)) =>
      let rhs' =
        surface_of_hash_ctx(
          ~namespace,
          ~store,
          ~in_scope,
          ~top=false,
          rhs,
        );
      let x = fresh_name(~in_scope);
      let body =
        surface_of_hash_ctx(
          ~namespace,
          ~store,
          ~in_scope=[x, ...in_scope],
          ~top=false,
          body_hash,
        );
      Surface_ast.Let(x, rhs', body);
    | Some(Node.If(c, t, e)) =>
      Surface_ast.If(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, c),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, t),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, e),
      )
    | Some(Node.Pair(a, b)) =>
      Surface_ast.Pair(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, b),
      )
    | Some(Node.Fst(a)) =>
      Surface_ast.Fst(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
      )
    | Some(Node.Snd(a)) =>
      Surface_ast.Snd(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
      )
    | Some(Node.Prim(op, args)) =>
      Surface_ast.Prim(
        op,
        List.map(
          h' =>
            surface_of_hash_ctx(
              ~namespace,
              ~store,
              ~in_scope,
              ~top=false,
              h',
            ),
          args,
        ),
      )
    };
  };
};

let surface_of_hash =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Surface_ast.t =>
  surface_of_hash_ctx(~namespace, ~store, ~in_scope=[], ~top=true, h);

/* ===== Surface_ast.t → string =====

   Operator precedence: lowest to highest:
     0  expr-level (let/if/lam/||)
     1  &&
     2  ==
     3  ++ (right)
     4  + - (left)
     5  mul / mod (left)
     6  prefix not
     7  app
     8  atom
*/

let prec_of_op =
  fun
  | Surface_ast.Or => 0
  | Surface_ast.And => 1
  | Surface_ast.Eq => 2
  | Surface_ast.Concat => 3
  | Surface_ast.Add
  | Surface_ast.Sub => 4
  | Surface_ast.Mul
  | Surface_ast.Div
  | Surface_ast.Mod => 5
  | Surface_ast.Not => 6;

let rec print_prec = (~prec: int, s: Surface_ast.t): string => {
  let wrap = (this_prec, body) =>
    if (this_prec < prec) {
      "(" ++ body ++ ")";
    } else {
      body;
    };
  switch (s) {
  | Surface_ast.Var(n) => n
  | Surface_ast.Int_lit(n) => string_of_int(n)
  | Surface_ast.Bool_lit(true) => "true"
  | Surface_ast.Bool_lit(false) => "false"
  | Surface_ast.String_lit(s) => "\"" ++ String.escaped(s) ++ "\""
  | Surface_ast.Hole => "?"
  | Surface_ast.Lam(x, ty, body) =>
    wrap(
      0,
      "\\"
      ++ x
      ++ ": "
      ++ Ty.print(ty)
      ++ ". "
      ++ print_prec(~prec=0, body),
    )
  | Surface_ast.Let(x, rhs, body) =>
    wrap(
      0,
      "let "
      ++ x
      ++ " = "
      ++ print_prec(~prec=0, rhs)
      ++ " in "
      ++ print_prec(~prec=0, body),
    )
  | Surface_ast.If(c, t, e) =>
    wrap(
      0,
      "if "
      ++ print_prec(~prec=0, c)
      ++ " then "
      ++ print_prec(~prec=0, t)
      ++ " else "
      ++ print_prec(~prec=0, e),
    )
  | Surface_ast.Pair(a, b) =>
    "(" ++ print_prec(~prec=0, a) ++ ", " ++ print_prec(~prec=0, b) ++ ")"
  | Surface_ast.Fst(a) =>
    wrap(7, "fst " ++ print_prec(~prec=8, a))
  | Surface_ast.Snd(a) =>
    wrap(7, "snd " ++ print_prec(~prec=8, a))
  | Surface_ast.App(f, a) =>
    wrap(7, print_prec(~prec=7, f) ++ " " ++ print_prec(~prec=8, a))
  | Surface_ast.Prim(Surface_ast.Not, [e]) =>
    wrap(6, "not " ++ print_prec(~prec=7, e))
  | Surface_ast.Prim(op, [a, b]) =>
    let op_prec = prec_of_op(op);
    let s = Surface_ast.prim_op_to_string(op);
    wrap(
      op_prec,
      print_prec(~prec=op_prec, a)
      ++ " "
      ++ s
      ++ " "
      ++ print_prec(~prec=op_prec + 1, b),
    );
  | Surface_ast.Prim(op, args) =>
    /* Defensive: unexpected arity. Render as a function call. */
    let arg_str =
      List.map(a => print_prec(~prec=8, a), args) |> String.concat(" ");
    wrap(7, Surface_ast.prim_op_to_string(op) ++ " " ++ arg_str);
  };
};

let print_surface = (s: Surface_ast.t): string => print_prec(~prec=0, s);

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  print_surface(surface_of_hash(~namespace, store, h));
