/* Language-dispatching façade for the two per-language printers.
   Pattern-matches on Definition and calls into Lc_pretty or
   Stlc_pretty. */

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Definition.Lc(_)) => Lc_pretty.print_named(~namespace, store, h)
  | Some(Definition.Stlc(_)) =>
    Stlc_pretty.print_named(~namespace, store, h)
  };

let print = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Definition.Lc(_)) => Lc_pretty.print(store, h)
  | Some(Definition.Stlc(_)) => Stlc_pretty.print(store, h)
  };

let print_shallow = (def: Definition.t): string =>
  switch (def) {
  | Definition.Lc(n) => Lc_pretty.print_shallow(n)
  | Definition.Stlc(n) => Stlc_pretty.print_shallow(n)
  };

/* Lc: closedness checked via Lc_pretty. Stlc: stored definitions are
   always type-checked, and a closed well-typed term is trivially
   closed, but a non-closed well-typed subterm exists too (any stored
   subterm of a Lam body carries free de Bruijn indices). So we still
   check closedness explicitly. */
let is_closed_hash = (store: Store.t, h: Hash.t): bool =>
  switch (Store.lookup(store, h)) {
  | None => false
  | Some(Definition.Lc(_)) => Lc_pretty.is_closed_hash(store, h)
  | Some(Definition.Stlc(_)) => Stlc_pretty.is_closed_hash(store, h)
  };

/* Display label for the REPL's language-labelled listings. Fixed-width
   (7 chars) so rows align. */
let language_tag = (def: Definition.t): string =>
  switch (def) {
  | Definition.Lc(_) => "[lc]   "
  | Definition.Stlc(_) => "[stlc] "
  };

let language_tag_of_hash = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "[?]    "
  | Some(d) => language_tag(d)
  };
