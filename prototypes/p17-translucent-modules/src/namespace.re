/* Separate name -> hash table (design/04). Names resolve to hashes at edit time
   and never appear in stored programs. Type identity is independent of names.
   Dotted names are opaque strings; resolution here is exact-match (longest-suffix
   resolution from p9 is deferred — see open-questions). A reverse index supports
   name-aware pretty-printing. Keyword segments are reserved. */

exception Name_reserved(string);

let reserved: list(string) = [
  "let",
  "in",
  "if",
  "then",
  "else",
  "true",
  "false",
  "fst",
  "snd",
  "mul",
  "Int",
  "Bool",
];

let split_segments = (s: string): list(string) => String.split_on_char('.', s);

let is_reserved_name = (s: string): bool =>
  List.exists(seg => List.mem(seg, reserved), split_segments(s));

type t = {
  by_name: Hashtbl.t(string, Hash.t),
  by_hash: Hashtbl.t(Hash.t, list(string)),
};

let create = (): t => {
  by_name: Hashtbl.create(64),
  by_hash: Hashtbl.create(64),
};

let add_inverse = (ns: t, name: string, h: Hash.t): unit => {
  let cur =
    switch (Hashtbl.find_opt(ns.by_hash, h)) {
    | Some(l) => l
    | None => []
    };
  if (!List.mem(name, cur)) {
    Hashtbl.replace(ns.by_hash, h, [name, ...cur]);
  };
};

let remove_inverse = (ns: t, name: string, h: Hash.t): unit =>
  switch (Hashtbl.find_opt(ns.by_hash, h)) {
  | None => ()
  | Some(l) =>
    let l' = List.filter(n => n != name, l);
    if (l' == []) {
      Hashtbl.remove(ns.by_hash, h);
    } else {
      Hashtbl.replace(ns.by_hash, h, l');
    };
  };

let rebind = (ns: t, ~name: string, h: Hash.t): unit => {
  if (is_reserved_name(name)) {
    raise(Name_reserved(name));
  };
  switch (Hashtbl.find_opt(ns.by_name, name)) {
  | Some(old) => remove_inverse(ns, name, old)
  | None => ()
  };
  Hashtbl.replace(ns.by_name, name, h);
  add_inverse(ns, name, h);
};

/* Delete a name. The definition it pointed at stays in the store (content is
   immutable and name-free); only the editor-level name goes away. Other names
   for the same hash are untouched. No-op if the name is unbound. */
let unbind = (ns: t, ~name: string): unit =>
  switch (Hashtbl.find_opt(ns.by_name, name)) {
  | None => ()
  | Some(h) =>
    Hashtbl.remove(ns.by_name, name);
    remove_inverse(ns, name, h);
  };

let resolve = (ns: t, name: string): option(Hash.t) =>
  Hashtbl.find_opt(ns.by_name, name);

let names_of = (ns: t, h: Hash.t): list(string) =>
  switch (Hashtbl.find_opt(ns.by_hash, h)) {
  | None => []
  | Some(l) => List.sort(String.compare, l)
  };

let name_of = (ns: t, h: Hash.t): option(string) =>
  switch (names_of(ns, h)) {
  | [] => None
  | [n, ..._] => Some(n)
  };

let entries = (ns: t): list((string, Hash.t)) =>
  Hashtbl.fold((n, h, acc) => [(n, h), ...acc], ns.by_name, [])
  |> List.sort(((a, _), (b, _)) => String.compare(a, b));
