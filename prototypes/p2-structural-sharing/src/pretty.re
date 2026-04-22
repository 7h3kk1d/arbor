/* Walks the Store to reconstruct a deep view, then prints with minimal
   parens. Roundtrips through the parser. If a hash is dangling, prints
   a `<missing h:...>` sentinel. */

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
