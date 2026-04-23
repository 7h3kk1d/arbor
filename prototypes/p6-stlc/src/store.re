/* Multi-language content-addressed DAG storage. Keyed by Hash.t; values
   are Definition.t (an Lc | Stlc sum). The Store is language-agnostic
   at the table level but enforces the "no cross-language references"
   rule from docs/design/03-content-addressing.md at registration time:
   a stored Lc parent's child hashes must all resolve to Lc definitions,
   and likewise for Stlc. Mixed children are rejected with
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
  | Some(Definition.Stlc(_)) => None
  | Some(Definition.Lc(Lc_node.Var(k))) => Some(Lc_ast.Var(k))
  | Some(Definition.Lc(Lc_node.Lam(ch))) =>
    Option.map(a => Lc_ast.Lam(a), reconstruct_lc(store, ch))
  | Some(Definition.Lc(Lc_node.App(f, a))) =>
    switch (reconstruct_lc(store, f), reconstruct_lc(store, a)) {
    | (Some(f'), Some(a')) => Some(Lc_ast.App(f', a'))
    | _ => None
    }
  };

/* ==================== Stlc registration + ingest ==================== */

let register_stlc_node = (store: t, n: Stlc_node.t): Hash.t => {
  switch (n) {
  | Stlc_node.Var(_)
  | Stlc_node.True
  | Stlc_node.False => ()
  | Stlc_node.Lam(_, h) => check_child_language(store, ~parent_lang="stlc", h)
  | Stlc_node.App(f, a) =>
    check_child_language(store, ~parent_lang="stlc", f);
    check_child_language(store, ~parent_lang="stlc", a);
  | Stlc_node.If(c, t, e) =>
    check_child_language(store, ~parent_lang="stlc", c);
    check_child_language(store, ~parent_lang="stlc", t);
    check_child_language(store, ~parent_lang="stlc", e);
  };
  let h = Stlc_node.hash(n);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, Definition.Stlc(n));
  };
  h;
};

let rec ingest_stlc = (store: t, ast: Stlc_ast.t): Hash.t =>
  switch (ast) {
  | Stlc_ast.Var(k) => register_stlc_node(store, Stlc_node.Var(k))
  | Stlc_ast.True => register_stlc_node(store, Stlc_node.True)
  | Stlc_ast.False => register_stlc_node(store, Stlc_node.False)
  | Stlc_ast.Lam(ty, body) =>
    let h = ingest_stlc(store, body);
    register_stlc_node(store, Stlc_node.Lam(ty, h));
  | Stlc_ast.App(f, a) =>
    let fh = ingest_stlc(store, f);
    let ah = ingest_stlc(store, a);
    register_stlc_node(store, Stlc_node.App(fh, ah));
  | Stlc_ast.If(c, t, e) =>
    let ch = ingest_stlc(store, c);
    let th = ingest_stlc(store, t);
    let eh = ingest_stlc(store, e);
    register_stlc_node(store, Stlc_node.If(ch, th, eh));
  };

let rec reconstruct_stlc = (store: t, h: Hash.t): option(Stlc_ast.t) =>
  switch (lookup(store, h)) {
  | None => None
  | Some(Definition.Lc(_)) => None
  | Some(Definition.Stlc(Stlc_node.Var(k))) => Some(Stlc_ast.Var(k))
  | Some(Definition.Stlc(Stlc_node.True)) => Some(Stlc_ast.True)
  | Some(Definition.Stlc(Stlc_node.False)) => Some(Stlc_ast.False)
  | Some(Definition.Stlc(Stlc_node.Lam(ty, ch))) =>
    Option.map(a => Stlc_ast.Lam(ty, a), reconstruct_stlc(store, ch))
  | Some(Definition.Stlc(Stlc_node.App(f, a))) =>
    switch (reconstruct_stlc(store, f), reconstruct_stlc(store, a)) {
    | (Some(f'), Some(a')) => Some(Stlc_ast.App(f', a'))
    | _ => None
    }
  | Some(Definition.Stlc(Stlc_node.If(c, t, e))) =>
    switch (
      reconstruct_stlc(store, c),
      reconstruct_stlc(store, t),
      reconstruct_stlc(store, e),
    ) {
    | (Some(c'), Some(t'), Some(e')) => Some(Stlc_ast.If(c', t', e'))
    | _ => None
    }
  };
