/* The evaluation cache.
 *
 * Keyed by definition hash, valued by the result. No invalidation function
 * exists here, and that is the point rather than an omission: the key is the
 * identity of exactly the thing that was evaluated, and that identity covers
 * the open term AND the link AND, transitively, everything the link reaches.
 * So a stale entry is unreachable rather than wrong. Staleness is
 * unrepresentable. (p21 carries the same argument as cor:wnoinval, inherited
 * from Unison, where the absence of invalidation logic is verified by absence.)
 *
 * What changes from p21 is which edits miss the cache, and this is worth
 * stating because it is a consequence of the open/link split rather than of
 * anything in this file:
 *
 *   - Reformatting or renaming a binder: same open term, same link, same
 *     definition hash. HIT. (p21 stores binder names, so a binder rename is a
 *     new hash and a miss.)
 *   - Renaming a dependency: same link targets -- names are not in the link --
 *     so same hash. HIT.
 *   - Editing the body: new open term, new definition hash. MISS, correctly.
 *   - A dependency actually changing: new link, new definition hash. MISS,
 *     correctly, and only for the definitions downstream of it. */

type t = {entries: Hashtbl.t(Hash.t, Eval.value)};

let create = (): t => {entries: Hashtbl.create(64)};

let get = (w: t, h: Hash.t): option(Eval.value) =>
  Hashtbl.find_opt(w.entries, h);

let put = (w: t, h: Hash.t, v: Eval.value): unit =>
  Hashtbl.replace(w.entries, h, v);

let mem = (w: t, h: Hash.t): bool => Hashtbl.mem(w.entries, h);

let size = (w: t): int => Hashtbl.length(w.entries);

let entries = (w: t): list((Hash.t, Eval.value)) =>
  Hashtbl.fold((h, v, acc) => [(h, v), ...acc], w.entries, []);

/* Evaluate a definition, consulting the cache first. The evaluation counter in
   Eval is what makes a hit observable: if it does not move, nothing ran. */
let eval_cached =
    (~overlay: Eval.overlay=[], w: t, st: Store.t, d: Def.t)
    : result(Eval.value, string) => {
  let h = Def.hash(d);
  switch (get(w, h)) {
  | Some(v) => Ok(v)
  | None =>
    switch (Eval.eval_def(~overlay, st, d)) {
    | v =>
      put(w, h, v);
      Ok(v);
    | exception (Eval.Stuck(m)) => Error(m)
    | exception Eval.Out_of_fuel => Error("evaluation ran out of fuel")
    }
  };
};
