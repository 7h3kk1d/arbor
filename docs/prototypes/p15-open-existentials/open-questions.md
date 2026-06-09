# p15 — Open questions

Running list. Findings that generalize bubble up to `docs/design/12-type-abstraction.md`.

## `open` is top-level only — no local / functor-body open

- `open` is a primitive *editor / namespace* gesture, not a term form: it extracts
  an existential's hidden type(s) into the namespace at top level (binding `N.t`,
  `N`, and the operation fields). There is **no way to open an existential inside a
  function or functor body**, so a non-top-level (local) open existential is not
  expressible. The only term-level way to consume an existential inside a body is
  the closed `unpack` (p14), which is bounded by avoidance — the witness type may
  not escape the `unpack` scope.
- This matches the OCaml correspondence we drew (`module M = (val e) in <rest>` is
  a top-level / `let`-region gesture, not something spliced into an arbitrary
  expression), but it is a real limit worth recording: a "functor that opens its
  argument and returns a module mentioning the freshly-opened abstract type" is not
  writable here.
- Open: whether to add a term-level local `open`, and how its generativity and
  scoping would interact with content-addressing (each open mints a fresh
  `Abstract`) and with avoidance (the minted type must not escape the body where it
  is in scope). Until then, the top-level gesture plus closed `unpack` is the whole
  surface.
- **Answered in p17** (`docs/prototypes/p17-translucent-modules/00-scope.md`):
  `open e as M in body` is a term form, scoped like `unpack` (de Bruijn type
  vars + occurs-check avoidance, **no mint** — chosen so a hidden type can
  never leak without a global name to refer to it by, and so α-equivalence
  survives). The second half — a functor that opens its argument and *returns*
  a module mentioning the freshly-opened type — remains inexpressible, now for
  a sharper reason: the witness varies at runtime with the argument, so escape
  would be unsound; see p17's open questions for the possible closed-scrutinee
  relaxation.
