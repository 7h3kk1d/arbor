/* Lc pretty-printer. Carried from p4's pretty.re (raw + name-aware
   printers with fresh-name generator); dispatches into this module
   from Pretty only when the target hash is an Lc definition. */

let lookup_lc = (store: Store.t, h: Hash.t): option(Lc_node.t) =>
  switch (Store.lookup(store, h)) {
  | Some(Definition.Lc(n)) => Some(n)
  | _ => None
  };

let rec print_node = (store: Store.t, node: Lc_node.t): string =>
  switch (node) {
  | Lc_node.Var(k) => string_of_int(k)
  | Lc_node.Lam(h) => "\\. " ++ print_hash(store, h)
  | Lc_node.App(f, a) => print_app_left(store, f) ++ " " ++ print_atom(store, a)
  }
and print_hash = (store: Store.t, h: Hash.t): string =>
  switch (lookup_lc(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(n) => print_node(store, n)
  }
and print_atom = (store: Store.t, h: Hash.t): string =>
  switch (lookup_lc(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Lc_node.Var(k)) => string_of_int(k)
  | Some(n) => "(" ++ print_node(store, n) ++ ")"
  }
and print_app_left = (store: Store.t, h: Hash.t): string =>
  switch (lookup_lc(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Lc_node.Var(k)) => string_of_int(k)
  | Some(Lc_node.App(_, _) as n) => print_node(store, n)
  | Some(Lc_node.Lam(_) as n) => "(" ++ print_node(store, n) ++ ")"
  };

let print = (store: Store.t, h: Hash.t): string => print_hash(store, h);

let print_shallow = (node: Lc_node.t): string =>
  switch (node) {
  | Lc_node.Var(k) => "Var " ++ string_of_int(k)
  | Lc_node.Lam(h) => "Lam " ++ Hash.short(h)
  | Lc_node.App(f, a) => "App " ++ Hash.short(f) ++ " " ++ Hash.short(a)
  };

let alphabet = [
  "x", "y", "z", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
  "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w",
];

let fresh_name = (~in_scope: list(string)): string => {
  let taken = s => List.exists(n => n == s, in_scope);
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
  switch (Store.reconstruct_lc(store, h)) {
  | None => false
  | Some(ast) => Lc_ast.is_closed(ast)
  };

let rec surface_of_hash_ctx =
        (
          ~namespace: Namespace.t,
          ~store: Store.t,
          ~in_scope: list(string),
          ~top: bool,
          h: Hash.t,
        )
        : Lc_surface_ast.t => {
  let named = Namespace.names_of(namespace, h);
  let is_collapsible =
    switch (named, lookup_lc(store, h)) {
    | ([_, ..._], Some(Lc_node.Lam(_) | Lc_node.App(_, _))) =>
      is_closed_hash(store, h)
    | _ => false
    };
  if (!top && is_collapsible) {
    Lc_surface_ast.Var(List.hd(named));
  } else {
    switch (lookup_lc(store, h)) {
    | None => Lc_surface_ast.Var("<missing " ++ Hash.short(h) ++ ">")
    | Some(Lc_node.Var(k)) =>
      if (k < List.length(in_scope)) {
        Lc_surface_ast.Var(List.nth(in_scope, k));
      } else {
        Lc_surface_ast.Var("$" ++ string_of_int(k));
      }
    | Some(Lc_node.Lam(body_hash)) =>
      let x = fresh_name(~in_scope);
      let body =
        surface_of_hash_ctx(
          ~namespace,
          ~store,
          ~in_scope=[x, ...in_scope],
          ~top=false,
          body_hash,
        );
      Lc_surface_ast.Lam(x, body);
    | Some(Lc_node.App(f, a)) =>
      Lc_surface_ast.App(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, f),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
      )
    };
  };
};

let surface_of_hash =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Lc_surface_ast.t =>
  surface_of_hash_ctx(~namespace, ~store, ~in_scope=[], ~top=true, h);

let rec print_surface = (s: Lc_surface_ast.t): string =>
  switch (s) {
  | Lc_surface_ast.Var(n) => n
  | Lc_surface_ast.Lam(x, body) => "\\" ++ x ++ ". " ++ print_surface(body)
  | Lc_surface_ast.App(f, a) =>
    print_surface_app_left(f) ++ " " ++ print_surface_atom(a)
  }
and print_surface_app_left = (s: Lc_surface_ast.t): string =>
  switch (s) {
  | Lc_surface_ast.Var(n) => n
  | Lc_surface_ast.App(_, _) => print_surface(s)
  | Lc_surface_ast.Lam(_, _) => "(" ++ print_surface(s) ++ ")"
  }
and print_surface_atom = (s: Lc_surface_ast.t): string =>
  switch (s) {
  | Lc_surface_ast.Var(n) => n
  | _ => "(" ++ print_surface(s) ++ ")"
  };

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  print_surface(surface_of_hash(~namespace, store, h));
