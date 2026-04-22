/* Arith pretty-printer. Carried from p3's pretty.re; dispatches into
   this module from Pretty only when the target hash is an Arith
   definition. */

let lookup_arith = (store: Store.t, h: Hash.t): option(Arith_node.t) =>
  switch (Store.lookup(store, h)) {
  | Some(Definition.Arith(n)) => Some(n)
  | _ => None
  };

let rec print_node = (store: Store.t, node: Arith_node.t): string =>
  switch (node) {
  | Arith_node.True => "true"
  | Arith_node.False => "false"
  | Arith_node.Zero => "0"
  | Arith_node.Succ(h) => "succ " ++ print_atom(store, h)
  | Arith_node.Pred(h) => "pred " ++ print_atom(store, h)
  | Arith_node.IsZero(h) => "iszero " ++ print_atom(store, h)
  | Arith_node.If(c, t, e) =>
    "if "
    ++ print_hash(store, c)
    ++ " then "
    ++ print_hash(store, t)
    ++ " else "
    ++ print_hash(store, e)
  }
and print_hash = (store: Store.t, h: Hash.t): string =>
  switch (lookup_arith(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(n) => print_node(store, n)
  }
and print_atom = (store: Store.t, h: Hash.t): string =>
  switch (lookup_arith(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Arith_node.True) => "true"
  | Some(Arith_node.False) => "false"
  | Some(Arith_node.Zero) => "0"
  | Some(n) => "(" ++ print_node(store, n) ++ ")"
  };

let print = (store: Store.t, h: Hash.t): string => print_hash(store, h);

let print_shallow = (node: Arith_node.t): string =>
  switch (node) {
  | Arith_node.True => "true"
  | Arith_node.False => "false"
  | Arith_node.Zero => "0"
  | Arith_node.Succ(h) => "Succ " ++ Hash.short(h)
  | Arith_node.Pred(h) => "Pred " ++ Hash.short(h)
  | Arith_node.IsZero(h) => "IsZero " ++ Hash.short(h)
  | Arith_node.If(c, t, e) =>
    "If " ++ Hash.short(c) ++ " " ++ Hash.short(t) ++ " " ++ Hash.short(e)
  };

/* ===== Name-aware printing ===== */

let surface_of_child =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Arith_surface_ast.t => {
  let rec child = (h: Hash.t): Arith_surface_ast.t =>
    switch (Namespace.names_of(namespace, h)) {
    | [n, ..._] => Arith_surface_ast.Name(n)
    | [] =>
      switch (lookup_arith(store, h)) {
      | None => Arith_surface_ast.Name("<missing " ++ Hash.short(h) ++ ">")
      | Some(Arith_node.True) => Arith_surface_ast.True
      | Some(Arith_node.False) => Arith_surface_ast.False
      | Some(Arith_node.Zero) => Arith_surface_ast.Zero
      | Some(Arith_node.Succ(c)) => Arith_surface_ast.Succ(child(c))
      | Some(Arith_node.Pred(c)) => Arith_surface_ast.Pred(child(c))
      | Some(Arith_node.IsZero(c)) => Arith_surface_ast.IsZero(child(c))
      | Some(Arith_node.If(c, t, e)) =>
        Arith_surface_ast.If(child(c), child(t), child(e))
      }
    };
  child(h);
};

let surface_of_hash =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Arith_surface_ast.t =>
  switch (lookup_arith(store, h)) {
  | None => Arith_surface_ast.Name("<missing " ++ Hash.short(h) ++ ">")
  | Some(Arith_node.True) => Arith_surface_ast.True
  | Some(Arith_node.False) => Arith_surface_ast.False
  | Some(Arith_node.Zero) => Arith_surface_ast.Zero
  | Some(Arith_node.Succ(c)) =>
    Arith_surface_ast.Succ(surface_of_child(~namespace, store, c))
  | Some(Arith_node.Pred(c)) =>
    Arith_surface_ast.Pred(surface_of_child(~namespace, store, c))
  | Some(Arith_node.IsZero(c)) =>
    Arith_surface_ast.IsZero(surface_of_child(~namespace, store, c))
  | Some(Arith_node.If(c, t, e)) =>
    Arith_surface_ast.If(
      surface_of_child(~namespace, store, c),
      surface_of_child(~namespace, store, t),
      surface_of_child(~namespace, store, e),
    )
  };

let rec print_surface = (s: Arith_surface_ast.t): string =>
  switch (s) {
  | Arith_surface_ast.True => "true"
  | Arith_surface_ast.False => "false"
  | Arith_surface_ast.Zero => "0"
  | Arith_surface_ast.Name(n) => n
  | Arith_surface_ast.Succ(a) => "succ " ++ print_surface_atom(a)
  | Arith_surface_ast.Pred(a) => "pred " ++ print_surface_atom(a)
  | Arith_surface_ast.IsZero(a) => "iszero " ++ print_surface_atom(a)
  | Arith_surface_ast.If(c, t, e) =>
    "if "
    ++ print_surface(c)
    ++ " then "
    ++ print_surface(t)
    ++ " else "
    ++ print_surface(e)
  }
and print_surface_atom = (s: Arith_surface_ast.t): string =>
  switch (s) {
  | Arith_surface_ast.True => "true"
  | Arith_surface_ast.False => "false"
  | Arith_surface_ast.Zero => "0"
  | Arith_surface_ast.Name(n) => n
  | _ => "(" ++ print_surface(s) ++ ")"
  };

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  print_surface(surface_of_hash(~namespace, store, h));
