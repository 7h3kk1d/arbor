/* Typing, over an open term together with a link.
 *
 * Syntax-directed, no inference, exactly p21's rules -- the object language is
 * unchanged, which is what makes p21 a usable control group. What changes is
 * where the type of a mentioned definition comes from.
 *
 * In p21 a reference carries the referent's hash, so typing it is a lookup with
 * nothing to decide. Here the term says only `Slot(i)`, and the link says what
 * slot i points at. So typing is parameterized by the link, and the SAME open
 * term has different types under different links -- which is not a defect but
 * the content of the design: an open term is a shape, and a shape is not typed
 * until you say what it is over.
 *
 * Two consequences worth naming:
 *
 *   - `Ty` is an aspect of the DEFINITION (open + link), never of the open term
 *     alone. Store.types is keyed by definition hash for that reason.
 *
 *   - An unbound slot is a type error with a position and a name to report, not
 *     a parse failure and not a refusal to store. The term was stored before
 *     anyone tried to type it. This is `feedback: visible breakage, not gates`
 *     at the layer where p21 has its commit gate. */

exception Type_error(string);

/* The signature. `def` is partial because a slot may point at a definition that
   does not exist yet, and `self` supplies the declared types of the members of
   a recursive group being checked -- a group has to be typed against its own
   signatures, since its members' types cannot be inferred from each other. */
type env = {
  builtin: string => option(Ty.t),
  def: Hash.t => option(Ty.t),
  self: int => option(Ty.t),
};

let env_of_store = (~self=_ => None, st: Store.t): env => {
  builtin: Builtins.type_of,
  def: h => Store.type_of(st, h),
  self,
};

let slot_ty = (env: env, link: Link.t, i: int): Ty.t =>
  if (i >= Array.length(link.Link.targets)) {
    raise(Type_error("slot " ++ string_of_int(i) ++ " is outside the telescope"));
  } else {
    switch (link.Link.targets[i]) {
    | Link.Unbound =>
      raise(Type_error("slot " ++ string_of_int(i) ++ " is not linked"))
    | Link.Builtin(b) =>
      switch (env.builtin(b)) {
      | Some(t) => t
      | None => raise(Type_error("unknown builtin: " ++ b))
      }
    | Link.Def(h) =>
      switch (env.def(h)) {
      | Some(t) => t
      | None =>
        raise(Type_error("slot " ++ string_of_int(i) ++ " points at an unknown definition " ++ Hash.short(h)))
      }
    | Link.Self(j) =>
      switch (env.self(j)) {
      | Some(t) => t
      | None =>
        raise(
          Type_error(
            "recursive member " ++ string_of_int(j) ++ " has no declared type",
          ),
        )
      }
    };
  };

/* ctx is the de Bruijn context: innermost binder first, so List.nth IS the
   index. p21 threaded an association list of names; there are no names here. */
let rec synth =
        (env: env, link: Link.t, ctx: list(Ty.t), tm: Open_term.t): Ty.t =>
  switch (tm) {
  | Open_term.Bound(i) =>
    switch (List.nth_opt(ctx, i)) {
    | Some(t) => t
    | None => raise(Type_error("unbound de Bruijn index " ++ string_of_int(i)))
    }
  | Open_term.Slot(i) => slot_ty(env, link, i)
  | Open_term.Nat(_) => Ty.Nat
  | Open_term.Text(_) => Ty.Text
  | Open_term.Lam(_) =>
    raise(Type_error("cannot infer a type for a lambda; annotate it"))
  | Open_term.App(f, a) =>
    switch (synth(env, link, ctx, f)) {
    | Ty.Arrow(t, u) =>
      check(env, link, ctx, a, t);
      u;
    | other =>
      raise(Type_error("applied a non-function of type " ++ Ty.to_string(other)))
    }
  | Open_term.Ann(e, t) =>
    check(env, link, ctx, e, t);
    t;
  }

and check =
    (env: env, link: Link.t, ctx: list(Ty.t), tm: Open_term.t, expected: Ty.t)
    : unit =>
  switch (tm, expected) {
  | (Open_term.Lam(b), Ty.Arrow(t, u)) => check(env, link, [t, ...ctx], b, u)
  | (Open_term.Lam(_), other) =>
    raise(Type_error("a lambda cannot have type " ++ Ty.to_string(other)))
  | _ =>
    let got = synth(env, link, ctx, tm);
    if (!Ty.equal(got, expected)) {
      raise(
        Type_error(
          "expected " ++ Ty.to_string(expected) ++ ", got " ++ Ty.to_string(got),
        ),
      );
    };
  };

let synth_top = (env: env, link: Link.t, tm: Open_term.t): result(Ty.t, string) =>
  try(Ok(synth(env, link, [], tm))) {
  | Type_error(m) => Error(m)
  };

let check_top =
    (env: env, link: Link.t, tm: Open_term.t, expected: Ty.t)
    : result(unit, string) =>
  try(Ok(check(env, link, [], tm, expected))) {
  | Type_error(m) => Error(m)
  };

/* Type a definition already in the store. */
let type_def = (st: Store.t, d: Def.t): result(Ty.t, string) =>
  switch (Store.open_of(st, d), Store.link_of(st, d)) {
  | (Some(o), Some(l)) => synth_top(env_of_store(st), l, o)
  | _ => Error("definition is not fully stored")
  };
