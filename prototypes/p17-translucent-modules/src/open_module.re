/* Open a module (a Sig-typed value) into the namespace — n-ary in ONE gesture
   (p17; replaces p15/p16's unary-`Open` peeling). A sig hiding several types
   lists them as labeled opaque components; opening mints one fresh witness-less
   `Abstract` per opaque component, all in a single `Open{pkg, mints}` node.

   Because components carry labels, the whole shape is pre-named: the caller can
   recover every type member's and value member's name from its label — no
   user-supplied name list needed (p16's record-interface win, extended to the
   hidden types themselves).

   Naming is still the caller's job; these functions return hashes. */

type shape = {
  /* (label, None = opaque | Some(equation) = manifest), opaques first in rank
     (label-hash sort) order so callers can pair them with minted Abstracts */
  type_comps: list((Hash.t, option(Hash.t))),
  /* (label, member type — under the sig's k binders, rank rule) */
  val_comps: list((Hash.t, Hash.t)),
};

let shape_of_sig =
    (comps: list((Hash.t, Tnode.sig_comp))): shape => {
  let ranks = Tnode.opaque_ranks(comps);
  let manifests =
    List.filter_map(
      ((l, c)) =>
        switch (c) {
        | Tnode.Smanifest(e) => Some((l, Some(e)))
        | _ => None
        },
      comps,
    );
  let type_comps = List.map(l => (l, None), ranks) @ manifests;
  let val_comps =
    List.filter_map(
      ((l, c)) =>
        switch (c) {
        | Tnode.Sval(t) => Some((l, t))
        | _ => None
        },
      comps,
    );
  {type_comps, val_comps};
};

/* Inspect a term's module shape without minting anything. Some(shape) iff the
   term type-checks to a Sig (k = 0 included — opening a fully-manifest module
   is a pure namespace convenience). */
let inspect = (st: Store.t, node: Node.t): option(shape) => {
  let env = Store.build_env(st);
  switch (Typecheck.synth_top(env, [], node)) {
  | Error(_) => None
  | Ok(ty) =>
    switch (Store.find(st, ty)) {
    | Some(Definition.Type(Tnode.Sig(comps))) => Some(shape_of_sig(comps))
    | _ => None
    }
  };
};

type opened = {
  /* (label, type hash) for every type component of the OPENED sig: a fresh
     Abstract for each opaque, the (Abstract-substituted) equation for each
     manifest — the translucency payoff, bindable as `N.u` */
  type_bindings: list((Hash.t, Hash.t)),
  module_hash: Hash.t, /* the Open node — the module value, fully manifest */
  fields: list((Hash.t, Hash.t)) /* (label, projection term hash) */,
};

/* Generatively open: mint one Abstract per opaque component (rank order),
   ingest the single n-ary `Open`, then project each value member by label. */
let open_package =
    (st: Store.t, mint_src: Mint.source, pkg: Node.t): result(opened, string) => {
  let env = Store.build_env(st);
  switch (Typecheck.synth_top(env, [], pkg)) {
  | Error(m) => Error(m)
  | Ok(ty) =>
    switch (Store.find(st, ty)) {
    | Some(Definition.Type(Tnode.Sig(comps))) =>
      let k = List.length(Tnode.opaque_ranks(comps));
      let mints = List.init(k, _ => Mint.fresh(mint_src));
      switch (Store.ingest_term(st, Node.Open({pkg, mints}))) {
      | Error(e) => Error(e)
      | Ok(oh) =>
        /* the Open node's type is the sig with every component manifest */
        switch (Store.type_of(st, oh)) {
        | Some(oty) =>
          switch (Store.find(st, oty)) {
          | Some(Definition.Type(Tnode.Sig(comps'))) =>
            let type_bindings =
              List.filter_map(
                ((l, c)) =>
                  switch (c) {
                  | Tnode.Smanifest(h) => Some((l, h))
                  | _ => None
                  },
                comps',
              );
            let fields =
              List.filter_map(
                ((l, c)) =>
                  switch (c) {
                  | Tnode.Sval(_) =>
                    switch (
                      Store.ingest_term(
                        st,
                        Node.Project_field(Node.Ref(oh), l),
                      )
                    ) {
                    | Ok(fh) => Some((l, fh))
                    | Error(_) => None
                    }
                  | _ => None
                  },
                comps',
              );
            Ok({type_bindings, module_hash: oh, fields});
          | _ => Error("open: opened type is not a sig")
          }
        | None => Error("open: no cached type for the opened module")
        }
      };
    | _ => Error("not a module (its type is not a sig)")
    }
  };
};
