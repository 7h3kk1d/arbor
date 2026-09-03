/* Name resolution: source terms -> resolved terms. (def:uresolution)
 *
 * Homomorphic except at variables:
 *
 *   R(x) = x          if x is locally bound, or bound by the group being ingested
 *        = Ref(r)     if N(x) = {r}
 *        = x          otherwise
 *
 * THREE OF THE FOUR CASES LEAVE A VARIABLE IN PLACE, and that is not an
 * omission (rem:staging).
 *
 * A name bound by the group being ingested CANNOT be resolved -- its referent
 * does not exist yet, and for a recursive group it cannot exist until the group
 * is hashed. Resolution stops at the boundary of what is already stored, and
 * ingest discharges the rest.
 *
 * A name that resolves to nothing, or to more than one thing, is left free ON
 * PURPOSE. The implementation's comments say why: "not found: leave free for
 * telling user about expected type" and "ambiguous: leave free for TDNR". An
 * unresolved name survives into typechecking, where the expected type may
 * produce a better error than resolution could. Failure to resolve is not an
 * error in itself; a free variable simply fails to type. */

/* What resolution DECIDED about one variable occurrence. The whole point of
   making this a datatype rather than an internal branch is that four of the five
   outcomes are invisible in the output term -- `Group`, `Unresolved` and
   `Ambiguous` all produce the same `Var(x)` node, and only the trace can tell
   them apart. An editor that shows the user "this stayed a variable" without
   saying WHICH of the three reasons applies is hiding the interesting half of
   def:uresolution. */
type outcome =
  | Local /* a lambda binder in scope: an ordinary bound variable */
  | Group /* bound by the group being ingested: cannot resolve YET */
  | Builtin(string) /* a primitive, named extrinsically (rem:builtin) */
  | Resolved(Reference.t) /* N(x) = {r} */
  | Unresolved /* N(x) = {}: left free on purpose */
  | Ambiguous(int); /* |N(x)| > 1: also left free, also on purpose */

type decision = {
  name: string,
  outcome,
};

let outcome_label = (o: outcome): string =>
  switch (o) {
  | Local => "local"
  | Group => "in this buffer"
  | Builtin(_) => "builtin"
  | Resolved(_) => "resolved"
  | Unresolved => "not found"
  | Ambiguous(n) => "ambiguous (" ++ string_of_int(n) ++ ")"
  };

/* Does this outcome leave a variable in the output term? Three of the five do,
   and that is rem:staging's point. */
let stays_free = (o: outcome): bool =>
  switch (o) {
  | Group
  | Unresolved
  | Ambiguous(_) => true
  | Local
  | Builtin(_)
  | Resolved(_) => false
  };

/* Is this outcome a problem? Only two are, and neither is a resolution error --
   they become type errors later, with a type to report. */
let is_trouble = (o: outcome): bool =>
  switch (o) {
  | Unresolved
  | Ambiguous(_) => true
  | _ => false
  };

let decide =
    (~group: list(string), ~local: list(string), ~ns: Names.t, x: string)
    : outcome =>
  if (List.mem(x, local)) {
    Local;
  } else if (List.mem(x, group)) {
    Group;
  } else if (Builtins.exists(x)) {
    Builtin(x);
  } else {
    switch (Names.lookup(ns, x)) {
    | [r] => Resolved(Reference.referent_ref(r))
    | [] => Unresolved
    | rs => Ambiguous(List.length(rs))
    };
  };

let apply = (x: string, o: outcome): Term.t =>
  switch (o) {
  | Builtin(b) => Term.Ref(Reference.Builtin(b))
  | Resolved(r) => Term.Ref(r)
  | Local
  | Group
  | Unresolved
  | Ambiguous(_) => Term.Var(x)
  };

let rec go =
        (
          ~trace: ref(list(decision)),
          ~group: list(string), /* B: names bound by the incoming group */
          ~local: list(string), /* lambda binders in scope */
          ~ns: Names.t,
          e: Term.Source.t,
        )
        : Term.t =>
  switch (e) {
  | Term.Source.Var(x) =>
    let o = decide(~group, ~local, ~ns, x);
    trace := [{name: x, outcome: o}, ...trace^];
    apply(x, o);
  | Term.Source.Lam(x, b) =>
    Term.Lam(x, go(~trace, ~group, ~local=[x, ...local], ~ns, b))
  | Term.Source.App(f, a) =>
    Term.App(
      go(~trace, ~group, ~local, ~ns, f),
      go(~trace, ~group, ~local, ~ns, a),
    )
  | Term.Source.Nat(n) => Term.Nat(n)
  | Term.Source.Text(s) => Term.Text(s)
  | Term.Source.Ann(e, t) => Term.Ann(go(~trace, ~group, ~local, ~ns, e), t)
  };

let term = (~group=[], ~ns: Names.t, e: Term.Source.t): Term.t => {
  let trace = ref([]);
  go(~trace, ~group, ~local=[], ~ns, e);
};

/* The same resolution, with its decisions. Occurrences are returned in source
   order; a name mentioned twice appears twice, because the two occurrences can
   genuinely decide differently (a binder may shadow between them). */
let term_traced =
    (~group=[], ~ns: Names.t, e: Term.Source.t): (Term.t, list(decision)) => {
  let trace = ref([]);
  let t = go(~trace, ~group, ~local=[], ~ns, e);
  (t, List.rev(trace^));
};
