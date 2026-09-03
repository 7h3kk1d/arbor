/* The namespace: a RELATION. (def:unames)
 *
 *   N  subset of  Name x Referent
 *
 * One name may denote several referents and one referent may have several
 * names; both are ordinary states rather than errors. Making conflict a legal
 * state rather than an error at bind time is what lets a merge complete without
 * resolving everything first -- the namespace can hold both candidates and the
 * user sorts it out afterwards (rem:conflict).
 *
 * This is the opposite of arbor's own position, which is that N is a partial
 * function and multiplicity belongs to a FAMILY of namespaces. p21 implements
 * Unison's, deliberately, and the cost is visible below: unconflictedness is a
 * derived predicate that half the operations have to demand, and it is
 * ALL-OR-NOTHING -- there is no way to take the unconflicted part of a
 * namespace, only to discover that the whole thing is or is not (def:unconf).
 *
 * Names here are atomic. Suffix resolution and the lib-depth priority rule are
 * not modeled. */

type t = {mutable rows: list((string, Reference.referent))};

let create = (): t => {rows: []};

let of_rows = (rows): t => {rows: rows};

let rows = (n: t): list((string, Reference.referent)) =>
  List.sort(
    ((a, x), (b, y)) => {
      let c = String.compare(a, b);
      c != 0 ? c : Reference.referent_compare(x, y);
    },
    n.rows,
  );

/* N(x) -- the set of referents x denotes. */
let lookup = (n: t, name: string): list(Reference.referent) =>
  List.filter_map(
    ((k, v)) => String.equal(k, name) ? Some(v) : None,
    n.rows,
  );

/* Names of a referent. One referent may have several. */
let names_of = (n: t, r: Reference.referent): list(string) =>
  List.filter_map(
    ((k, v)) => Reference.referent_equal(v, r) ? Some(k) : None,
    n.rows,
  )
  |> List.sort_uniq(String.compare);

let names = (n: t): list(string) =>
  List.map(fst, n.rows) |> List.sort_uniq(String.compare);

let add = (n: t, name: string, r: Reference.referent): unit =>
  if (!List.exists(
        ((k, v)) => String.equal(k, name) && Reference.referent_equal(v, r),
        n.rows,
      )) {
    n.rows = [(name, r), ...n.rows];
  };

/* Remove every row for a name. This is the only kind of deletion the system
   supports: the store keeps the definition, it just stops being reachable. */
let remove_name = (n: t, name: string): unit =>
  n.rows = List.filter(((k, _)) => !String.equal(k, name), n.rows);

let remove_names = (n: t, ns: list(string)): unit =>
  n.rows = List.filter(((k, _)) => !List.mem(k, ns), n.rows);

/* Rebind: remove then add. Renaming a definition does not change it, because
   its identity never mentioned the name. */
let rebind = (n: t, name: string, r: Reference.referent): unit => {
  remove_name(n, name);
  add(n, name, r);
};

let copy = (n: t): t => {rows: n.rows};

/* (def:unconf) unconflicted(N, x) iff |N(x)| = 1. */
let unconflicted_at = (n: t, name: string): bool =>
  List.length(lookup(n, name)) == 1;

/* All-or-nothing, matching the implementation: either the whole namespace as a
   partial function, or the offending name. There is deliberately no
   "unconflicted part of N" -- that absence is what makes def:uscope's scope a
   precondition rather than a filter. */
let as_unconflicted = (n: t): result(list((string, Reference.referent)), string) => {
  let ns = names(n);
  switch (List.find_opt(name => !unconflicted_at(n, name), ns)) {
  | Some(bad) => Error(bad)
  | None => Ok(List.map(name => (name, List.hd(lookup(n, name))), ns))
  };
};

let resolve = (n: t, name: string): option(Reference.referent) =>
  switch (lookup(n, name)) {
  | [r] => Some(r)
  | _ => None
  };

/* shadowing(a, b) -- left-biased union on the DOMAIN, not on the rows. A name
   bound in `a` wins over the same name in `b` outright, taking all of b's rows
   for that name with it. This one function is the whole of the scratch buffer's
   name-overlay semantics: everything you are editing is visible to everything
   else you are editing, with no gesture. */
let shadowing = (a: t, b: t): t => {
  let a_names = names(a);
  {rows: a.rows @ List.filter(((k, _)) => !List.mem(k, a_names), b.rows)};
};
