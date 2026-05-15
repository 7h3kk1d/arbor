/* Content-addressed DAG storage. p11 holds five entry sorts in one
   table:

   - Substructure: Term(Node.t) and Type(Ty.t) — structural,
     content-shareable. Internal AST nodes and inline types live here.
   - Named: Named_term(Mint, body_hash) and Named_type(Mint, body_hash)
     — mint-wrapped pointers into substructure. User-bound top-level
     definitions live here. Two plain ingests of the same source mint
     two distinct Named entries pointing at the same body; an explicit
     edit-of-X gesture (via `register_named_term_with_mint`) reuses
     the prior mint so the new hash joins the same mint thread.
   - Label(Label.t) — minted record/constructor/method identities.

   Namespace bindings point at Named entries (or Labels). References
   inside stored bodies — Lam's parameter type, App's children, Let's
   rhs/body — point at substructure entries, so the DAG's structural
   sharing wins are preserved regardless of the surrounding mint.

   p11 additions:

   - `threads`: a `mint → list(hash)` index. Every Named_term,
     Named_type, and Label registration appends to its mint's list.
     Used by `thread_of(hash)`.
   - `callers`: a `child_hash → list(parent_hash)` index. Every
     `register_term` walks the Node's child hashes and records
     edges; every `register_named_*` records the body→named edge.
     Used by `callers_of(hash)` to drive the Follow / Migrate
     cascade. */

type t = {
  defs: Hashtbl.t(Hash.t, Definition.t),
  threads: Hashtbl.t(Mint.t, list(Hash.t)),
  callers: Hashtbl.t(Hash.t, list(Hash.t)),
};

let create = (): t => {
  defs: Hashtbl.create(64),
  threads: Hashtbl.create(16),
  callers: Hashtbl.create(64),
};

let record_caller_edge = (store: t, ~child: Hash.t, ~parent: Hash.t): unit => {
  let existing =
    switch (Hashtbl.find_opt(store.callers, child)) {
    | Some(xs) => xs
    | None => []
    };
  if (!List.exists(h => h == parent, existing)) {
    Hashtbl.replace(store.callers, child, [parent, ...existing]);
  };
};

let record_thread_entry = (store: t, ~mint: Mint.t, ~hash: Hash.t): unit => {
  let existing =
    switch (Hashtbl.find_opt(store.threads, mint)) {
    | Some(xs) => xs
    | None => []
    };
  if (!List.exists(h => h == hash, existing)) {
    /* Append, preserving ingest order. */
    Hashtbl.replace(store.threads, mint, existing @ [hash]);
  };
};

let node_children = (n: Node.t): list(Hash.t) =>
  switch (n) {
  | Node.Var(_)
  | Node.Int_lit(_)
  | Node.Bool_lit(_)
  | Node.String_lit(_)
  | Node.Hole => []
  | Node.Lam(ty, body) => [ty, body]
  | Node.App(a, b)
  | Node.Let(a, b)
  | Node.Pair(a, b) => [a, b]
  | Node.If(c, t, e) => [c, t, e]
  | Node.Fst(a)
  | Node.Snd(a) => [a]
  | Node.Tuple(items)
  | Node.List_lit(items) => items
  | Node.Record_lit(fields) =>
    List.concat_map(((l, v)) => [l, v], fields)
  | Node.Record_update(target, fields) =>
    [target, ...List.concat_map(((l, v)) => [l, v], fields)]
  | Node.Project_field(target, label) => [target, label]
  | Node.Project_index(target, _) => [target]
  | Node.Prim(_, args)
  | Node.Prim_call(_, args) => args
  };

/* Substructure registration. Same shape as p10 — registering the same
   Node.t / Ty.t twice is idempotent. The reverse-DAG `callers` index
   is populated only on first registration to avoid duplicate edges. */

let register_term = (store: t, n: Node.t): Hash.t => {
  let h = Node.hash(n);
  if (!Hashtbl.mem(store.defs, h)) {
    Hashtbl.add(store.defs, h, Definition.Term(n));
    List.iter(
      child => record_caller_edge(store, ~child, ~parent=h),
      node_children(n),
    );
  };
  h;
};

let register_type = (store: t, ty: Ty.t): Hash.t => {
  let h = Ty.hash(ty);
  if (!Hashtbl.mem(store.defs, h)) {
    Hashtbl.add(store.defs, h, Definition.Type(ty));
  };
  h;
};

/* Named registration with an explicit mint. Used by p11's edit-of-X
   gesture: ingest the new body, then call this with the prior mint
   to produce a new Named hash that joins the same mint thread. */

