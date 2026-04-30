/* Resolver — surface→internal AST translation with name resolution
   and ingest pipeline.

   Name resolution: a Var(name) lookup checks the binder context first
   (de Bruijn index for the innermost binder); if unbound, falls
   through to Namespace.resolve_query, which supports both full-path
   and longest-segment-suffix matching. Ambiguity becomes
   Ambiguous_name with the candidate list. The resolved hash must
   denote a `Definition.Term` — a name bound to a type definition is a
   kind error in expression position.

   Type resolution (resolve_ty): walks Surface_ty.t, expanding
   `Surface_ty.Named(name)` via the same namespace-resolution path
   constrained to `Definition.Type`. Hole-in-type-position resolves
   to `Ty.Int` (matching pre-named-types behavior; promotion to a
   first-class `Ty.Unknown` is tracked in open-questions).

   Ingest pipeline (terms):
     parse → resolve names + types → typecheck (permissive) → reject
     if Ill_typed → store → register typecheck aspect → register
     has-holes aspect.

   Ingest pipeline (types):
     parse → resolve_ty → register as Definition.Type → return hash.
     No typecheck or has-holes aspects today.

   Lam and Let both bind by name on the surface and by de Bruijn index
   internally. Let's binder name is pushed onto context for the body
   only (rhs is in the outer scope). */

let ( let* ) = Result.bind;

type error =
  | Unbound_name(string)
  | Ambiguous_name(string, list(string))
  | Missing_hash(string, Hash.t)
  | Kind_mismatch({
      name: string,
      expected: Definition.kind,
      got: Definition.kind,
    })
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
  | Kind_mismatch({name, expected, got}) =>
    "kind mismatch: "
    ++ name
    ++ " is bound to a "
    ++ Definition.kind_to_string(got)
    ++ ", expected a "
    ++ Definition.kind_to_string(expected)
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

/* Resolve a surface type to a concrete Ty.t. Named references go
   through the namespace; the bound hash must denote a
   `Definition.Type`. Hole defaults to Ty.Int. */
let rec resolve_ty =
        (~namespace: Namespace.t, ~store: Store.t, s: Surface_ty.t)
        : result(Ty.t, error) =>
  switch (s) {
  | Surface_ty.Int => Ok(Ty.Int)
  | Surface_ty.Bool => Ok(Ty.Bool)
  | Surface_ty.String => Ok(Ty.String)
  | Surface_ty.Hole => Ok(Ty.Int) /* hole-in-type → Int (best guess) */
  | Surface_ty.Arrow(a, b) =>
    let* a' = resolve_ty(~namespace, ~store, a);
    let* b' = resolve_ty(~namespace, ~store, b);
    Ok(Ty.Arrow(a', b'));
  | Surface_ty.Product(a, b) =>
    let* a' = resolve_ty(~namespace, ~store, a);
    let* b' = resolve_ty(~namespace, ~store, b);
    Ok(Ty.Product(a', b'));
  | Surface_ty.Named(name) =>
    switch (Namespace.resolve_query(namespace, name)) {
    | Error(Namespace.Unbound) => Error(Unbound_name(name))
    | Error(Namespace.Ambiguous(candidates)) =>
      Error(Ambiguous_name(name, candidates))
    | Ok(h) =>
      switch (Store.lookup(store, h)) {
      | None => Error(Missing_hash(name, h))
      | Some(Definition.Type(ty)) => Ok(ty)
      | Some(Definition.Term(_)) =>
        Error(
          Kind_mismatch({
            name,
            expected: Definition.Type_kind,
            got: Definition.Term_kind,
          }),
        )
      }
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
        switch (Store.lookup(store, h)) {
        | None => Error(Missing_hash(name, h))
        | Some(Definition.Type(_)) =>
          Error(
            Kind_mismatch({
              name,
              expected: Definition.Term_kind,
              got: Definition.Type_kind,
            }),
          )
        | Some(Definition.Term(_)) =>
          switch (Store.reconstruct(store, h)) {
          | None => Error(Missing_hash(name, h))
          | Some(ast) => Ok(ast)
          }
        }
      }
    }
  | Surface_ast.Int_lit(n) => Ok(Ast.Int_lit(n))
  | Surface_ast.Bool_lit(b) => Ok(Ast.Bool_lit(b))
  | Surface_ast.String_lit(s) => Ok(Ast.String_lit(s))
  | Surface_ast.Hole => Ok(Ast.Hole)
  | Surface_ast.Lam(x, ty, body) =>
    let* ty' = resolve_ty(~namespace, ~store, ty);
    let* body' =
      resolve_ctx(~context=[x, ...context], ~namespace, ~store, body);
    Ok(Ast.Lam(ty', body'));
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
   actually looked up in the namespace (the resolved name + the hash
   it resolved to, via Namespace.resolve_query). Used by the UI to
   render "resolved" chips next to the editor. Bound variables are
   skipped. Names that fail to resolve (Unbound or Ambiguous) are
   skipped too — they aren't true resolved references. Includes both
   term-position references and named-type references inside lambda
   annotations, since both go through the namespace. */

