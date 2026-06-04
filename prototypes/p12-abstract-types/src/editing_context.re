/* The editing context (design/12 §"Unbundled abstract types"): ephemeral editor
   state carrying an *open set* of abstract types treated as transparent here.
   The default context opens nothing — that is where ordinary consumers are
   written and abstract types stay opaque. Two gestures create opening contexts:
   creating abstract type(s) and opening an existing one to add operations.

   This is where opacity lives: the substrate enforces nothing, but `commit`
   only ever opens what the context opened, and a draft is sealed only when it
   genuinely needs the unfold (minimal sealing). */

type t = {mutable opens: list(Hash.t)};

let make = (): t => {opens: []};

let open_type = (ctx: t, h: Hash.t): unit =>
  if (!List.mem(h, ctx.opens)) {
    ctx.opens = [h, ...ctx.opens];
  };

let opens = (ctx: t): list(Hash.t) => ctx.opens;

type kind = [ | `Normal | `Sealed];

/* Commit a draft (already resolved to a Node.t) under an external-type
   annotation `ann` (a type hash that may mention opaques). Minimal sealing:

   1. If it type-checks with NOTHING open and matches `ann` -> ordinary term.
   2. Else if it type-checks with the context's opens and matches `ann` ->
      normalize the impl (witness-substitute opened opaques) and wrap in a Seal.
   3. Else -> error. */
let commit =
    (st: Store.t, ctx: t, ~term: Node.t, ~ann: Hash.t)
    : result((Hash.t, kind), string) => {
  let env = Store.build_env(st);
  switch (Typecheck.synth_top(env, [], term)) {
  | Ok(t0) when Typecheck.equal_ty(env, [], t0, ann) =>
    switch (Store.ingest_term(st, term)) {
    | Ok(h) => Ok((h, `Normal))
    | Error(m) => Error(m)
    }
  | _ =>
    switch (Typecheck.synth_top(env, ctx.opens, term)) {
    | Error(m) => Error("does not type-check even with opened types: " ++ m)
    | Ok(t1) when !Typecheck.equal_ty(env, ctx.opens, t1, ann) =>
      Error("annotated external type does not match under opened witnesses")
    | Ok(_) =>
      let impl = Typecheck.normalize_term(env, ctx.opens, term);
      switch (Store.ingest_term(st, impl)) {
      | Error(m) => Error("normalized impl failed to ingest: " ++ m)
      | Ok(impl_h) =>
        let seal = Node.Seal({opens: ctx.opens, ty: ann, impl: impl_h});
        switch (Store.ingest_term(st, seal)) {
        | Ok(seal_h) => Ok((seal_h, `Sealed))
        | Error(m) => Error("seal failed to ingest: " ++ m)
        };
      }
    }
  };
};
