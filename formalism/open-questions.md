# Formalism open questions

Running list. Items graduate into `decisions.md`, into the paper, or into a successor artifact
as they resolve. Add, don't silently remove; when a question closes, link what closed it.

## The "work up" ladder (successor artifacts)

Each rung adds one thing to `arbor-core`, mirroring the prototype progression:

- **Types (STLC).** Promote the well-formedness gate to a typecheck gate. This is where
  `follow` first *fails* interestingly (a caller no longer type-checks against the new
  definition). Add a `Type_of` derived aspect beside `E`; state cache-invalidation on the type
  aspect. Source language: p6 / p9.
- **Mint / thread identity.** The second identity axis (p10/p11): threads that survive content
  edits, `edit-of-X` vs. fresh ingest, `thread_of` grouping. Formalize the mint as orthogonal to
  the content hash and re-derive migration lineage from it (stronger than history-based lineage).
- **Aspects in general.** Generalize `E` (derived) and `N` (asserted) to the full asserted/
  derived aspect store (`docs/design/02`); procedure identity as the cache key.
- **Translation / multi-language.** `Definition` as a sum over languages; the "no cross-language
  reference" construction-time invariant; translators as cached derived aspects on the source
  (`docs/design/05`).
- **Holes / incomplete programs.** How a hole participates in the canonical form and the hash
  (`docs/design/open-questions.md` §"Holes"); whether `wf` relaxes.
- **Branching / merging.** History and namespace lifecycle once more than one namespace exists.

## Modeling questions inside `arbor-core`

- **`hash` injectivity packaging.** Bundle `Hash` + `hash` + `hash-inj` as one record/axiom, or
  keep `hash` a bare postulated function? Affects how cleanly the paper's axiom maps to the Agda
  postulate. Lean: a record (one named assumption).
- **Evaluation under open terms.** p4 classifies a head `Var` as `Stuck` on closed terms "so the
  evaluator is total" (`eval.re`). Since stored defs are closed (`Term 0`), does `Stuck` ever
  actually arise? If provably never, drop it from the modeled result and simplify T5/T6; if it
  can arise via a malformed store, keep it and let `wf` rule it out. Decide during the proofs.
- **`multi_rebind` atomicity vs. `Σ` accumulation.** p11 registers candidate new hashes into
  `Σ` *before* `multi_rebind` may abort, so a failed cascade leaves dead hashes in `Σ` while
  `⟨N,H⟩` is untouched (`update_strategy.re`). The paper models `Σ` as strictly accumulating and
  only `⟨N,H⟩` as transactional (T8). Confirm this is the intended contract, not an artifact.
- **Round-trip precision.** `resolve_N ∘ print_N` is identity "up to name choice" (alphabetically
  -first among aliases, `pretty.re`). State the equivalence relation on surface terms precisely
  (α + name-choice), or restrict the round-trip theorem to core terms and treat surface as a
  quotient.
- **Determinism / confluence.** Is CBV-WHNF evaluation with `Ref`-unfolding deterministic as a
  function (it should be)? Worth a small lemma; needed for `E` to be well-defined as a partial
  *function* rather than a relation.

## Mechanization questions (Agda)

- **Store representation.** `Data.AVL` map vs. association list vs. a function with a finiteness
  proof. Trade-off: decidable membership + `wf` decidability vs. proof ergonomics.
- **Intrinsic vs. extrinsic scoping.** Intrinsic `Term : ℕ → Set` (vars `Fin n`) gives
  closedness and α for free but complicates `shift`/`subst` with `Fin` arithmetic; extrinsic
  `Term` + a separate `wf`/closedness predicate is closer to the p4 code. Lean: intrinsic.
- **How much to mechanize first.** T1–T4 (identity, no-silent-breakage, monotonicity, wf) are the
  natural first milestone; T5–T8 (eval stability, migration) are the second. Confirm staging.
