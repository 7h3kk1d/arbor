# p14 — Open questions

Running list. Findings that generalize bubble up to `docs/design/12-type-abstraction.md`.

## Existential core

- **Avoidance problem.** A result type that mentions the unpacked `t` is rejected
  ("witness escapes"). Cleverer avoidance (find the least supertype not mentioning
  `t`, à la SML) and open existentials (Montagu–Rémy) are deferred. Is plain
  rejection too restrictive for any program we actually want to write?
- **Witness check on pack.** `pack [W] e as exists t. T` checks `e : T[t := W]`.
  Confirm the de Bruijn substitution lines up with `unpack`'s fresh-variable
  introduction (the two must agree on the body type's indices).
- **Relationship to opaque types (p12).** Both hide a representation: `Opaque`
  (sealed, authoring-time, one fixed abstract type) vs `∃` (value-level, a fresh
  abstract type per unpack). Worth a design note on when each is the right tool —
  `12` §"The two quantifiers" gestures at it.

## Surface / ergonomics

- **pack/unpack syntax.** `pack [W] e as E` and `unpack [t] x = e in body` chosen;
  `[t]` reuses the type-application brackets. Revisit if it reads poorly.
- **Bundling.** Positional `Product` for ops; `fst (snd c)` nesting is ugly past a
  couple of fields. Records (`11`) are the real fix; until then keep packages
  small.

## Toward records / first-class modules

- **Named-field packages.** `∃t. { empty: t; incr: t->t; get: t->Int }` with
  projection by name needs label-records (`11`). That plus `∃` is the genuine
  first-class module; this prototype does the `∃` half over Products.
