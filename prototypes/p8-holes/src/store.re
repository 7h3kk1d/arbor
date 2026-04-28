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
   is registered (idempotently), so structural sharing is automatic.
   Alpha-equivalent surface terms share an ingested hash because they
   already share Ast.t representations (de Bruijn). */
let rec ingest = (store: t, ast: Ast.t): Hash.t =>
  switch (ast) {
  | Ast.Var(k) => register_node(store, Node.Var(k))
  | Ast.Hole => register_node(store, Node.Hole)
  | Ast.Lam(body) =>
    let h = ingest(store, body);
    register_node(store, Node.Lam(h));
  | Ast.App(f, a) =>
    let fh = ingest(store, f);
    let ah = ingest(store, a);
    register_node(store, Node.App(fh, ah));
  };

/* Reconstruct: DAG walk from a hash back to a deep Ast.t. Returns None
   if a hash is dangling (shouldn't happen for hashes produced by
   ingest, but we express it honestly). */
let rec reconstruct = (store: t, h: Hash.t): option(Ast.t) =>
  switch (lookup(store, h)) {
  | None => None
  | Some(Node.Var(k)) => Some(Ast.Var(k))
  | Some(Node.Hole) => Some(Ast.Hole)
  | Some(Node.Lam(ch)) =>
    Option.map(a => Ast.Lam(a), reconstruct(store, ch))
  | Some(Node.App(f, a)) =>
    switch (reconstruct(store, f), reconstruct(store, a)) {
    | (Some(f'), Some(a')) => Some(Ast.App(f', a'))
    | _ => None
    }
  };
