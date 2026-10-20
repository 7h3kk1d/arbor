/* Call-by-value evaluation of an open term under a link, with a step budget.
 *
 * The language has no recursive term former, so a single definition cannot
 * diverge -- but a bundle can, since its members reach each other through the
 * store. The budget is what makes evaluating an arbitrary stored definition
 * safe, and it is the same budget p21 has, for the same reason.
 *
 * `Slot(i)` is where evaluation differs from p21. There, a term carries the
 * referent's hash and evaluation follows it. Here the term carries an index and
 * the LINK says what it points at, so the link is an explicit parameter all the
 * way down. Evaluating the same open term under two links gives two answers,
 * which is the design: an open term does not denote anything by itself.
 *
 * A budget-exhausted result is deliberately not cached -- a later call with a
 * larger budget may succeed, and caching the failure would make the cache
 * depend on something other than content. */

type value =
  | VNat(int)
  | VText(string)
  | VClosure(list(value), Link.t, Open_term.t) /* de Bruijn env, its link, body */
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

/* How many times the evaluator has actually run something to completion, so
   that "the cache answered" is observable rather than merely fast. p21 carries
   the same counter for the same reason. */
let evaluations = ref(0);
let reset_evaluations = () => evaluations := 0;

/* Definitions addressable but not yet in the store -- the buffer's own, before
   anything is committed. */
type overlay = list((Hash.t, Def.t));

let eval =
    (~overlay: overlay=[], ~budget=default_budget, st: Store.t, link: Link.t, tm: Open_term.t)
    : value => {
  incr(evaluations);
  let fuel = ref(budget);
  let tick = () => {
    decr(fuel);
    if (fuel^ <= 0) {
      raise(Out_of_fuel);
    };
  };
  let resolve_def = (h: Hash.t): option(Def.t) =>
    switch (List.assoc_opt(h, overlay)) {
    | Some(d) => Some(d)
    | None => Store.get_def(st, h)
    };
  let rec go = (env: list(value), link: Link.t, tm: Open_term.t): value => {
    tick();
    switch (tm) {
    | Open_term.Nat(n) => VNat(n)
    | Open_term.Text(s) => VText(s)
    | Open_term.Bound(i) =>
      switch (List.nth_opt(env, i)) {
      | Some(v) => v
      | None => raise(Stuck("unbound de Bruijn index " ++ string_of_int(i)))
      }
    | Open_term.Slot(i) => slot(link, i)
    | Open_term.Lam(b) => VClosure(env, link, b)
    | Open_term.Ann(e, _) => go(env, link, e)
    | Open_term.App(f, a) =>
      let fv = go(env, link, f);
      let av = go(env, link, a);
      apply(fv, av);
    };
  }
  /* A slot is evaluated by following the link to a definition and evaluating
     THAT definition under ITS link. This is the only place the two halves are
     recombined at runtime. */
  and slot = (link: Link.t, i: int): value =>
    if (i >= Array.length(link.Link.targets)) {
      raise(Stuck("slot " ++ string_of_int(i) ++ " is outside the telescope"));
    } else {
      switch (link.Link.targets[i]) {
      | Link.Unbound =>
        raise(Stuck("slot " ++ string_of_int(i) ++ " is not linked"))
      | Link.Builtin(b) =>
        arity(b) == 0 ? apply_builtin(b, []) : VBuiltin(b, [])
      | Link.Self(j) =>
        raise(Stuck("unclosed recursive reference to member " ++ string_of_int(j)))
      | Link.Def(h) =>
        switch (resolve_def(h)) {
        | None => raise(Stuck("no such definition: " ++ Hash.short(h)))
        | Some(d) =>
          switch (open_and_link(d)) {
          | None => raise(Stuck("definition is not fully stored: " ++ Hash.short(h)))
          | Some((o, l)) => go([], l, o)
          }
        }
      };
    }
  and open_and_link = (d: Def.t): option((Open_term.t, Link.t)) =>
    switch (Store.open_of(st, d), Store.link_of(st, d)) {
    | (Some(o), Some(l)) => Some((o, l))
    | _ => None
    }
  and apply = (f: value, a: value): value =>
    switch (f) {
    | VClosure(env, l, body) => go([a, ...env], l, body)
    | VBuiltin(b, args) =>
      let args' = args @ [a];
      List.length(args') >= arity(b)
        ? apply_builtin(b, args') : VBuiltin(b, args');
    | VNat(_)
    | VText(_) => raise(Stuck("applied a non-function"))
    };
  go([], link, tm);
};

/* Evaluate a stored definition. */
let eval_def =
    (~overlay: overlay=[], ~budget=default_budget, st: Store.t, d: Def.t)
    : value =>
  switch (Store.open_of(st, d), Store.link_of(st, d)) {
  | (Some(o), Some(l)) => eval(~overlay, ~budget, st, l, o)
  | _ => raise(Stuck("definition is not fully stored"))
  };

let rec to_string = (v: value): string =>
  switch (v) {
  | VNat(n) => string_of_int(n)
  | VText(s) => "\"" ++ String.concat("\\\"", String.split_on_char('"', s)) ++ "\""
  | VClosure(_, _, _) => "<function>"
  | VBuiltin(b, []) => b
  | VBuiltin(b, args) =>
    b ++ "(" ++ String.concat(", ", List.map(to_string, args)) ++ ")"
  };
