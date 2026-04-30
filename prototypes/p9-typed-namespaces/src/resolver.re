/* Resolver — surface→internal AST translation with name resolution
   and ingest pipeline.

   Name resolution: a Var(name) lookup checks the binder context first
   (de Bruijn index for the innermost binder); if unbound, falls
   through to Namespace.resolve_query, which supports both full-path
   and longest-segment-suffix matching. Ambiguity becomes
   Ambiguous_name with the candidate list.

   Ingest pipeline:
     parse → resolve → typecheck (permissive) → reject if Ill_typed
     → store → register typecheck aspect → register has-holes aspect.

   Lam and Let both bind by name on the surface and by de Bruijn index
   internally. Let's binder name is pushed onto context for the body
   only (rhs is in the outer scope). */

let ( let* ) = Result.bind;

type error =
  | Unbound_name(string)
  | Ambiguous_name(string, list(string))
  | Missing_hash(string, Hash.t)
  | Type_error(string);

let error_to_string =
  fun
  | Unbound_name(n) => "unbound name: " ++ n
  | Ambiguous_name(n, candidates) =>
    "ambiguous name: "
    ++ n
    ++ " could refer to "
    ++ String.concat(", ", candidates)
  | Missing_hash(n, h) =>
    "namespace binding points at missing hash: "
    ++ n
    ++ " -> "
    ++ Hash.short(h)
  | Type_error(msg) => "type error: " ++ msg;

let rec lookup_ctx = (ctx: list(string), name: string): option(int) =>
  switch (ctx) {
  | [] => None
  | [h, ...rest] =>
    if (h == name) {
      Some(0);
    } else {
      Option.map(i => i + 1, lookup_ctx(rest, name));
    }
  };

