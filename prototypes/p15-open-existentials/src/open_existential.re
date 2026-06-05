/* Open existentials into the namespace, generalized to n nested quantifiers.
   `exists c. exists k. T` carries two *distinct* abstract types; opening peels
   every leading `exists`, minting one fresh `Abstract` per level (nesting `Open`
   nodes), then projects the operations product positionally. The substrate's
   unary `Open` composes — `Open{Open{pkg, m1}, m2}` type-checks to the body with
   both abstracts substituted — so no substrate change is needed; this is just
   the iteration plus the positional field projection.

   Naming the extracted types and fields is the caller's job (interface layer):
   these functions return hashes only. */

/* leading-`exists` count + the operations body (bound type vars left as TVars),
   for inspecting shape without minting anything */
let rec strip_exists = (st: Store.t, ty: Hash.t, depth: int): (int, Hash.t) =>
  switch (Store.find(st, ty)) {
  | Some(Definition.Type(Tnode.Exists(body))) => strip_exists(st, body, depth + 1)
  | _ => (depth, ty)
  };

/* flatten a right-nested product into its leaf types (the module's fields) */
let rec field_types = (st: Store.t, ty: Hash.t): list(Hash.t) =>
  switch (Store.find(st, ty)) {
  | Some(Definition.Type(Tnode.Product(a, b))) => [a, ...field_types(st, b)]
  | _ => [ty]
  };

/* Inspect a package's shape: (#abstract types, the field types of the
   operations record). Field types still mention the bound type vars as TVars
   (de Bruijn depth = #types), so a caller can pretty-print them at that tdepth.
   None when [node] is not an existential. */
let inspect = (st: Store.t, node: Node.t): option((int, list(Hash.t))) => {
  let env = Store.build_env(st);
  switch (Typecheck.synth_top(env, [], node)) {
  | Error(_) => None
  | Ok(ty) =>
    let (arity, ops) = strip_exists(st, ty, 0);
    arity == 0 ? None : Some((arity, field_types(st, ops)));
  };
};

type opened = {
  type_hashes: list(Hash.t), /* the minted Abstract types, outer-first */
  module_hash: Hash.t, /* the fully-opened operations value */
  field_hashes: list(Hash.t), /* positional projections of the module */
};

/* Generatively open every leading `exists`: mint a fresh Abstract per level,
   nest `Open` nodes, then project the operations product positionally. */
let open_package =
    (st: Store.t, mint_src: Mint.source, pkg: Node.t): result(opened, string) => {
  let env = Store.build_env(st);
  let rec peel = (node: Node.t, last: option(Hash.t), acc: list(Hash.t)) =>
    switch (Typecheck.synth_top(env, [], node)) {
    | Error(m) => Error(m)
    | Ok(ty) =>
      switch (Store.find(st, ty)) {
      | Some(Definition.Type(Tnode.Exists(_))) =>
        let m = Mint.fresh(mint_src);
        let at = Store.ingest_type(st, Tnode.Abstract(m));
        switch (Store.ingest_term(st, Node.Open({pkg: node, mint: m}))) {
        | Error(e) => Error(e)
        | Ok(oh) => peel(Node.Ref(oh), Some(oh), [at, ...acc])
        };
      | _ => Ok((List.rev(acc), last, ty))
      }
    };
  switch (peel(pkg, None, [])) {
  | Error(e) => Error(e)
  | Ok((_, None, _)) => Error("not an existential package")
  | Ok((type_hashes, Some(module_hash), ops_ty)) =>
    let ftypes = field_types(st, ops_ty);
    let k = List.length(ftypes);
    let rec snds = (i, n) => i <= 0 ? n : snds(i - 1, Node.Snd(n));
    let field_hashes =
      List.mapi(
        (i, _ft) => {
          let base = snds(i, Node.Ref(module_hash));
          let proj = i == k - 1 ? base : Node.Fst(base);
          switch (Store.ingest_term(st, proj)) {
          | Ok(h) => h
          | Error(_) => module_hash
          };
        },
        ftypes,
      );
    Ok({type_hashes, module_hash, field_hashes});
  };
};
