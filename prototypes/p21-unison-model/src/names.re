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

/* (def:gestures) The three operations that change N and nothing else.
 *
 *   move(x, y)   N' = (N \ {(x,r)}) u {(y,r)}
 *   alias(x, y)  N' = N u {(y,r)}
 *   delete(x)    N' = N \ ({x} x Referent)
 *
 * None reads or writes the Store, none can fail on a typing ground, and none
 * changes any hash. That is rem:renamefree -- a rename is one row, it cannot
 * break a caller and it cannot invalidate a cached result (cor:wnoinval),
 * because no stored definition contains a name to rewrite. Nothing in the
 * store, the cache or update can observe that one happened.
 *
 * TWO PRECONDITIONS THE PAPER'S SET EQUATIONS DO NOT SHOW, both verified
 * against ucm 1.1.1:
 *
 *   - The source name must be unconflicted. The equations write `r` for "the
 *     referent N binds at x", which presupposes it.
 *   - The target name must be free. The relation would hold the result of
 *     moving onto an occupied name perfectly well -- the union simply makes
 *     that name conflicted -- but the interface refuses: "A term named bar
 *     already exists." So conflict stays REPRESENTABLE while no gesture here
 *     can produce one. Third instance of rem:conflict's pattern, after
 *     def:unconf's all-or-nothing view and update's precondition.
 *
 * delete needs neither: it removes every row for the name, conflicted or not. */

type gesture_error =
  | Unbound(string) /* source name denotes nothing */
  | Ambiguous(string) /* source name denotes several referents */
  | Occupied(string); /* target name is already bound */

let gesture_error_to_string =
  fun
  | Unbound(x) => "I don't know about a term named " ++ x
  | Ambiguous(x) => "the name " ++ x ++ " is ambiguous"
  | Occupied(y) => "a term named " ++ y ++ " already exists";

let source_referent = (n: t, from_: string): result(Reference.referent, gesture_error) =>
  switch (lookup(n, from_)) {
  | [] => Error(Unbound(from_))
  | [r] => Ok(r)
  | _ => Error(Ambiguous(from_))
  };

let alias = (n: t, from_: string, to_: string): result(unit, gesture_error) =>
  switch (source_referent(n, from_)) {
  | Error(_) as e => e
  | Ok(r) =>
    List.mem(to_, names(n)) ? Error(Occupied(to_)) : Ok(add(n, to_, r))
  };

let move = (n: t, from_: string, to_: string): result(unit, gesture_error) =>
  switch (source_referent(n, from_)) {
  | Error(_) as e => e
  | Ok(r) =>
    if (List.mem(to_, names(n))) {
      Error(Occupied(to_));
    } else {
      remove_name(n, from_);
      add(n, to_, r);
      Ok();
    }
  };

/* Total: the store keeps the definition, it just stops being reachable. */
let delete = (n: t, name: string): unit => remove_name(n, name);
