/* Content-addressed, in-memory definition store. A Hashtbl from Hash.t
   to Definition.t; exiting the process drops all state. */

type t = Hashtbl.t(Hash.t, Definition.t);

let create = (): t => Hashtbl.create(64);

/* Register a definition. Returns its hash. Idempotent: registering an
   equal definition yields the same hash and leaves the table unchanged. */
let register = (store: t, d: Definition.t): Hash.t => {
  let h = Definition.hash(d);
  if (!Hashtbl.mem(store, h)) {
    Hashtbl.add(store, h, d);
  };
  h;
};

let lookup = (store: t, h: Hash.t): option(Definition.t) =>
  Hashtbl.find_opt(store, h);

let hashes = (store: t): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], store, []);

let entries = (store: t): list((Hash.t, Definition.t)) =>
  Hashtbl.fold((h, d, acc) => [(h, d), ...acc], store, []);

let resolve_prefix = (store: t, prefix: string): Hash.lookup_result =>
  Hash.lookup_by_prefix(prefix, hashes(store));
