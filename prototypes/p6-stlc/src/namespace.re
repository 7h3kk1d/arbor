/* Namespace — first-class name ↔ hash bindings.

   Carried from p4 verbatim. The only p5 change: the reserved-name list
   is the UNION of both languages' surface keywords. p4 drained the
   list to empty (λ-calculus has no keywords); arith brings back
   `true false succ pred iszero if then else`. (The literal `0` is a
   keyword at the lexer level but is never a valid identifier anyway,
   so it does not need to be reserved here.)

   One name binds to one hash regardless of the hash's language; the
   Resolver enforces language compatibility at edit time. */

exception Name_reserved(string);
exception Name_already_bound(string);

let reserved_names = [
  "true",
  "false",
  "succ",
  "pred",
  "iszero",
  "if",
  "then",
  "else",
];

let is_reserved_name = (s: string): bool =>
  List.exists(r => r == s, reserved_names);

type t = {
  by_name: Hashtbl.t(string, Hash.t),
  by_hash: Hashtbl.t(Hash.t, list(string)),
};

let create = (): t => {
  by_name: Hashtbl.create(32),
  by_hash: Hashtbl.create(32),
};

let size = (ns: t): int => Hashtbl.length(ns.by_name);

let add_inverse = (ns: t, ~name: string, ~hash: Hash.t): unit => {
  let existing =
    switch (Hashtbl.find_opt(ns.by_hash, hash)) {
    | Some(ns) => ns
    | None => []
    };
  if (!List.exists(n => n == name, existing)) {
    Hashtbl.replace(ns.by_hash, hash, existing @ [name]);
  };
};

let remove_inverse = (ns: t, ~name: string, ~hash: Hash.t): unit =>
  switch (Hashtbl.find_opt(ns.by_hash, hash)) {
  | None => ()
  | Some(names) =>
    let names' = List.filter(n => n != name, names);
    if (names' == []) {
      Hashtbl.remove(ns.by_hash, hash);
    } else {
      Hashtbl.replace(ns.by_hash, hash, names');
    };
  };

let bind = (ns: t, ~name: string, hash: Hash.t): unit => {
  if (is_reserved_name(name)) {
    raise(Name_reserved(name));
  };
  if (Hashtbl.mem(ns.by_name, name)) {
    raise(Name_already_bound(name));
  };
  Hashtbl.add(ns.by_name, name, hash);
  add_inverse(ns, ~name, ~hash);
};

let rebind = (ns: t, ~name: string, hash: Hash.t): unit => {
  if (is_reserved_name(name)) {
    raise(Name_reserved(name));
  };
  switch (Hashtbl.find_opt(ns.by_name, name)) {
  | Some(old_hash) => remove_inverse(ns, ~name, ~hash=old_hash)
  | None => ()
  };
  Hashtbl.replace(ns.by_name, name, hash);
  add_inverse(ns, ~name, ~hash);
};

let unbind = (ns: t, ~name: string): bool =>
  switch (Hashtbl.find_opt(ns.by_name, name)) {
  | None => false
  | Some(old_hash) =>
    Hashtbl.remove(ns.by_name, name);
    remove_inverse(ns, ~name, ~hash=old_hash);
    true;
  };

type rename_error =
  | Source_unbound
  | Target_already_bound
  | Target_reserved;

let rename =
    (ns: t, ~from: string, ~to_: string): result(unit, rename_error) =>
  if (is_reserved_name(to_)) {
    Error(Target_reserved);
  } else {
    switch (Hashtbl.find_opt(ns.by_name, from)) {
    | None => Error(Source_unbound)
    | Some(_) when Hashtbl.mem(ns.by_name, to_) => Error(Target_already_bound)
    | Some(h) =>
      let _: bool = unbind(ns, ~name=from);
      bind(ns, ~name=to_, h);
      Ok();
    };
  };

let resolve = (ns: t, name: string): option(Hash.t) =>
  Hashtbl.find_opt(ns.by_name, name);

let names_of = (ns: t, hash: Hash.t): list(string) =>
  switch (Hashtbl.find_opt(ns.by_hash, hash)) {
  | None => []
  | Some(names) => List.sort(String.compare, names)
  };

let entries = (ns: t): list((string, Hash.t)) => {
  let acc = Hashtbl.fold((name, hash, acc) => [(name, hash), ...acc], ns.by_name, []);
  List.sort(((a, _), (b, _)) => String.compare(a, b), acc);
};