let collect_resolved_names =
    (~namespace: Namespace.t, surface: Surface_ast.t)
    : list((string, Hash.t)) => {
  let acc = ref([]);
  let push = (name, h) =>
    if (!List.exists(((n, _)) => n == name, acc^)) {
      acc := [(name, h), ...acc^];
    };
  let try_resolve = name =>
    switch (Namespace.resolve_query(namespace, name)) {
    | Ok(h) => push(name, h)
    | Error(_) => ()
    };
  let rec walk_ty = (ty: Surface_ty.t) =>
    switch (ty) {
    | Surface_ty.Int
    | Surface_ty.Bool
    | Surface_ty.String
    | Surface_ty.Hole => ()
    | Surface_ty.Named(n) => try_resolve(n)
    | Surface_ty.Arrow(a, b)
    | Surface_ty.Product(a, b) =>
      walk_ty(a);
      walk_ty(b);
    };
  let rec walk = (~in_scope, s) =>
    switch (s) {
    | Surface_ast.Var(name) =>
      if (!List.exists(n => n == name, in_scope)) {
        try_resolve(name);
      }
    | Surface_ast.Int_lit(_)
    | Surface_ast.Bool_lit(_)
    | Surface_ast.String_lit(_)
    | Surface_ast.Hole => ()
    | Surface_ast.Lam(x, ty, body) =>
      walk_ty(ty);
      walk(~in_scope=[x, ...in_scope], body);
    | Surface_ast.Let(x, rhs, body) =>
      walk(~in_scope, rhs);
      walk(~in_scope=[x, ...in_scope], body);
    | Surface_ast.App(f, a) =>
      walk(~in_scope, f);
      walk(~in_scope, a);
    | Surface_ast.If(c, t, e) =>
      walk(~in_scope, c);
      walk(~in_scope, t);
      walk(~in_scope, e);
    | Surface_ast.Pair(a, b) =>
      walk(~in_scope, a);
      walk(~in_scope, b);
    | Surface_ast.Fst(a)
    | Surface_ast.Snd(a) => walk(~in_scope, a)
    | Surface_ast.Prim(_, args) =>
      List.iter(a => walk(~in_scope, a), args)
    };
  walk(~in_scope=[], surface);
  List.rev(acc^);
};

/* Symmetric for type-pane input. */
let collect_resolved_names_ty =
    (~namespace: Namespace.t, surface_ty: Surface_ty.t)
    : list((string, Hash.t)) => {
  let acc = ref([]);
  let push = (name, h) =>
    if (!List.exists(((n, _)) => n == name, acc^)) {
      acc := [(name, h), ...acc^];
    };
  let rec walk = (ty: Surface_ty.t) =>
    switch (ty) {
    | Surface_ty.Int
    | Surface_ty.Bool
    | Surface_ty.String
    | Surface_ty.Hole => ()
    | Surface_ty.Named(n) =>
      switch (Namespace.resolve_query(namespace, n)) {
      | Ok(h) => push(n, h)
      | Error(_) => ()
      }
    | Surface_ty.Arrow(a, b)
    | Surface_ty.Product(a, b) =>
      walk(a);
      walk(b);
    };
  walk(surface_ty);
  List.rev(acc^);
};

/* Ingest pipeline (terms): surface → resolve → typecheck → store +
   aspects. Type discipline: Ill_typed rejects ingest. Well_typed and
   Well_typed_with_holes both ingest, with the appropriate aspect
   variant. The has-holes aspect runs after ingest; it walks the
   stored DAG syntactically. */

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
    Typecheck.attach_result(~store, att, ~target=h, well_typed);
    let has_holes = Has_holes.compute(~store, ~att, h);
    Ok({hash: h, was_new, type_result: well_typed, has_holes});
  };
};

/* Ingest pipeline (types): surface_ty → resolve_ty → register as
   Definition.Type → return hash. Idempotent — repeat ingest of an
   identical type returns the same hash with `was_new = false`. No
   typecheck or has-holes aspects (types in p9 have no holes and no
   meta-type system). */

type ingest_ty_ok = {
  hash: Hash.t,
  was_new: bool,
  ty: Ty.t,
};

let ingest_ty =
    (~namespace: Namespace.t, ~store: Store.t, surface_ty: Surface_ty.t)
    : result(ingest_ty_ok, error) => {
  let* ty = resolve_ty(~namespace, ~store, surface_ty);
  let size_before = Store.size(store);
  let h = Store.register_type(store, ty);
  let was_new = Store.size(store) > size_before;
  Ok({hash: h, was_new, ty});
};
