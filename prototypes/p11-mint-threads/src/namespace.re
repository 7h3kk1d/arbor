/* Namespace — first-class name → hash bindings, with hierarchical
   dot-delimited names and Unison-style longest-suffix resolution.

   Storage: flat Hashtbl keyed by the dotted string, plus a reverse
   Hashtbl for hash → list(name). Hierarchy is purely a
   convention on the string key — `math.add` is a single string, not a
   trie node — but the resolution path knows how to split on `.` and
   match by whole-segment suffix. The trie is built lazily by the UI
   when rendering the namespace browser; the substrate keeps a single
   source of truth.

   Name resolution comes in two flavors:
   - `resolve_path`: fast path for full-path lookup; one Hashtbl probe.
   - `resolve_query`: full-path first, otherwise longest-segment-suffix
     with ambiguity detection. The matched segments must align on
     dot-segment boundaries, so `add` matches `math.add` but `dd` does
     not match `add`.

   The "suffix-as-segments" rule rules out half-segment matches (so
   `ist.head` matches neither `data.list.head` nor `list.head`) and is
   the simplest definition that still produces the Unison-feel where
   leaf names work as long as they're unambiguous.

   Reserved names: keywords from the surface grammar. Cannot be bound. */

exception Name_reserved(string);
exception Name_already_bound(string);

let reserved_names: list(string) = [
  "let",
  "in",
  "if",
  "then",
  "else",
  "true",
  "false",
  "fst",
  "snd",
  "not",
  "mul",
  "mod",
  "Int",
  "Bool",
  "String",
];

let is_reserved_segment = (s: string): bool =>
  List.exists(r => r == s, reserved_names);

let split_segments = (name: string): list(string) =>
  String.split_on_char('.', name);

/* A name with any segment in the reserved-keyword set is reserved.
   Bindings like `let.x` would be confusing because `let` is a parser
   keyword; refusing them upfront avoids surprises. */
let is_reserved_name = (s: string): bool =>
  List.exists(is_reserved_segment, split_segments(s));

/* Per-name binding history. Each bind / rebind / unbind appends a
   `(option(Hash.t), timestamp)` row; the head is the newest event.
   `Some(h)` is a bind or rebind to `h`; `None` is an unbind tombstone.
   The current binding is the head if it is `Some`, else there is no
   current binding. */
type history_event = (option(Hash.t), float);

type t = {
  by_name: Hashtbl.t(string, Hash.t),
  by_hash: Hashtbl.t(Hash.t, list(string)),
  history: Hashtbl.t(string, list(history_event)),
};

let create = (): t => {
  by_name: Hashtbl.create(32),
  by_hash: Hashtbl.create(32),
  history: Hashtbl.create(32),
};

let now_ = (): float =>
  /* `Unix.gettimeofday` is not available under jsoo without a polyfill;
     `Sys.time` is OCaml stdlib and works everywhere. Coarser but
     sufficient for binding-history ordering. */
  Sys.time();

let append_history = (ns: t, ~name: string, ev: history_event): unit => {
  let prior =
    switch (Hashtbl.find_opt(ns.history, name)) {
    | Some(xs) => xs
    | None => []
    };
  Hashtbl.replace(ns.history, name, [ev, ...prior]);
};

let history = (ns: t, name: string): list(history_event) =>
  switch (Hashtbl.find_opt(ns.history, name)) {
  | Some(xs) => xs
  | None => []
  };

let all_history = (ns: t): list((string, list(history_event))) =>
  Hashtbl.fold((k, v, acc) => [(k, v), ...acc], ns.history, []);

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
  append_history(ns, ~name, (Some(hash), now_()));
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
  append_history(ns, ~name, (Some(hash), now_()));
};

let unbind = (ns: t, ~name: string): bool =>
  switch (Hashtbl.find_opt(ns.by_name, name)) {
  | None => false
  | Some(old_hash) =>
    Hashtbl.remove(ns.by_name, name);
    remove_inverse(ns, ~name, ~hash=old_hash);
    append_history(ns, ~name, (None, now_()));
    true;
  };

