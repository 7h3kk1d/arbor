# p21 — Open questions

Running list. Findings that generalize bubble up to `docs/design/` and to
`formalism/open-questions.md`. Resolved entries are struck through in place with a date, not deleted.

## The paper

- **Is the `Pos = 0` transparency boundary deliberate?** `prop:transparent` shows transparency stops
  at the head of a component; the source comment justifies only the `0` case. Building it does not
  settle intent — inlining a singleton component's reference is free, so there is a plausible reason,
  but it may equally be an artifact of the encoding. Worth asking upstream rather than inferring.
- **Are the two orderings of `rem:twoorders` meant to be distinct?** Same shape of question. p21
  confirms they *are* distinct and that the difference is observable (committing
  `countDown`/`countUp` puts `countUp` at position 0), but not that anyone intended it.
- ~~**Does a definition's component hash equal its term hash?**~~ Answered 2026-09-02: no, and the
  gap is two `accumulate` layers even at *k* = 1. `rem:transparency-limit` is correct; three arbor
  documents that said otherwise are corrected.
- ~~**Does renaming a tied cycle member change the component hash?**~~ Answered 2026-09-02: not
  always. `thm:ordering`(b) split into (b1) position assignment, which always goes name-dependent,
  and (b2) the hash itself, which does in general but not for `ping`/`pong`.

## The scratch buffer as an editing context

- **Does the frontier/commit-set split survive a commit gate?** `13-editing-context.md` distinguishes
  the *frontier* (derived, grows, live) from the *commit set* (curated, incremental). p21 has neither
  — it has one buffer that is entirely both, committed atomically. Unison's `---` fold is the closest
  thing to the split and it is a much weaker object: a boundary, not two sets. The question is whether
  the split needs permissive ingest to be expressible at all, since a curated *subset* of a gated
  commit is not something Unison can express either.
- **Is "the residual is the next frontier" right, given that eviction is silent?** Using
  evict-on-failure confirms it is more useful than a list of names. It also removes bindings the user
  did not ask it to touch, without confirmation. Both halves are real; the synthesis is probably
  "deliver the residual as code *and* make the eviction visible and undoable", which is neither
  system's behaviour.
- **What would p21 look like with arbor's ingest?** The one experiment most worth running next: same
  buffer, same UI, but ingest everything and let names be what is uncommitted. The comparison would
  be direct, and it is the cleanest available test of `13-editing-context.md`'s central claim.

- ~~**Are watch expressions part of the model?**~~ Answered 2026-09-02, by being asked. They are:
  hashed in the same call as the definitions, unstored, unnamed, results cached. Both the prototype
  and the paper were missing it; see `formalism/decisions.md` 2026-09-02 (later).
- ~~**Does the cache survive across processes?**~~ Answered 2026-09-02: yes. Same watch, new
  process, warm codebase: **0.084 s** against ~5.8 s cold. Within one session the figures are
  5.79 / 5.82 / 11.45 s for one, twice, and two distinct watches. (First measured wrong — plain
  `ucm transcript` ignores `-c` and builds a fresh codebase; `transcript.fork -c` is the one that
  reads an existing one.)

## Things the model left unexercised

- **The relation-valued namespace has no job here.** Conflict is representable and every operation
  refuses it; nothing benefits, because merge is out of scope. That is evidence for the
  single-valued-`N` position, but it is evidence gathered with the motivating case removed. A p21
  successor with two namespaces and a merge would be the honest test.

- ~~**Merge is now in the paper and not in the prototype.**~~ **Closed 2026-09-03.** Built:
  `src/synhash.re`, `src/merge.re`, eight tests, 49/49 statements mirrored. Building it corrected
  `def:mergediff` and `def:umerge` and settled `rem:merge-soundness` — see `formalism/decisions.md`.
  The one thing left unmirrored in spirit rather than in letter is the render environment: the paper
  distinguishes `rho` (suffixified) from `nu` (not), and with atomic names the two coincide here, so
  `prop:merge-asym`'s second mechanism is exercised and suffixification is not.

- **`rem:innercycle` is out of reach, and the reason is the interesting part.** The finding is that
  `hash_component` has two callers who decide the ordering warning differently: the top-level path
  aborts (`cor:detect`), the internal `let rec` path drops the warning and lets *source declaration
  order* fix the de Bruijn indices, so the enclosing definition's hash depends on which of two
  indistinguishable bindings was written first. p21 has one caller — `def:urterm` has no `let`, so no
  inner cycle exists to hash. Modelling it means adding let-rec to the object language purely to
  acquire a second call site, which is not obviously worth it. Recorded here rather than left implied,
  because the shape generalizes: the guarantee is a property of a *call site*, not of the hash
  function, and any substrate with more than one caller has to make them agree.

## Vocabulary

- **"Editing context" is still two things.** `10-followups.md` marks the collision as blocking:
  p12–p17 use it for an opacity *open set* (a capability), `13-editing-context.md` for a *working set*
  of definitions. p21 sidesteps it by carrying no `Opaque`/`Seal` machinery and saying "scratch
  buffer" throughout, which resolves nothing but does supply evidence that the working-set reading is
  the one with an implementation behind it.