let register_named_term_with_mint =
    (store: t, ~mint: Mint.t, body: Hash.t): Hash.t => {
  let def = Definition.Named_term(mint, body);
  let h = Definition.hash(def);
  if (!Hashtbl.mem(store.defs, h)) {
    Hashtbl.add(store.defs, h, def);
    record_thread_entry(store, ~mint, ~hash=h);
    record_caller_edge(store, ~child=body, ~parent=h);
  };
  h;
};

let register_named_type_with_mint =
    (store: t, ~mint: Mint.t, body: Hash.t): Hash.t => {
  let def = Definition.Named_type(mint, body);
  let h = Definition.hash(def);
  if (!Hashtbl.mem(store.defs, h)) {
    Hashtbl.add(store.defs, h, def);
    record_thread_entry(store, ~mint, ~hash=h);
    record_caller_edge(store, ~child=body, ~parent=h);
  };
  h;
};

let register_named_term = (store: t, body: Hash.t): Hash.t =>
  register_named_term_with_mint(store, ~mint=Mint.fresh(), body);

let register_named_type = (store: t, body: Hash.t): Hash.t =>
  register_named_type_with_mint(store, ~mint=Mint.fresh(), body);

let register_label = (store: t, label: Label.t): Hash.t => {
  let h = Label.hash(label);
  if (!Hashtbl.mem(store.defs, h)) {
    Hashtbl.add(store.defs, h, Definition.Label(label));
    record_thread_entry(store, ~mint=Label.mint(label), ~hash=h);
  };
  h;
};

let lookup = (store: t, h: Hash.t): option(Definition.t) =>
  Hashtbl.find_opt(store.defs, h);

let rec lookup_term = (store: t, h: Hash.t): option(Node.t) =>
  switch (lookup(store, h)) {
  | Some(Definition.Term(n)) => Some(n)
  | Some(Definition.Named_term(_, body)) => lookup_term(store, body)
  | _ => None
  };

let rec lookup_type = (store: t, h: Hash.t): option(Ty.t) =>
  switch (lookup(store, h)) {
  | Some(Definition.Type(ty)) => Some(ty)
  | Some(Definition.Named_type(_, body)) => lookup_type(store, body)
  | _ => None
  };

let kind_of = (store: t, h: Hash.t): option(Definition.kind) =>
  Option.map(Definition.kind, lookup(store, h));

let rec unwrap_named = (store: t, h: Hash.t): Hash.t =>
  switch (lookup(store, h)) {
  | Some(Definition.Named_term(_, body))
  | Some(Definition.Named_type(_, body)) => unwrap_named(store, body)
  | _ => h
  };

let has = (store: t, h: Hash.t): bool => Hashtbl.mem(store.defs, h);

let size = (store: t): int => Hashtbl.length(store.defs);

let hashes = (store: t): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], store.defs, []);

let entries = (store: t): list((Hash.t, Definition.t)) =>
  Hashtbl.fold((h, n, acc) => [(h, n), ...acc], store.defs, []);

let term_entries = (store: t): list((Hash.t, Node.t)) =>
  Hashtbl.fold(
    (h, def, acc) =>
      switch (def) {
      | Definition.Term(n) => [(h, n), ...acc]
      | Definition.Type(_)
      | Definition.Named_term(_, _)
      | Definition.Named_type(_, _)
      | Definition.Label(_) => acc
      },
    store.defs,
    [],
  );

let resolve_prefix = (store: t, prefix: string): Hash.lookup_result =>
  Hash.lookup_by_prefix(prefix, hashes(store));

/* Mint thread queries.

   `mint_of` returns the mint embedded in a Named_term, Named_type,
   or Label hash; substructure Term/Type return None.

   `thread_of` returns the list of hashes sharing this hash's mint, in
   ingest order. Singleton case (no edits yet): returns [hash]. */

let mint_of = (store: t, h: Hash.t): option(Mint.t) =>
  switch (lookup(store, h)) {
  | Some(Definition.Named_term(m, _))
  | Some(Definition.Named_type(m, _)) => Some(m)
  | Some(Definition.Label(l)) => Some(Label.mint(l))
  | _ => None
  };

let thread_of = (store: t, h: Hash.t): list(Hash.t) =>
  switch (mint_of(store, h)) {
  | None => [h]
  | Some(mint) =>
    switch (Hashtbl.find_opt(store.threads, mint)) {
    | Some(xs) => xs
    | None => [h]
    }
  };

let callers_of = (store: t, h: Hash.t): list(Hash.t) =>
  switch (Hashtbl.find_opt(store.callers, h)) {
  | Some(xs) => xs
  | None => []
  };