let rec resolve_ctx =
        (
          ~context: list(string),
          ~namespace: Namespace.t,
          ~store: Store.t,
          s: Surface_ast.t,
        )
        : result(Ast.t, error) =>
  switch (s) {
  | Surface_ast.Var(name) =>
    switch (lookup_ctx(context, name)) {
    | Some(i) => Ok(Ast.Var(i))
    | None =>
      switch (Namespace.resolve_query(namespace, name)) {
      | Error(Namespace.Unbound) => Error(Unbound_name(name))
      | Error(Namespace.Ambiguous(candidates)) =>
        Error(Ambiguous_name(name, candidates))
      | Ok(h) =>
        switch (Store.reconstruct(store, h)) {
        | None => Error(Missing_hash(name, h))
        | Some(ast) => Ok(ast)
        }
      }
    }
  | Surface_ast.Int_lit(n) => Ok(Ast.Int_lit(n))
  | Surface_ast.Bool_lit(b) => Ok(Ast.Bool_lit(b))
  | Surface_ast.String_lit(s) => Ok(Ast.String_lit(s))
  | Surface_ast.Hole => Ok(Ast.Hole)
  | Surface_ast.Lam(x, ty, body) =>
    let* body' =
      resolve_ctx(~context=[x, ...context], ~namespace, ~store, body);
    Ok(Ast.Lam(ty, body'));
  | Surface_ast.App(f, a) =>
    let* f' = resolve_ctx(~context, ~namespace, ~store, f);
    let* a' = resolve_ctx(~context, ~namespace, ~store, a);
    Ok(Ast.App(f', a'));
  | Surface_ast.Let(x, rhs, body) =>
    let* rhs' = resolve_ctx(~context, ~namespace, ~store, rhs);
    let* body' =
      resolve_ctx(~context=[x, ...context], ~namespace, ~store, body);
    Ok(Ast.Let(rhs', body'));
  | Surface_ast.If(c, t, e) =>
    let* c' = resolve_ctx(~context, ~namespace, ~store, c);
    let* t' = resolve_ctx(~context, ~namespace, ~store, t);
    let* e' = resolve_ctx(~context, ~namespace, ~store, e);
    Ok(Ast.If(c', t', e'));
  | Surface_ast.Pair(a, b) =>
    let* a' = resolve_ctx(~context, ~namespace, ~store, a);
    let* b' = resolve_ctx(~context, ~namespace, ~store, b);
    Ok(Ast.Pair(a', b'));
  | Surface_ast.Fst(a) =>
    let* a' = resolve_ctx(~context, ~namespace, ~store, a);
    Ok(Ast.Fst(a'));
  | Surface_ast.Snd(a) =>
    let* a' = resolve_ctx(~context, ~namespace, ~store, a);
    Ok(Ast.Snd(a'));
  | Surface_ast.Prim(op, args) =>
    let rec resolve_all = lst =>
      switch (lst) {
      | [] => Ok([])
      | [a, ...rest] =>
        let* a' = resolve_ctx(~context, ~namespace, ~store, a);
        let* rest' = resolve_all(rest);
        Ok([a', ...rest']);
      };
    let* args' = resolve_all(args);
    Ok(Ast.Prim(op, args'));
  };

let resolve = (~namespace, ~store, s): result(Ast.t, error) =>
  resolve_ctx(~context=[], ~namespace, ~store, s);

/* Walk the surface tree and return the names that the resolver
   actually looked up in the namespace (the resolved name + the hash it
   resolved to, via Namespace.resolve_query). Used by the UI to render
   "resolved" chips next to the editor. Bound variables are skipped.
   Names that fail to resolve (Unbound or Ambiguous) are skipped too —
   they aren't true resolved references. */

let collect_resolved_names =
    (~namespace: Namespace.t, surface: Surface_ast.t)
    : list((string, Hash.t)) => {
  let rec walk = (~in_scope, acc, s) =>
    switch (s) {
    | Surface_ast.Var(name) =>
      if (List.exists(n => n == name, in_scope)) {
        acc;
      } else {
        switch (Namespace.resolve_query(namespace, name)) {
        | Ok(h) =>
          if (List.exists(((n, _)) => n == name, acc)) {
            acc;
          } else {
            [(name, h), ...acc];
          }
        | Error(_) => acc
        };
      }
    | Surface_ast.Int_lit(_)
    | Surface_ast.Bool_lit(_)
    | Surface_ast.String_lit(_)
    | Surface_ast.Hole => acc
    | Surface_ast.Lam(x, _, body) =>
      walk(~in_scope=[x, ...in_scope], acc, body)
    | Surface_ast.Let(x, rhs, body) =>
      let acc = walk(~in_scope, acc, rhs);
      walk(~in_scope=[x, ...in_scope], acc, body);
    | Surface_ast.App(f, a) =>
      let acc = walk(~in_scope, acc, f);
      walk(~in_scope, acc, a);
    | Surface_ast.If(c, t, e) =>
      let acc = walk(~in_scope, acc, c);
      let acc = walk(~in_scope, acc, t);
      walk(~in_scope, acc, e);
    | Surface_ast.Pair(a, b) =>
      let acc = walk(~in_scope, acc, a);
      walk(~in_scope, acc, b);
    | Surface_ast.Fst(a) => walk(~in_scope, acc, a)
    | Surface_ast.Snd(a) => walk(~in_scope, acc, a)
    | Surface_ast.Prim(_, args) =>
      List.fold_left((acc, a) => walk(~in_scope, acc, a), acc, args)
    };
  List.rev(walk(~in_scope=[], [], surface));
};

/* Ingest pipeline: surface → resolve → typecheck → store + aspects.

   Type discipline: Ill_typed rejects ingest. Well_typed and
   Well_typed_with_holes both ingest, with the appropriate aspect
   variant. The has-holes aspect runs after ingest; it walks the stored
   DAG syntactically. */

type ingest_ok = {
  hash: Hash.t,
  was_new: bool,
  type_result: Typecheck.check_result,
  has_holes: bool,
};

let ingest =
    (
      ~namespace: Namespace.t,
      ~store: Store.t,
      ~att: Attachment.t,
      surface: Surface_ast.t,
    )
    : result(ingest_ok, error) => {
  let* ast = resolve(~namespace, ~store, surface);
  let canonical = Canonicalize.canonicalize(ast);
  switch (Typecheck.check_top(canonical)) {
  | Typecheck.Ill_typed(msg) => Error(Type_error(msg))
  | well_typed =>
    let size_before = Store.size(store);
    let h = Store.ingest(store, canonical);
    let was_new = Store.size(store) > size_before;
    Typecheck.attach_result(att, ~target=h, well_typed);
    let has_holes = Has_holes.compute(~store, ~att, h);
    Ok({hash: h, was_new, type_result: well_typed, has_holes});
  };
};
