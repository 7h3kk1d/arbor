/* Content-addressed DAG storage. Keyed by Hash.t; values are shallow
   nodes whose children are themselves hashes into this table. */

type t = Hashtbl.t(Hash.t, Definition.t);

let create = (): t => Hashtbl.create(64);

/* Register a node. Idempotent: the hash is content-determined, and
   writing the same node twice is a no-op. Returns the hash. */
let register_node = (store: t, n: Node.t): Hash.t => {
  let h = Node.hash(n);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, n);
  };
  h;
};

let lookup = (store: t, h: Hash.t): option(Definition.t) =>
  Hashtbl.find_opt(store, h);

let has = (store: t, h: Hash.t): bool => Hashtbl.mem(store, h);

let size = (store: t): int => Hashtbl.length(store);

let hashes = (store: t): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], store, []);

let entries = (store: t): list((Hash.t, Definition.t)) =>
  Hashtbl.fold((h, n, acc) => [(h, n), ...acc], store, []);

let resolve_prefix = (store: t, prefix: string): Hash.lookup_result =>
  Hash.lookup_by_prefix(prefix, hashes(store));

/* Ingest: walk a deep Ast.t bottom-up, registering each subterm as a
   shallow node. Returns the hash of the top-level node. Every subterm
   is registered (idempotently), so structural sharing is automatic. */
let rec ingest = (store: t, ast: Ast.t): Hash.t =>
  switch (ast) {
  | Ast.True => register_node(store, Node.True)
  | Ast.False => register_node(store, Node.False)
  | Ast.Zero => register_node(store, Node.Zero)
  | Ast.Succ(a) =>
    let h = ingest(store, a);
    register_node(store, Node.Succ(h));
  | Ast.Pred(a) =>
    let h = ingest(store, a);
    register_node(store, Node.Pred(h));
  | Ast.IsZero(a) =>
    let h = ingest(store, a);
    register_node(store, Node.IsZero(h));
  | Ast.If(c, thn, els) =>
    let ch = ingest(store, c);
    let th = ingest(store, thn);
    let eh = ingest(store, els);
    register_node(store, Node.If(ch, th, eh));
  };

/* Reconstruct: DAG walk from a hash back to a deep Ast.t. Returns None
   if a hash is dangling (shouldn't happen for hashes produced by
   ingest, but we express it honestly). */
let rec reconstruct = (store: t, h: Hash.t): option(Ast.t) =>
  switch (lookup(store, h)) {
  | None => None
  | Some(Node.True) => Some(Ast.True)
  | Some(Node.False) => Some(Ast.False)
  | Some(Node.Zero) => Some(Ast.Zero)
  | Some(Node.Succ(ch)) =>
    Option.map(a => Ast.Succ(a), reconstruct(store, ch))
  | Some(Node.Pred(ch)) =>
    Option.map(a => Ast.Pred(a), reconstruct(store, ch))
  | Some(Node.IsZero(ch)) =>
    Option.map(a => Ast.IsZero(a), reconstruct(store, ch))
  | Some(Node.If(c, t, e)) =>
    switch (reconstruct(store, c), reconstruct(store, t), reconstruct(store, e)) {
    | (Some(c'), Some(t'), Some(e')) => Some(Ast.If(c', t', e'))
    | _ => None
    }
  };