/* Closure of callers — transitive parents reachable from `h`. */
let callers_closure = (store: t, h: Hash.t): list(Hash.t) => {
  let seen = Hashtbl.create(16);
  let order = ref([]);
  let rec walk = h =>
    if (!Hashtbl.mem(seen, h)) {
      Hashtbl.add(seen, h, ());
      List.iter(
        parent => {
          if (!Hashtbl.mem(seen, parent)) {
            order := [parent, ...order^];
            walk(parent);
          }
        },
        callers_of(store, h),
      );
    };
  walk(h);
  List.rev(order^);
};

/* Ingest an Ast.t bottom-up as substructure. Lam's type annotation is
   registered as a substructure `Type` first. Internal references stay
   in substructure space — never wrapped with a mint. The bind action
   is what wraps with `register_named_term`. */
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
  | Ast.Tuple(items) =>
    let item_hashes = List.map(t => ingest(store, t), items);
    register_term(store, Node.Tuple(item_hashes));
  | Ast.List_lit(items) =>
    let item_hashes = List.map(t => ingest(store, t), items);
    register_term(store, Node.List_lit(item_hashes));
  | Ast.Record_lit(fields) =>
    let field_hashes =
      List.map(((label_h, t)) => (label_h, ingest(store, t)), fields);
    register_term(store, Node.Record_lit(field_hashes));
  | Ast.Record_update(target, fields) =>
    let target_h = ingest(store, target);
    let field_hashes =
      List.map(((label_h, t)) => (label_h, ingest(store, t)), fields);
    register_term(store, Node.Record_update(target_h, field_hashes));
  | Ast.Project_field(target, label_h) =>
    let target_h = ingest(store, target);
    register_term(store, Node.Project_field(target_h, label_h));
  | Ast.Project_index(target, i) =>
    let target_h = ingest(store, target);
    register_term(store, Node.Project_index(target_h, i));
  };

/* Reconstruct follows Named wrappers transparently — callers asking
   for the Ast.t at a hash get the body's Ast.t regardless of whether
   the hash is a substructure Term or a Named_term wrapping one. */
let rec reconstruct = (store: t, h: Hash.t): option(Ast.t) =>
  switch (lookup(store, h)) {
  | None
  | Some(Definition.Type(_))
  | Some(Definition.Named_type(_, _))
  | Some(Definition.Label(_)) => None
  | Some(Definition.Named_term(_, body)) => reconstruct(store, body)
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
    | Node.Tuple(items) =>
      let rec rec_all = items =>
        switch (items) {
        | [] => Some([])
        | [h, ...rest] =>
          switch (reconstruct(store, h), rec_all(rest)) {
          | (Some(a), Some(rest')) => Some([a, ...rest'])
          | _ => None
          }
        };
      Option.map(items' => Ast.Tuple(items'), rec_all(items));
    | Node.List_lit(items) =>
      let rec rec_all = items =>
        switch (items) {
        | [] => Some([])
        | [h, ...rest] =>
          switch (reconstruct(store, h), rec_all(rest)) {
          | (Some(a), Some(rest')) => Some([a, ...rest'])
          | _ => None
          }
        };
      Option.map(items' => Ast.List_lit(items'), rec_all(items));
    | Node.Record_lit(fields) =>
      let rec rec_all = fields =>
        switch (fields) {
        | [] => Some([])
        | [(label_h, value_h), ...rest] =>
          switch (reconstruct(store, value_h), rec_all(rest)) {
          | (Some(v), Some(rest')) => Some([(label_h, v), ...rest'])
          | _ => None
          }
        };
      Option.map(fields' => Ast.Record_lit(fields'), rec_all(fields));
    | Node.Record_update(target, fields) =>
      switch (reconstruct(store, target)) {
      | None => None
      | Some(target') =>
        let rec rec_all = fields =>
          switch (fields) {
          | [] => Some([])
          | [(label_h, value_h), ...rest] =>
            switch (reconstruct(store, value_h), rec_all(rest)) {
            | (Some(v), Some(rest')) => Some([(label_h, v), ...rest'])
            | _ => None
            }
          };
        Option.map(
          fields' => Ast.Record_update(target', fields'),
          rec_all(fields),
        );
      }
    | Node.Project_field(target, label_h) =>
      Option.map(
        t' => Ast.Project_field(t', label_h),
        reconstruct(store, target),
      )
    | Node.Project_index(target, i) =>
      Option.map(
        t' => Ast.Project_index(t', i),
        reconstruct(store, target),
      )
    }
  };
