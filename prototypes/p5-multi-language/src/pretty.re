/* Language-dispatching façade for the two per-language printers.
   `print_named` / `print` / `print_shallow` / `surface_language_tag`
   pattern-match on the Definition and call into Arith_pretty or
   Lc_pretty. */

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Definition.Arith(_)) =>
    Arith_pretty.print_named(~namespace, store, h)
  | Some(Definition.Lc(_)) => Lc_pretty.print_named(~namespace, store, h)
  };

let print = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Definition.Arith(_)) => Arith_pretty.print(store, h)
  | Some(Definition.Lc(_)) => Lc_pretty.print(store, h)
  };

let print_shallow = (def: Definition.t): string =>
  switch (def) {
  | Definition.Arith(n) => Arith_pretty.print_shallow(n)
  | Definition.Lc(n) => Lc_pretty.print_shallow(n)
  };

/* Closed-ness check is lc-specific (arith has no binders, so "closed"
   is trivially true). The REPL's `:list closed` filter uses this. */
let is_closed_hash = (store: Store.t, h: Hash.t): bool =>
  switch (Store.lookup(store, h)) {
  | None => false
  | Some(Definition.Arith(_)) => true
  | Some(Definition.Lc(_)) => Lc_pretty.is_closed_hash(store, h)
  };

/* Display label for the REPL's language-labelled listings. Fixed-width
   (7 chars) so rows align. */
let language_tag = (def: Definition.t): string =>
  switch (def) {
  | Definition.Arith(_) => "[arith]"
  | Definition.Lc(_) => "[lc]   "
  };

let language_tag_of_hash = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "[?]    "
  | Some(d) => language_tag(d)
  };
