/* Content-addressed DAG storage. p10 holds five entry sorts in one
   table:

   - Substructure: Term(Node.t) and Type(Ty.t) — structural,
     content-shareable. Internal AST nodes and inline types live here.
   - Named: Named_term(Mint, body_hash) and Named_type(Mint, body_hash)
     — mint-wrapped pointers into substructure. User-bound top-level
     definitions live here. Two ingests of the same source mint two
     distinct Named entries pointing at the same body.
   - Label(Label.t) — minted record/constructor/method identities.

   Namespace bindings point at Named entries (or Labels). References
   inside stored bodies — Lam's parameter type, App's children, Let's
   rhs/body — point at substructure entries, so the DAG's structural
   sharing wins are preserved regardless of the surrounding mint. */

type t = Hashtbl.t(Hash.t, Definition.t);

let create = (): t => Hashtbl.create(64);

/* Substructure registration. Same shape as p9 — registering the same
   Node.t / Ty.t twice is idempotent. */

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

/* Named registration. Mints a fresh mark every call; never idempotent.
   The body_hash must already be registered as a substructure
   Term / Type — the caller is responsible. */

let register_named_term = (store: t, body: Hash.t): Hash.t => {
  let mint = Mint.fresh();
  let def = Definition.Named_term(mint, body);
  let h = Definition.hash(def);
  Hashtbl.add(store, h, def);
  h;
};

let register_named_type = (store: t, body: Hash.t): Hash.t => {
  let mint = Mint.fresh();
  let def = Definition.Named_type(mint, body);
  let h = Definition.hash(def);
  Hashtbl.add(store, h, def);
  h;
};

let register_label = (store: t, label: Label.t): Hash.t => {
  let h = Label.hash(label);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, Definition.Label(label));
  };
  h;
};

let lookup = (store: t, h: Hash.t): option(Definition.t) =>
  Hashtbl.find_opt(store, h);

/* `lookup_term` and `lookup_type` follow named wrappers automatically.
   Callers that want the underlying substructure should not need to
   care whether they're holding a named hash or a substructure hash. */
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

/* Unwrap Named_term and Named_type wrappers: returns the substructure
   body hash if `h` is a Named wrapper, otherwise `h` unchanged. Used
   by aspect lookups (typecheck, has-holes) so callers can pass a
   user-visible Named hash and get the cache hit on the body. */
let rec unwrap_named = (store: t, h: Hash.t): Hash.t =>
  switch (lookup(store, h)) {
  | Some(Definition.Named_term(_, body))
  | Some(Definition.Named_type(_, body)) => unwrap_named(store, body)
  | _ => h
  };

let has = (store: t, h: Hash.t): bool => Hashtbl.mem(store, h);

let size = (store: t): int => Hashtbl.length(store);

let hashes = (store: t): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], store, []);

let entries = (store: t): list((Hash.t, Definition.t)) =>
  Hashtbl.fold((h, n, acc) => [(h, n), ...acc], store, []);

/* Convenience: only the term entries (substructure + named).
   `term_entries` returns substructure pairs (Hash.t * Node.t); for
   named-term iteration use `entries` and filter. */
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
    store,
    [],
  );

let resolve_prefix = (store: t, prefix: string): Hash.lookup_result =>
  Hash.lookup_by_prefix(prefix, hashes(store));

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
