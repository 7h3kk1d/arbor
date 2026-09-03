/* Call-by-value evaluation, with a step budget.
 *
 * The language is simply typed and has no recursive term former, so a single
 * definition cannot diverge -- but a mutually recursive COMPONENT can, since
 * its members reference each other through the store. The budget is what makes
 * evaluating an arbitrary stored reference safe.
 *
 * A result is written into the cache against the identity of the expression
 * that produced it. A budget-exhausted result is deliberately NOT cached: a
 * later call with a larger budget may produce an answer, and caching the
 * failure would make the cache depend on something other than the hash. */

type value =
  | VNat(int)
  | VText(string)
  | VClosure(list((string, value)), string, Term.t)
  | VBuiltin(string, list(value)); /* partially applied primitive */

exception Stuck(string);
exception Out_of_fuel;

let arity = (b: string): int =>
  switch (Builtins.type_of(b)) {
  | None => 0
  | Some(t) =>
    let rec go = t =>
      switch (t) {
      | Ty.Arrow(_, u) => 1 + go(u)
      | _ => 0
      };
    go(t);
  };

let apply_builtin = (b: string, args: list(value)): value =>
  switch (b, args) {
  | ("Nat.+", [VNat(x), VNat(y)]) => VNat(x + y)
  | ("Nat.*", [VNat(x), VNat(y)]) => VNat(x * y)
  | ("Nat.drop", [VNat(x), VNat(y)]) => VNat(max(0, x - y))
  | ("Text.size", [VText(s)]) => VNat(String.length(s))
  | ("Text.++", [VText(a), VText(b)]) => VText(a ++ b)
  | ("Text.repeat", [VNat(n), VText(s)]) =>
    let buf = Buffer.create(String.length(s) * max(n, 0));
    for (_ in 1 to n) {
      Buffer.add_string(buf, s);
    };
    VText(Buffer.contents(buf));
  | _ => raise(Stuck("bad primitive application: " ++ b))
  };

let default_budget = 100000;

/* How many times the evaluator has actually run a term to completion.
 *
 * This exists so that "the cache was consulted rather than the expression
 * re-evaluated" is OBSERVABLE rather than merely timed. Unison's runtime
 * reports the same fact per watch as an `isHit` flag, and `loadUnisonFile`
 * writes the cache only `when (not isHit)`. A counter is the interface-level
 * equivalent: if it does not move, nothing ran. */
let evaluations = ref(0);

let reset_evaluations = () => evaluations := 0;

/* An overlay of terms addressable by reference but not (yet) in the store.
 * At save time the buffer's own definitions have hashes and no storage, so a
 * watch mentioning one of them can only be evaluated against an overlay --
 * which is why Unison hands the runtime a CodeLookup over the file. */
type overlay = list((Reference.t, Term.t));

let eval = (~overlay: overlay=[], st: Store.t, tm: Term.t): value => {
  incr(evaluations);
  let fuel = ref(default_budget);
  let tick = () => {
    decr(fuel);
    if (fuel^ <= 0) {
      raise(Out_of_fuel);
    };
  };
  let rec go = (env: list((string, value)), tm: Term.t): value => {
    tick();
    switch (tm) {
    | Term.Nat(n) => VNat(n)
    | Term.Text(s) => VText(s)
    | Term.Var(x) =>
      switch (List.assoc_opt(x, env)) {
      | Some(v) => v
      | None => raise(Stuck("unbound at runtime: " ++ x))
      }
    | Term.Lam(x, b) => VClosure(env, x, b)
    | Term.Ann(e, _) => go(env, e)
    | Term.Ref(Reference.Builtin(b)) =>
      arity(b) == 0 ? raise(Stuck("nullary builtin: " ++ b)) : VBuiltin(b, [])
    | Term.Ref(r) =>
      switch (List.find_opt(((r2, _)) => Reference.equal(r, r2), overlay)) {
      | Some((_, body)) => go([], body)
      | None =>
        switch (Store.lookup(st, r)) {
        | None => raise(Stuck("dangling reference: " ++ Reference.to_string(r)))
        | Some(body) => go([], body)
        }
      }
    | Term.App(f, a) =>
      let fv = go(env, f);
      let av = go(env, a);
      apply(fv, av);
    };
  }
  and apply = (fv: value, av: value): value =>
    switch (fv) {
    | VClosure(cenv, x, b) => go([(x, av), ...cenv], b)
    | VBuiltin(b, acc) =>
      let acc = acc @ [av];
      List.length(acc) >= arity(b) ? apply_builtin(b, acc) : VBuiltin(b, acc);
    | VNat(_)
    | VText(_) => raise(Stuck("applied a non-function"))
    };
  go([], tm);
};

let rec to_term = (v: value): Term.t =>
  switch (v) {
  | VNat(n) => Term.Nat(n)
  | VText(s) => Term.Text(s)
  | VClosure(_, x, b) => Term.Lam(x, b)
  | VBuiltin(b, args) =>
    List.fold_left((f, a) => Term.App(f, to_term(a)), Term.Ref(Reference.Builtin(b)), args)
  };

let to_string = (v: value): string =>
  switch (v) {
  | VNat(n) => string_of_int(n)
  | VText(s) => "\"" ++ String.escaped(s) ++ "\""
  | VClosure(_, x, _) => "<function \\" ++ x ++ ". ...>"
  | VBuiltin(b, _) => "<builtin " ++ b ++ ">"
  };

/* Evaluate a stored reference, consulting and populating the cache. Cache hits
   never have to be checked for staleness -- see watch.re. */
let eval_ref = (st: Store.t, w: Watch.t, r: Reference.t): result(Term.t, string) =>
  switch (Watch.get(w, r)) {
  | Some(v) => Ok(v)
  | None =>
    switch (Store.lookup(st, r)) {
    | None => Error("dangling reference: " ++ Reference.to_string(r))
    | Some(body) =>
      switch (
        try(Ok(eval(st, body))) {
        | Stuck(m) => Error("stuck: " ++ m)
        | Out_of_fuel => Error("step budget exhausted")
        }
      ) {
      | Error(m) => Error(m)
      | Ok(v) =>
        let t = to_term(v);
        Watch.put(w, r, t);
        Ok(t);
      }
    }
  };

let eval_top = (~overlay: overlay=[], st: Store.t, tm: Term.t): result(value, string) =>
  try(Ok(eval(~overlay, st, tm))) {
  | Stuck(m) => Error("stuck: " ++ m)
  | Out_of_fuel => Error("step budget exhausted")
  };

/* Evaluate a watch at its own reference, consulting the cache first and
 * reporting whether it was a HIT -- the flag Unison's runtime returns and its
 * save path branches on. `overlay` carries the buffer's own definitions, which
 * at save time exist nowhere else. */
let watch_at =
    (
      ~overlay: overlay=[],
      st: Store.t,
      w: Watch.t,
      r: Reference.t,
      body: Term.t,
    )
    : (result(Term.t, string), bool) =>
  switch (Watch.get(w, r)) {
  | Some(v) => (Ok(v), true)
  | None =>
    switch (eval_top(~overlay, st, body)) {
    | Error(m) => (Error(m), false)
    | Ok(v) =>
      let t = to_term(v);
      /* written only on a miss, as in loadUnisonFile */
      Watch.put(w, r, t);
      (Ok(t), false);
    }
  };
