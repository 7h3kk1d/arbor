/* Stlc pretty-printer. Same shape as lc_pretty (raw + name-aware with
   fresh-name generator), extended for True/False/If and type
   annotations on Lam. Closed, namespace-bound subterms collapse to a
   Var leaf in the name-aware printer, just as in lc. */

let lookup_stlc = (store: Store.t, h: Hash.t): option(Stlc_node.t) =>
  switch (Store.lookup(store, h)) {
  | Some(Definition.Stlc(n)) => Some(n)
  | _ => None
  };

let rec print_node = (store: Store.t, node: Stlc_node.t): string =>
  switch (node) {
  | Stlc_node.Var(k) => string_of_int(k)
  | Stlc_node.Lam(ty, h) =>
    "\\:" ++ Ty.print(ty) ++ ". " ++ print_hash(store, h)
  | Stlc_node.App(f, a) =>
    print_app_left(store, f) ++ " " ++ print_atom(store, a)
  | Stlc_node.True => "true"
  | Stlc_node.False => "false"
  | Stlc_node.If(c, t, e) =>
    "if "
    ++ print_hash(store, c)
    ++ " then "
    ++ print_hash(store, t)
    ++ " else "
    ++ print_hash(store, e)
  }
and print_hash = (store: Store.t, h: Hash.t): string =>
  switch (lookup_stlc(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(n) => print_node(store, n)
  }
and print_atom = (store: Store.t, h: Hash.t): string =>
  switch (lookup_stlc(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Stlc_node.Var(k)) => string_of_int(k)
  | Some(Stlc_node.True) => "true"
  | Some(Stlc_node.False) => "false"
  | Some(n) => "(" ++ print_node(store, n) ++ ")"
  }
and print_app_left = (store: Store.t, h: Hash.t): string =>
  switch (lookup_stlc(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Stlc_node.Var(k)) => string_of_int(k)
  | Some(Stlc_node.True) => "true"
  | Some(Stlc_node.False) => "false"
  | Some(Stlc_node.App(_, _) as n) => print_node(store, n)
  | Some(n) => "(" ++ print_node(store, n) ++ ")"
  };

let print = (store: Store.t, h: Hash.t): string => print_hash(store, h);

let print_shallow = (node: Stlc_node.t): string =>
  switch (node) {
  | Stlc_node.Var(k) => "Var " ++ string_of_int(k)
  | Stlc_node.Lam(ty, h) =>
    "Lam :" ++ Ty.print(ty) ++ " " ++ Hash.short(h)
  | Stlc_node.App(f, a) =>
    "App " ++ Hash.short(f) ++ " " ++ Hash.short(a)
  | Stlc_node.True => "True"
  | Stlc_node.False => "False"
  | Stlc_node.If(c, t, e) =>
    "If "
    ++ Hash.short(c)
    ++ " "
    ++ Hash.short(t)
    ++ " "
    ++ Hash.short(e)
  };

let alphabet = [
  "x", "y", "z", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
  "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
];

/* Reserved tokens in the stlc lexer — avoid shadowing them with fresh
   binder names or the round-trip through the parser would misread. */
let reserved = ["true", "false", "if", "then", "else", "Bool"];

let fresh_name = (~in_scope: list(string)): string => {
  let taken = s =>
    List.exists(n => n == s, in_scope) || List.exists(n => n == s, reserved);
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

let is_closed_hash = (store: Store.t, h: Hash.t): bool =>
  switch (Store.reconstruct_stlc(store, h)) {
  | None => false
  | Some(ast) => Stlc_ast.is_closed(ast)
  };

let rec surface_of_hash_ctx =
        (
          ~namespace: Namespace.t,
          ~store: Store.t,
          ~in_scope: list(string),
          ~top: bool,
          h: Hash.t,
        )
        : Stlc_surface_ast.t => {
  let named = Namespace.names_of(namespace, h);
  let is_collapsible =
    switch (named, lookup_stlc(store, h)) {
    | (
        [_, ..._],
        Some(
          Stlc_node.Lam(_, _) | Stlc_node.App(_, _) | Stlc_node.If(_, _, _),
        ),
      ) =>
      is_closed_hash(store, h)
    | _ => false
    };
  if (!top && is_collapsible) {
    Stlc_surface_ast.Var(List.hd(named));
  } else {
    switch (lookup_stlc(store, h)) {
    | None => Stlc_surface_ast.Var("<missing " ++ Hash.short(h) ++ ">")
    | Some(Stlc_node.Var(k)) =>
      if (k < List.length(in_scope)) {
        Stlc_surface_ast.Var(List.nth(in_scope, k));
      } else {
        Stlc_surface_ast.Var("$" ++ string_of_int(k));
      }
    | Some(Stlc_node.True) => Stlc_surface_ast.True
    | Some(Stlc_node.False) => Stlc_surface_ast.False
    | Some(Stlc_node.Lam(ty, body_hash)) =>
      let x = fresh_name(~in_scope);
      let body =
        surface_of_hash_ctx(
          ~namespace,
          ~store,
          ~in_scope=[x, ...in_scope],
          ~top=false,
          body_hash,
        );
      Stlc_surface_ast.Lam(x, ty, body);
    | Some(Stlc_node.App(f, a)) =>
      Stlc_surface_ast.App(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, f),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
      )
    | Some(Stlc_node.If(c, t, e)) =>
      Stlc_surface_ast.If(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, c),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, t),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, e),
      )
    };
  };
};

let surface_of_hash =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Stlc_surface_ast.t =>
  surface_of_hash_ctx(~namespace, ~store, ~in_scope=[], ~top=true, h);

let rec print_surface = (s: Stlc_surface_ast.t): string =>
  switch (s) {
  | Stlc_surface_ast.Var(n) => n
  | Stlc_surface_ast.True => "true"
  | Stlc_surface_ast.False => "false"
  | Stlc_surface_ast.Lam(x, ty, body) =>
    "\\" ++ x ++ ":" ++ Ty.print(ty) ++ ". " ++ print_surface(body)
  | Stlc_surface_ast.App(f, a) =>
    print_surface_app_left(f) ++ " " ++ print_surface_atom(a)
  | Stlc_surface_ast.If(c, t, e) =>
    "if "
    ++ print_surface(c)
    ++ " then "
    ++ print_surface(t)
    ++ " else "
    ++ print_surface(e)
  }
and print_surface_app_left = (s: Stlc_surface_ast.t): string =>
  switch (s) {
  | Stlc_surface_ast.Var(n) => n
  | Stlc_surface_ast.True => "true"
  | Stlc_surface_ast.False => "false"
  | Stlc_surface_ast.App(_, _) => print_surface(s)
  | _ => "(" ++ print_surface(s) ++ ")"
  }
and print_surface_atom = (s: Stlc_surface_ast.t): string =>
  switch (s) {
  | Stlc_surface_ast.Var(n) => n
  | Stlc_surface_ast.True => "true"
  | Stlc_surface_ast.False => "false"
  | _ => "(" ++ print_surface(s) ++ ")"
  };

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  print_surface(surface_of_hash(~namespace, store, h));
