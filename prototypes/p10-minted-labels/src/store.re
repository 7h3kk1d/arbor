/* Content-addressed DAG storage. p9 holds two definition kinds in one
   table: terms (Node.t) under tag byte 'P', types (Ty.t) under leading
   byte 'T'. Lookup returns Definition.t; consumers that only want one
   kind use lookup_term / lookup_type. */

type t = Hashtbl.t(Hash.t, Definition.t);

let create = (): t => Hashtbl.create(64);

let register_term = (store: t, n: Node.t): Hash.t => {
  let h = Node.hash(n);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, Definition.Term(n));
  };
  h;
};

let register_type = (store: t, ty: Ty.t): Hash.t => {
  let h = Ty.hash(ty);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, Definition.Type(ty));
  };
  h;
};

let lookup = (store: t, h: Hash.t): option(Definition.t) =>
  Hashtbl.find_opt(store, h);

let lookup_term = (store: t, h: Hash.t): option(Node.t) =>
  switch (lookup(store, h)) {
  | Some(Definition.Term(n)) => Some(n)
  | _ => None
  };

let lookup_type = (store: t, h: Hash.t): option(Ty.t) =>
  switch (lookup(store, h)) {
  | Some(Definition.Type(ty)) => Some(ty)
  | _ => None
  };

let kind_of = (store: t, h: Hash.t): option(Definition.kind) =>
  Option.map(Definition.kind, lookup(store, h));

let has = (store: t, h: Hash.t): bool => Hashtbl.mem(store, h);

let size = (store: t): int => Hashtbl.length(store);

let hashes = (store: t): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], store, []);

let entries = (store: t): list((Hash.t, Definition.t)) =>
  Hashtbl.fold((h, n, acc) => [(h, n), ...acc], store, []);

/* Convenience: only the term entries. */
let term_entries = (store: t): list((Hash.t, Node.t)) =>
  Hashtbl.fold(
    (h, def, acc) =>
      switch (def) {
      | Definition.Term(n) => [(h, n), ...acc]
      | Definition.Type(_) => acc
      },
    store,
    [],
  );

let resolve_prefix = (store: t, prefix: string): Hash.lookup_result =>
  Hash.lookup_by_prefix(prefix, hashes(store));

/* Ingest an Ast.t bottom-up. Lam's type annotation is registered as a
   Definition.Type before the Lam itself, so the Node carries the
   type's hash. */
let rec ingest = (store: t, ast: Ast.t): Hash.t =>
  switch (ast) {
  | Ast.Var(k) => register_term(store, Node.Var(k))
  | Ast.Int_lit(n) => register_term(store, Node.Int_lit(n))
  | Ast.Bool_lit(b) => register_term(store, Node.Bool_lit(b))
  | Ast.String_lit(s) => register_term(store, Node.String_lit(s))
  | Ast.Hole => register_term(store, Node.Hole)
  | Ast.Lam(ty, body) =>
    let ty_h = register_type(store, ty);
    let body_h = ingest(store, body);
    register_term(store, Node.Lam(ty_h, body_h));
  | Ast.App(f, a) =>
    let fh = ingest(store, f);
    let ah = ingest(store, a);
    register_term(store, Node.App(fh, ah));
  | Ast.Let(rhs, body) =>
    let rh = ingest(store, rhs);
    let bh = ingest(store, body);
    register_term(store, Node.Let(rh, bh));
  | Ast.If(c, t, e) =>
    let ch = ingest(store, c);
    let th = ingest(store, t);
    let eh = ingest(store, e);
    register_term(store, Node.If(ch, th, eh));
  | Ast.Pair(a, b) =>
    let ah = ingest(store, a);
    let bh = ingest(store, b);
    register_term(store, Node.Pair(ah, bh));
  | Ast.Fst(a) =>
    let ah = ingest(store, a);
    register_term(store, Node.Fst(ah));
  | Ast.Snd(a) =>
    let ah = ingest(store, a);
    register_term(store, Node.Snd(ah));
  | Ast.Prim(op, args) =>
    let arg_hashes = List.map(t => ingest(store, t), args);
    register_term(store, Node.Prim(op, arg_hashes));
  | Ast.Prim_call(id, args) =>
    let arg_hashes = List.map(t => ingest(store, t), args);
    register_term(store, Node.Prim_call(id, arg_hashes));
  };

/* Reconstruct a term Ast.t from its hash. Lam re-fetches its Ty.t
   from the type-hash; a missing type definition produces None. */
let rec reconstruct = (store: t, h: Hash.t): option(Ast.t) =>
  switch (lookup(store, h)) {
  | None
  | Some(Definition.Type(_)) => None
  | Some(Definition.Term(node)) =>
    switch (node) {
    | Node.Var(k) => Some(Ast.Var(k))
    | Node.Int_lit(n) => Some(Ast.Int_lit(n))
    | Node.Bool_lit(b) => Some(Ast.Bool_lit(b))
    | Node.String_lit(s) => Some(Ast.String_lit(s))
    | Node.Hole => Some(Ast.Hole)
    | Node.Lam(ty_h, body_h) =>
      switch (lookup_type(store, ty_h), reconstruct(store, body_h)) {
      | (Some(ty), Some(body)) => Some(Ast.Lam(ty, body))
      | _ => None
      }
    | Node.App(f, a) =>
      switch (reconstruct(store, f), reconstruct(store, a)) {
      | (Some(f'), Some(a')) => Some(Ast.App(f', a'))
      | _ => None
      }
    | Node.Let(rhs, body) =>
      switch (reconstruct(store, rhs), reconstruct(store, body)) {
      | (Some(r), Some(b)) => Some(Ast.Let(r, b))
      | _ => None
      }
    | Node.If(c, t, e) =>
      switch (
        reconstruct(store, c),
        reconstruct(store, t),
        reconstruct(store, e),
      ) {
      | (Some(c'), Some(t'), Some(e')) => Some(Ast.If(c', t', e'))
      | _ => None
      }
    | Node.Pair(a, b) =>
      switch (reconstruct(store, a), reconstruct(store, b)) {
      | (Some(a'), Some(b')) => Some(Ast.Pair(a', b'))
      | _ => None
      }
    | Node.Fst(a) =>
      Option.map(a' => Ast.Fst(a'), reconstruct(store, a))
    | Node.Snd(a) =>
      Option.map(a' => Ast.Snd(a'), reconstruct(store, a))
    | Node.Prim(op, args) =>
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
    | Node.Prim_call(id, args) =>
      let rec rec_all = args =>
        switch (args) {
        | [] => Some([])
        | [h, ...rest] =>
          switch (reconstruct(store, h), rec_all(rest)) {
          | (Some(a), Some(rest')) => Some([a, ...rest'])
          | _ => None
          }
        };
      Option.map(args' => Ast.Prim_call(id, args'), rec_all(args));
    }
  };
