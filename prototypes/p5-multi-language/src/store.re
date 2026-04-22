/* Multi-language content-addressed DAG storage. Keyed by Hash.t; values
   are Definition.t (an Arith|Lc sum). The Store is language-agnostic at
   the table level but enforces the "no cross-language references" rule
   from docs/design/03-content-addressing.md at registration time: a
   stored Arith parent's child hashes must all resolve to Arith
   definitions, and likewise for Lc. Mixed children are rejected with
   Language_mismatch. */

type t = Hashtbl.t(Hash.t, Definition.t);

let create = (): t => Hashtbl.create(64);

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

let language_of = (store: t, h: Hash.t): option(string) =>
  Option.map(Definition.language, lookup(store, h));

/* ==================== Language validation ==================== */

exception Language_mismatch(string /* parent_language */, Hash.t /* child */, string /* child_language */);

let check_child_language = (store: t, ~parent_lang: string, child: Hash.t): unit =>
  switch (lookup(store, child)) {
  | None => ()
  | Some(d) when Definition.language(d) == parent_lang => ()
  | Some(d) =>
    raise(Language_mismatch(parent_lang, child, Definition.language(d)))
  };

/* ==================== Arith registration + ingest ==================== */

let register_arith_node = (store: t, n: Arith_node.t): Hash.t => {
  switch (n) {
  | Arith_node.True
  | Arith_node.False
  | Arith_node.Zero => ()
  | Arith_node.Succ(h)
  | Arith_node.Pred(h)
  | Arith_node.IsZero(h) =>
    check_child_language(store, ~parent_lang="arith", h)
  | Arith_node.If(c, t, e) =>
    check_child_language(store, ~parent_lang="arith", c);
    check_child_language(store, ~parent_lang="arith", t);
    check_child_language(store, ~parent_lang="arith", e);
  };
  let h = Arith_node.hash(n);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, Definition.Arith(n));
  };
  h;
};

let rec ingest_arith = (store: t, ast: Arith_ast.t): Hash.t =>
  switch (ast) {
  | Arith_ast.True => register_arith_node(store, Arith_node.True)
  | Arith_ast.False => register_arith_node(store, Arith_node.False)
  | Arith_ast.Zero => register_arith_node(store, Arith_node.Zero)
  | Arith_ast.Succ(a) =>
    let h = ingest_arith(store, a);
    register_arith_node(store, Arith_node.Succ(h));
  | Arith_ast.Pred(a) =>
    let h = ingest_arith(store, a);
    register_arith_node(store, Arith_node.Pred(h));
  | Arith_ast.IsZero(a) =>
    let h = ingest_arith(store, a);
    register_arith_node(store, Arith_node.IsZero(h));
  | Arith_ast.If(c, thn, els) =>
    let ch = ingest_arith(store, c);
    let th = ingest_arith(store, thn);
    let eh = ingest_arith(store, els);
    register_arith_node(store, Arith_node.If(ch, th, eh));
  };

let rec reconstruct_arith = (store: t, h: Hash.t): option(Arith_ast.t) =>
  switch (lookup(store, h)) {
  | None => None
  | Some(Definition.Lc(_)) => None
  | Some(Definition.Arith(Arith_node.True)) => Some(Arith_ast.True)
  | Some(Definition.Arith(Arith_node.False)) => Some(Arith_ast.False)
  | Some(Definition.Arith(Arith_node.Zero)) => Some(Arith_ast.Zero)
  | Some(Definition.Arith(Arith_node.Succ(ch))) =>
    Option.map(a => Arith_ast.Succ(a), reconstruct_arith(store, ch))
  | Some(Definition.Arith(Arith_node.Pred(ch))) =>
    Option.map(a => Arith_ast.Pred(a), reconstruct_arith(store, ch))
  | Some(Definition.Arith(Arith_node.IsZero(ch))) =>
    Option.map(a => Arith_ast.IsZero(a), reconstruct_arith(store, ch))
  | Some(Definition.Arith(Arith_node.If(c, t, e))) =>
    switch (
      reconstruct_arith(store, c),
      reconstruct_arith(store, t),
      reconstruct_arith(store, e),
    ) {
    | (Some(c'), Some(t'), Some(e')) => Some(Arith_ast.If(c', t', e'))
    | _ => None
    }
  };

/* ==================== Lc registration + ingest ==================== */

let register_lc_node = (store: t, n: Lc_node.t): Hash.t => {
  switch (n) {
  | Lc_node.Var(_) => ()
  | Lc_node.Lam(h) => check_child_language(store, ~parent_lang="lc", h)
  | Lc_node.App(f, a) =>
    check_child_language(store, ~parent_lang="lc", f);
    check_child_language(store, ~parent_lang="lc", a);
  };
  let h = Lc_node.hash(n);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, Definition.Lc(n));
  };
  h;
};

let rec ingest_lc = (store: t, ast: Lc_ast.t): Hash.t =>
  switch (ast) {
  | Lc_ast.Var(k) => register_lc_node(store, Lc_node.Var(k))
  | Lc_ast.Lam(body) =>
    let h = ingest_lc(store, body);
    register_lc_node(store, Lc_node.Lam(h));
  | Lc_ast.App(f, a) =>
    let fh = ingest_lc(store, f);
    let ah = ingest_lc(store, a);
    register_lc_node(store, Lc_node.App(fh, ah));
  };

let rec reconstruct_lc = (store: t, h: Hash.t): option(Lc_ast.t) =>
  switch (lookup(store, h)) {
  | None => None
  | Some(Definition.Arith(_)) => None
  | Some(Definition.Lc(Lc_node.Var(k))) => Some(Lc_ast.Var(k))
  | Some(Definition.Lc(Lc_node.Lam(ch))) =>
    Option.map(a => Lc_ast.Lam(a), reconstruct_lc(store, ch))
  | Some(Definition.Lc(Lc_node.App(f, a))) =>
    switch (reconstruct_lc(store, f), reconstruct_lc(store, a)) {
    | (Some(f'), Some(a')) => Some(Lc_ast.App(f', a'))
    | _ => None
    }
  };