/* Reverse lookup that includes a version index.

   The version is the 1-indexed position of `hash` among the `Some(_)`
   events of `name`'s history, counting from oldest forward. If `hash`
   is the current binding (head is `Some(hash)`), `is_current` is true.
   Returns the first name with this hash in its history; for hashes
   that never appeared in history, returns None.

   Used by Pretty to render orphan references as `name(vN)`. */
let reverse_lookup_with_version =
    (ns: t, hash: Hash.t): option((string, int, bool)) => {
  let result = ref(None);
  Hashtbl.iter(
    (name, events) =>
      if (result^ == None) {
        let some_events =
          List.filter_map(((h, _)) => h, List.rev(events));
        let rec find = (i, hs) =>
          switch (hs) {
          | [] => None
          | [h, ...rest] =>
            if (h == hash) {
              Some(i);
            } else {
              find(i + 1, rest);
            }
          };
        switch (find(1, some_events)) {
        | None => ()
        | Some(v) =>
          let is_current =
            switch (Hashtbl.find_opt(ns.by_name, name)) {
            | Some(curr) => curr == hash
            | None => false
            };
          result := Some((name, v, is_current));
        };
      },
    ns.history,
  );
  result^;
};

/* Atomic multi-rebind. All updates are validated (target hash present
   in `target_exists` predicate, name not reserved) then committed as a
   bundle. Returns Error with the offending entry if validation fails;
   the namespace is unmodified in that case. */
type multi_rebind_error =
  | Mr_reserved(string)
  | Mr_target_missing(string, Hash.t);

let multi_rebind =
    (
      ns: t,
      ~target_exists: Hash.t => bool,
      updates: list((string, Hash.t)),
    )
    : result(unit, multi_rebind_error) => {
  let rec validate = lst =>
    switch (lst) {
    | [] => Ok()
    | [(name, _), ..._] when is_reserved_name(name) =>
      Error(Mr_reserved(name))
    | [(name, hash), ...rest] =>
      if (!target_exists(hash)) {
        Error(Mr_target_missing(name, hash));
      } else {
        validate(rest);
      }
    };
  switch (validate(updates)) {
  | Error(_) as e => e
  | Ok () =>
    List.iter(((name, hash)) => rebind(ns, ~name, hash), updates);
    Ok();
  };
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

/* Hierarchical resolution.

   resolve_query first tries an exact full-path lookup (fast path; the
   common case after suffix resolution has been printed back to a full
   name in the editor). If that fails, splits the query on '.', scans
   the namespace for entries whose dot-segments END with the query's
   segments. Empty match → Unbound; one match → Found; multiple →
   Ambiguous (sorted full names of the candidates). */

type resolve_error =
  | Unbound
  | Ambiguous(list(string));

let segments_match_suffix = (~entry_segs: list(string), ~query_segs: list(string)): bool => {
  let entry_n = List.length(entry_segs);
  let query_n = List.length(query_segs);
  if (query_n == 0 || query_n > entry_n) {
    false;
  } else {
    let drop = entry_n - query_n;
    let rec drop_n = (n, lst) =>
      if (n <= 0) {
        lst;
      } else {
        switch (lst) {
        | [] => []
        | [_, ...rest] => drop_n(n - 1, rest)
        };
      };
    let suffix = drop_n(drop, entry_segs);
    List.equal(String.equal, suffix, query_segs);
  };
};

let resolve_query = (ns: t, query: string): result(Hash.t, resolve_error) =>
  switch (Hashtbl.find_opt(ns.by_name, query)) {
  | Some(h) => Ok(h)
  | None =>
    let query_segs = split_segments(query);
    let matches =
      Hashtbl.fold(
        (entry_name, h, acc) => {
          let entry_segs = split_segments(entry_name);
          if (segments_match_suffix(~entry_segs, ~query_segs)) {
            [(entry_name, h), ...acc];
          } else {
            acc;
          };
        },
        ns.by_name,
        [],
      );
    switch (matches) {
    | [] => Error(Unbound)
    | [(_, h)] => Ok(h)
    | _ =>
      let names =
        List.sort(String.compare, List.map(fst, matches));
      Error(Ambiguous(names));
    };
  };
