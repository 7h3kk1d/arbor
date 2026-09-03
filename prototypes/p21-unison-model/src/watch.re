/* The evaluation cache. (def:watch)
 *
 *   W : (Hash x Pos) -> Term
 *
 * Two things about the shape are easy to get wrong. The key is a REFERENCE, not
 * a bare hash -- a cached expression is addressed exactly the way a definition
 * is. And the value is the evaluated result TERM, held inline; caching a result
 * does not put the result in the store.
 *
 * The key need not be a definition. There is no "is a definition" status
 * attached to stored data that the cache would have to respect: a hash is a
 * hash (rem:watchkey). That is what lets `> 2 + 3` in a scratch buffer be
 * cached on the same footing as a top-level binding, without ever acquiring a
 * name.
 *
 * Cache soundness is stable under store growth (thm:wstable): an entry, once
 * written, is valid in every future store, because the store is append-only and
 * the key is a hash of exactly the expression that was evaluated. So no
 * invalidation is required (cor:wnoinval), and there is no invalidation
 * function here to call -- which is the point. Staleness is unrepresentable. Note that nothing in that argument
 * mentions names, types, or how often anyone edits anything. */

type t = {entries: Hashtbl.t((Hash.t, int), Term.t)};

let create = (): t => {entries: Hashtbl.create(64)};

let get = (w: t, r: Reference.t): option(Term.t) =>
  switch (r) {
  | Reference.Builtin(_) => None
  | Reference.Derived(h, i) => Hashtbl.find_opt(w.entries, (h, i))
  };

let put = (w: t, r: Reference.t, v: Term.t): unit =>
  switch (r) {
  | Reference.Builtin(_) => ()
  | Reference.Derived(h, i) => Hashtbl.replace(w.entries, (h, i), v)
  };

let size = (w: t): int => Hashtbl.length(w.entries);

let entries = (w: t): list((Reference.t, Term.t)) =>
  Hashtbl.fold(
    ((h, i), v, acc) => [(Reference.Derived(h, i), v), ...acc],
    w.entries,
    [],
  );
