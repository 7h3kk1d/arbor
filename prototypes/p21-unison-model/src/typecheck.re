/* Typing, over RESOLVED terms. (def:utyping)
 *
 * Syntax-directed, no inference. A context maps variables to types; a signature
 * maps references to types -- given for builtins, read off the store for
 * derived references.
 *
 * A term with a free variable is not typeable, which is how the names that
 * resolution deliberately left free (rem:staging) become errors: at
 * typechecking, with a type to report, rather than at resolution.
 *
 * Typing a reference costs nothing -- the signature is consulted and no
 * derivation about the target is required. That is affordable precisely because
 * the type is inside the hash (rem:uref-delta): a stored definition's recorded
 * type cannot disagree with the definition, since changing the type would
 * change the hash and produce a different definition. */

exception Type_error(string);

/* The signature. `derived` is a partial map because a reference into a
   component being ingested does not exist yet. */
type env = {
  builtin: string => option(Ty.t),
  derived: Reference.t => option(Ty.t),
};

let rec synth = (env: env, ctx: list((string, Ty.t)), tm: Term.t): Ty.t =>
  switch (tm) {
  /* (U-Var) */
  | Term.Var(x) =>
    switch (List.assoc_opt(x, ctx)) {
    | Some(t) => t
    | None => raise(Type_error("unbound: " ++ x))
    }
  /* (U-Nat) (U-Text) */
  | Term.Nat(_) => Ty.Nat
  | Term.Text(_) => Ty.Text
  /* (U-Ref) */
  | Term.Ref(Reference.Builtin(b) as r) =>
    switch (env.builtin(b)) {
    | Some(t) => t
    | None =>
      switch (env.derived(r)) {
      | Some(t) => t
      | None => raise(Type_error("unknown builtin: " ++ b))
      }
    }
  | Term.Ref(r) =>
    switch (env.derived(r)) {
    | Some(t) => t
    | None => raise(Type_error("unknown reference: " ++ Reference.to_string(r)))
    }
  /* (U-Abs). The binder's type has to come from somewhere; with no inference,
     an unannotated lambda is only checkable, never synthesizable. */
  | Term.Lam(_, _) =>
    raise(Type_error("cannot infer a type for a lambda; annotate it"))
  /* (U-App) */
  | Term.App(f, a) =>
    switch (synth(env, ctx, f)) {
    | Ty.Arrow(t, u) =>
      check(env, ctx, a, t);
      u;
    | other =>
      raise(
        Type_error("applied a non-function of type " ++ Ty.to_string(other)),
      )
    }
  /* (U-Ann) */
  | Term.Ann(e, t) =>
    check(env, ctx, e, t);
    t;
  }

and check = (env: env, ctx: list((string, Ty.t)), tm: Term.t, expected: Ty.t): unit =>
  switch (tm, expected) {
  | (Term.Lam(x, b), Ty.Arrow(t, u)) => check(env, [(x, t), ...ctx], b, u)
  | (Term.Lam(_, _), other) =>
    raise(Type_error("a lambda cannot have type " ++ Ty.to_string(other)))
  | _ =>
    let got = synth(env, ctx, tm);
    if (!Ty.equal(got, expected)) {
      raise(
        Type_error(
          "expected " ++ Ty.to_string(expected) ++ ", got " ++ Ty.to_string(got),
        ),
      );
    };
  };

let synth_top = (env: env, tm: Term.t): result(Ty.t, string) =>
  try(Ok(synth(env, [], tm))) {
  | Type_error(m) => Error(m)
  };

let check_top = (env: env, tm: Term.t, expected: Ty.t): result(unit, string) =>
  try(Ok(check(env, [], tm, expected))) {
  | Type_error(m) => Error(m)
  };
