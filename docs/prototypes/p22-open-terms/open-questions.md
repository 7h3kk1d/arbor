# p22 — Open questions

Running list. Findings that generalize bubble up to `docs/design/` and to
`formalism/open-questions.md`. Resolved entries are struck through in place with
a date, not deleted.

## The open/link split

- **Does the labelled telescope buy anything?** The plan was to build positional
  as the stored form and a label-keyed view as a derived aspect, and compare.
  Positional landed; the labelled view was never needed — nothing in the
  prototype wanted an open term that distinguishes `\n. scale n` from
  `\n. shrink n`, because everything that cares about the dependency reads the
  link. *Leaning:* the labelled form is a display concern that the intent table
  already covers, and `03`'s B2 (labels to fix ordering) is answered by the
  automorphism result rather than by labels. Worth one more look before the
  question is closed, because the argument is "nothing needed it", which is
  weaker than "nothing could need it".

- **Is first-occurrence the right slot order?** It is deterministic and
  name-free, which is what `03-content-addressing.md:142` requires, but it is
  not the only such order and nothing here tests the alternatives. Reordering an
  expression's subterms changes the telescope, so two definitions that differ
  only in evaluation-irrelevant ordering get different open hashes.

- **What is the arity of an edit?** Adding a free variable changes the open term
  *and* the link's shape, so an edit that only adds a dependency reads as an
  authored edit. Correct, but it means "authored edit" is coarser than "the body
  changed in a way a reader would notice".

## Cycles and bundles

- **`Bundle.canonical_order` is identity.** The composition argued for in
  `bundle.re` — sort asymmetric groups by member open hash, leave symmetric ones
  alone because the automorphism argument already covers them — is written down
  and not implemented, and therefore not tested. It is the obvious next thing to
  check.

- **Editing one member of a cycle rehashes the whole bundle.** The open terms
  stay independent, which is the win, but the bundle is one object so its hash
  moves and every member's `Def.Member` hash moves with it. `03`'s table records
  the same cost for all three options; p22 does not improve on it. Whether a
  per-member closed identity is possible at all is unasked.

- **Recursive groups cannot be edited.** `Session.edit` handles `Def.Simple`
  only; a buffer whose plan produces a `Group` always re-defines. Propagation
  into and out of bundles is untested.

## Propagation and intent

- **`explicit` takes a site list and nothing computes one.** The UI offers the
  strategy and passes `[]`, so explicit is pin with extra steps. The sites a
  follow *would* touch are available (`Propagate.reachable_callers`); what is
  missing is the interface for choosing among them, which is
  `04-naming-layer.md`'s "user in loop" axis and the actual hard part.

- **`follow_clean` is computed and never cached.** `02-definitions-and-derived-data.md`
  would key it `(h_old, h_new)` as a derived aspect. There is no aspect store in
  p22 at all — `Store.types` is a bare table — so the aspect model went
  unexercised, which is a gap given that the model is arbor's.

- **Intent accumulates and nothing ever reads more than the head.**
  `Store.intent_of` returns every name anyone used, which is what makes the
  clobber impossible, but display takes the first and `~via` tests membership.
  What a second name in that list should *mean* to a reader is unanswered.

## History

- **Undo is not implemented.** The log carries grouping ids precisely so a
  gesture can be taken back as one act, and nothing consumes them. The inverse
  of `Relinked` is well defined (rebind the names, the old objects are still
  there); the inverse of `Defined` is a namespace operation, since nothing is
  ever removed from the store.

- **The log is in memory and dies with the process**, like p21's scratch state.
  Whether the operation log is itself content-addressed — which is what
  "git-style content addressing over the history" in the design doc asks for —
  is not touched here. `H` vs `Causal` (`formalism/open-questions.md:213-219`)
  remains the open rung.

- **`versions_of_name` follows a name, so a rename splits a history.** `rename`
  emits `Unbound` then `Bound`, and the new name's version list starts at the
  rename. The definition is the same object throughout and the log has both
  events, so the join is derivable and simply not derived.

## Vocabulary

- **"Link" is doing two jobs.** It is the stored object (a target vector) and the
  operation (binding produces one). `bind.re` and `link.re` both read as if the
  reader already knows which is meant.

- **"Open term" collides with `13-editing-context.md`'s "open" (a definition
  currently being edited) and with p12–p17's "open set" of transparent abstract
  types.** Three unrelated senses of *open* now exist in the repo.
