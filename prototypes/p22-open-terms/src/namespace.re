/* The namespace — a single-valued map from name to definition hash.
 *
 * Single-valued, per 13-editing-context.md:135 (2026-08-20): "N stays
 * single-valued. A name denotes one hash. Where multiplicity is needed it
 * belongs to a family of namespaces ... not to a family of targets inside one
 * namespace."
 *
 * p21 implements the opposite, because Unison does: there a name maps to a SET
 * of referents, conflict is a representable state, and `unconflicted` is a
 * derived predicate that half the operations then demand. p21's finding was
 * that merge -- the motivating case for the relation -- never puts a conflict
 * into the namespace either, so the representable state goes unused by the one
 * operation it exists for.
 *
 * ALIASES ARE FREE AND CANNOT AFFECT ANYTHING.
 *
 * Several names may denote one hash; that is an alias, and it is ordinary. What
 * is different here from p21 is that an alias cannot change a result. In Unison
 * an alias can defeat propagation outright, and WHICH alias you update decides
 * whether propagation happens, because propagation renders dependents through a
 * pretty-print environment that picks one name per hash. Nothing in p22 asks
 * the namespace which name to use for a hash, because nothing renders. The
 * namespace is consulted at bind time, to turn a name the author wrote into a
 * target, and never again. */

type t = {
  bindings: Hashtbl.t(string, Hash.t),
  reverse: Hashtbl.t(Hash.t, list(string)),
};

let create = (): t => {
  bindings: Hashtbl.create(64),
  reverse: Hashtbl.create(64),
};

let resolve = (ns: t, name: string): option(Hash.t) =>
  Hashtbl.find_opt(ns.bindings, name);

let names_of = (ns: t, h: Hash.t): list(string) =>
  Option.value(~default=[], Hashtbl.find_opt(ns.reverse, h));

let unbind = (ns: t, name: string): unit =>
  switch (Hashtbl.find_opt(ns.bindings, name)) {
  | None => ()
  | Some(h) =>
    Hashtbl.remove(ns.bindings, name);
    let remaining =
      List.filter(n => !String.equal(n, name), names_of(ns, h));
    remaining == []
      ? Hashtbl.remove(ns.reverse, h)
      : Hashtbl.replace(ns.reverse, h, remaining);
  };

/* Bind, replacing any existing binding for the name. Single-valued, so this is
   the only way a name changes meaning. */
let bind = (ns: t, name: string, h: Hash.t): unit => {
  unbind(ns, name);
  Hashtbl.replace(ns.bindings, name, h);
  let prior = names_of(ns, h);
  if (!List.mem(name, prior)) {
    Hashtbl.replace(ns.reverse, h, prior @ [name]);
  };
};

/* An alias is a second name on one hash. Nothing about it is special. */
let alias = (ns: t, ~existing: string, ~new_name: string): result(unit, string) =>
  switch (resolve(ns, existing)) {
  | None => Error("no such name: " ++ existing)
  | Some(h) =>
    bind(ns, new_name, h);
    Ok();
  };

let all = (ns: t): list((string, Hash.t)) =>
  Hashtbl.fold((n, h, acc) => [(n, h), ...acc], ns.bindings, [])
  |> List.sort(((a, _), (b, _)) => String.compare(a, b));

let size = (ns: t): int => Hashtbl.length(ns.bindings);
