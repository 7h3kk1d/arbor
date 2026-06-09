/* Open existentials into the namespace, generalized to n nested quantifiers and
   to a record interface. `exists c. exists k. T` carries two *distinct* abstract
   types; opening peels every leading `exists`, minting one fresh `Abstract` per
   level (nesting `Open` nodes — the substrate's unary `Open` composes, so no
   substrate change). The operations interface T may be:

   - a positional `Product` (p13–p15 style): fields are projected by Fst/Snd and
     have no names — the caller supplies them; or
   - a `Record` (p16): fields are projected by `Project_field` *by label*, and
     each field carries its label hash so the caller can recover its name.

   Naming the extracted types is still the caller's job; these functions return
   hashes (and, for records, the field labels). */

/* leading-`exists` count + the operations body (bound type vars left as TVars),
   for inspecting shape without minting anything */
let rec strip_exists = (st: Store.t, ty: Hash.t, depth: int): (int, Hash.t) =>
  switch (Store.find(st, ty)) {
  | Some(Definition.Type(Tnode.Exists(body))) => strip_exists(st, body, depth + 1)
  | _ => (depth, ty)
  };

/* The interface's fields as `(optional label hash, field-type hash)`: a Record
   yields its labeled fields (sorted by label hash for a stable order); a
   right-nested Product flattens positionally with no labels; anything else is a
   single unnamed field. */
let rec interface_fields =
        (st: Store.t, ty: Hash.t): list((option(Hash.t), Hash.t)) =>
  switch (Store.find(st, ty)) {
  | Some(Definition.Type(Tnode.Record(fields))) =>
    fields
    |> List.sort(((l1, _), (l2, _)) => String.compare(l1, l2))
    |> List.map(((l, ft)) => (Some(l), ft))
  | Some(Definition.Type(Tnode.Product(a, b))) =>
    [(None, a), ...interface_fields(st, b)]
  | _ => [(None, ty)]
  };

/* Inspect a package's shape: (#abstract types, its interface fields). Field
   types still mention the bound type vars as TVars (de Bruijn depth = #types),
   so a caller can pretty-print them at that tdepth; the label (if any) lets a
   record-interfaced field be pre-named. None when [node] is not an existential. */
let inspect =
    (st: Store.t, node: Node.t): option((int, list((option(Hash.t), Hash.t)))) => {
  let env = Store.build_env(st);
  switch (Typecheck.synth_top(env, [], node)) {
  | Error(_) => None
  | Ok(ty) =>
    let (arity, ops) = strip_exists(st, ty, 0);
    arity == 0 ? None : Some((arity, interface_fields(st, ops)));
  };
};

type opened = {
  type_hashes: list(Hash.t), /* the minted Abstract types, outer-first */
  module_hash: Hash.t, /* the fully-opened operations value */
  fields: list((option(Hash.t), Hash.t)) /* (label hash if a record, projection node hash) */,
};

/* Generatively open every leading `exists`: mint a fresh Abstract per level,
   nest `Open` nodes, then project each operation field — by label for a record
   interface, positionally for a product. */
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
    let ifields = interface_fields(st, ops_ty);
    let n = List.length(ifields);
    let rec snds = (i, nd) => i <= 0 ? nd : snds(i - 1, Node.Snd(nd));
    let fields =
      List.mapi(
        (i, (label_opt, _ft)) => {
          let proj =
            switch (label_opt) {
            | Some(lh) => Node.Project_field(Node.Ref(module_hash), lh)
            | None =>
              let base = snds(i, Node.Ref(module_hash));
              i == n - 1 ? base : Node.Fst(base);
            };
          let fh =
            switch (Store.ingest_term(st, proj)) {
            | Ok(h) => h
            | Error(_) => module_hash
            };
          (label_opt, fh);
        },
        ifields,
      );
    Ok({type_hashes, module_hash, fields});
  };
};
