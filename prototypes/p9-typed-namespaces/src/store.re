/* Content-addressed DAG storage. Single language (p9), so the store is
   a Hashtbl from Hash.t to Definition.t (= Node.t). Ingest walks an
   Ast.t bottom-up; reconstruct walks a hash back into an Ast.t. */

type t = Hashtbl.t(Hash.t, Definition.t);

let create = (): t => Hashtbl.create(64);

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

let rec ingest = (store: t, ast: Ast.t): Hash.t =>
  switch (ast) {
  | Ast.Var(k) => register_node(store, Node.Var(k))
  | Ast.Int_lit(n) => register_node(store, Node.Int_lit(n))
  | Ast.Bool_lit(b) => register_node(store, Node.Bool_lit(b))
  | Ast.String_lit(s) => register_node(store, Node.String_lit(s))
  | Ast.Hole => register_node(store, Node.Hole)
  | Ast.Lam(ty, body) =>
    let h = ingest(store, body);
    register_node(store, Node.Lam(ty, h));
  | Ast.App(f, a) =>
    let fh = ingest(store, f);
    let ah = ingest(store, a);
    register_node(store, Node.App(fh, ah));
  | Ast.Let(rhs, body) =>
    let rh = ingest(store, rhs);
    let bh = ingest(store, body);
    register_node(store, Node.Let(rh, bh));
  | Ast.If(c, t, e) =>
    let ch = ingest(store, c);
    let th = ingest(store, t);
    let eh = ingest(store, e);
    register_node(store, Node.If(ch, th, eh));
  | Ast.Pair(a, b) =>
    let ah = ingest(store, a);
    let bh = ingest(store, b);
    register_node(store, Node.Pair(ah, bh));
  | Ast.Fst(a) =>
    let ah = ingest(store, a);
    register_node(store, Node.Fst(ah));
  | Ast.Snd(a) =>
    let ah = ingest(store, a);
    register_node(store, Node.Snd(ah));
  | Ast.Prim(op, args) =>
    let arg_hashes = List.map(t => ingest(store, t), args);
    register_node(store, Node.Prim(op, arg_hashes));
  };

let rec reconstruct = (store: t, h: Hash.t): option(Ast.t) =>
  switch (lookup(store, h)) {
  | None => None
  | Some(Node.Var(k)) => Some(Ast.Var(k))
  | Some(Node.Int_lit(n)) => Some(Ast.Int_lit(n))
  | Some(Node.Bool_lit(b)) => Some(Ast.Bool_lit(b))
  | Some(Node.String_lit(s)) => Some(Ast.String_lit(s))
  | Some(Node.Hole) => Some(Ast.Hole)
  | Some(Node.Lam(ty, ch)) =>
    Option.map(a => Ast.Lam(ty, a), reconstruct(store, ch))
  | Some(Node.App(f, a)) =>
    switch (reconstruct(store, f), reconstruct(store, a)) {
    | (Some(f'), Some(a')) => Some(Ast.App(f', a'))
    | _ => None
    }
  | Some(Node.Let(rhs, body)) =>
    switch (reconstruct(store, rhs), reconstruct(store, body)) {
    | (Some(r), Some(b)) => Some(Ast.Let(r, b))
    | _ => None
    }
  | Some(Node.If(c, t, e)) =>
    switch (
      reconstruct(store, c),
      reconstruct(store, t),
      reconstruct(store, e),
    ) {
    | (Some(c'), Some(t'), Some(e')) => Some(Ast.If(c', t', e'))
    | _ => None
    }
  | Some(Node.Pair(a, b)) =>
    switch (reconstruct(store, a), reconstruct(store, b)) {
    | (Some(a'), Some(b')) => Some(Ast.Pair(a', b'))
    | _ => None
    }
  | Some(Node.Fst(a)) =>
    Option.map(a' => Ast.Fst(a'), reconstruct(store, a))
  | Some(Node.Snd(a)) =>
    Option.map(a' => Ast.Snd(a'), reconstruct(store, a))
  | Some(Node.Prim(op, args)) =>
    let rec rec_all = args =>
      switch (args) {
      | [] => Some([])
      | [h, ...rest] =>
        switch (reconstruct(store, h), rec_all(rest)) {
        | (Some(a), Some(rest')) => Some([a, ...rest'])
        | _ => None
        }
      };
    Option.map(args' => Ast.Prim(op, args'), rec_all(args));
  };
