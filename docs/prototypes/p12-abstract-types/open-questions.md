# p12 — Open questions

Running list. Findings that generalize bubble up to `docs/design/12-type-abstraction.md`
(and its open-questions section).

## Editing-context mechanics

- **What exactly defines a context's open set, and its lifecycle?** Auto-open on
  create-type; explicit open on open-existing; closing a context is just
  discarding ephemeral state (no stored effect). Can you open several abstract
  types at once (the "module" shape)? Leaning yes.
- **Legibility of "I am in an opening context."** How does the interface make the
  transparency-on state obvious, so a user is never surprised that `0 : t`
  type-checked? (Question 4 of the scope doc.)
- **Should the prototype let you hand-write a seal outside any opening context?**
  Doing so would demonstrate the editor-enforcement boundary directly: the
  substrate accepts it (no ingest enforcement), but the editor would never author
  it. Useful as a teaching artifact; risks looking like a hole in the model.

## Minimal sealing edge cases

- **Mixed impls.** A term that *both* touches the representation *and* calls a
  sealed op (e.g. `\x: t. incr x + 1`) is sealed (needs the unfold), but its impl
  cannot be fully normalized to a witness-only body — it retains an opaque `Ref`.
  Is that fine (store the impl as-is, checked under `opens`), or does it want a
  different treatment? Suspect fine; confirm against examples.
- **What counts as "the opaques actually used"** when computing a seal's `opens`
  from a context that opened several? The minimal set that makes the check pass,
  or the whole context open set? Minimal set is cleaner but costs a search.

## Identity / lineage

- **edit-of mint carry-forward UX.** The gesture that re-uses an abstract type's
  mint across a witness change (pair-counter lineage). What's the surface gesture,
  and how is "same abstraction, new representation" shown?
- **Abstract-over-abstract witnesses.** Can an opaque type's witness be another
  opaque type? Deferred; note where it would break the "witness is concrete"
  assumption in normalization.

## Display

- **Sealed op vs. shared raw body.** A seal renders by its name (`Counter.incr`);
  its normalized raw body may *also* be named (`Math.inc`) and is a different
  hash. When inspecting a seal, do we reveal the raw body's name (which leaks the
  witness-level identity)? Leaning: only inside an opening context.
- **Rendering an opaque type.** `Counter.t` by name; fall back to a short hash
  when unbound. Never show the witness outside an opening context.
