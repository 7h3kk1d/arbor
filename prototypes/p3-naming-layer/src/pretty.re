/* Walks the Store to reconstruct a deep view, then prints with minimal
   parens. Roundtrips through the parser. If a hash is dangling, prints
   a `<missing #...>` sentinel. */

let rec print_node = (store: Store.t, node: Node.t): string =>
  switch (node) {
  | Node.True => "true"
  | Node.False => "false"
  | Node.Zero => "0"
  | Node.Succ(h) => "succ " ++ print_atom(store, h)
  | Node.Pred(h) => "pred " ++ print_atom(store, h)
  | Node.IsZero(h) => "iszero " ++ print_atom(store, h)
  | Node.If(c, t, e) =>
    "if "
    ++ print_hash(store, c)
    ++ " then "
    ++ print_hash(store, t)
    ++ " else "
    ++ print_hash(store, e)
  }
and print_hash = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(n) => print_node(store, n)
  }
and print_atom = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Node.True) => "true"
  | Some(Node.False) => "false"
  | Some(Node.Zero) => "0"
  | Some(n) => "(" ++ print_node(store, n) ++ ")"
  };

let print = (store: Store.t, h: Hash.t): string => print_hash(store, h);

/* Shallow, "as stored" view of a single node. Children are rendered as
   short hash prefixes instead of being reconstructed. Useful for
   walking the DAG by hand. */
let print_shallow = (node: Node.t): string =>
  switch (node) {
  | Node.True => "true"
  | Node.False => "false"
  | Node.Zero => "0"
  | Node.Succ(h) => "Succ " ++ Hash.short(h)
  | Node.Pred(h) => "Pred " ++ Hash.short(h)
  | Node.IsZero(h) => "IsZero " ++ Hash.short(h)
  | Node.If(c, t, e) =>
    "If " ++ Hash.short(c) ++ " " ++ Hash.short(t) ++ " " ++ Hash.short(e)
  };

/* ===== Name-aware printing ===== */

/* Produce a Surface_ast.t view of a stored hash. The top-level hash is
   always unpacked (its constructor is rendered); any child subterm
   whose hash has one or more bound names in the namespace collapses to
   Name(first-alphabetical-name), halting further recursion into that
   child. Unnamed descendants are reconstructed as usual. Missing hashes
   yield Name("<missing #abc123...>") as a last-ditch marker — this
   shouldn't happen for hashes produced by ingest. */

let surface_of_child =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Surface_ast.t => {
  let rec child = (h: Hash.t): Surface_ast.t =>
    switch (Namespace.names_of(namespace, h)) {
    | [n, ..._] => Surface_ast.Name(n)
    | [] =>
      switch (Store.lookup(store, h)) {
      | None => Surface_ast.Name("<missing " ++ Hash.short(h) ++ ">")
      | Some(Node.True) => Surface_ast.True
      | Some(Node.False) => Surface_ast.False
      | Some(Node.Zero) => Surface_ast.Zero
      | Some(Node.Succ(c)) => Surface_ast.Succ(child(c))
      | Some(Node.Pred(c)) => Surface_ast.Pred(child(c))
      | Some(Node.IsZero(c)) => Surface_ast.IsZero(child(c))
      | Some(Node.If(c, t, e)) =>
        Surface_ast.If(child(c), child(t), child(e))
      }
    };
  child(h);
};

let surface_of_hash =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Surface_ast.t =>
  switch (Store.lookup(store, h)) {
  | None => Surface_ast.Name("<missing " ++ Hash.short(h) ++ ">")
  | Some(Node.True) => Surface_ast.True
  | Some(Node.False) => Surface_ast.False
  | Some(Node.Zero) => Surface_ast.Zero
  | Some(Node.Succ(c)) => Surface_ast.Succ(surface_of_child(~namespace, store, c))
  | Some(Node.Pred(c)) => Surface_ast.Pred(surface_of_child(~namespace, store, c))
  | Some(Node.IsZero(c)) =>
    Surface_ast.IsZero(surface_of_child(~namespace, store, c))
  | Some(Node.If(c, t, e)) =>
    Surface_ast.If(
      surface_of_child(~namespace, store, c),
      surface_of_child(~namespace, store, t),
      surface_of_child(~namespace, store, e),
    )
  };

let rec print_surface = (s: Surface_ast.t): string =>
  switch (s) {
  | Surface_ast.True => "true"
  | Surface_ast.False => "false"
  | Surface_ast.Zero => "0"
  | Surface_ast.Name(n) => n
  | Surface_ast.Succ(a) => "succ " ++ print_surface_atom(a)
  | Surface_ast.Pred(a) => "pred " ++ print_surface_atom(a)
  | Surface_ast.IsZero(a) => "iszero " ++ print_surface_atom(a)
  | Surface_ast.If(c, t, e) =>
    "if "
    ++ print_surface(c)
    ++ " then "
    ++ print_surface(t)
    ++ " else "
    ++ print_surface(e)
  }
and print_surface_atom = (s: Surface_ast.t): string =>
  switch (s) {
  | Surface_ast.True => "true"
  | Surface_ast.False => "false"
  | Surface_ast.Zero => "0"
  | Surface_ast.Name(n) => n
  | _ => "(" ++ print_surface(s) ++ ")"
  };

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  print_surface(surface_of_hash(~namespace, store, h));
