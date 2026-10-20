# Formalism open questions

Running list. Items graduate into `decisions.md`, into a paper, or into a successor artifact as they
resolve. Add; do not silently remove. When a question closes, link what closed it.

## The "work up" ladder (successor artifacts)

Each rung adds one thing to `arbor-core`, mirroring the prototype progression:

- **Types (STLC).** Promote the well-formedness gate to a typecheck gate. This is where `follow`
  first fails interestingly: a caller no longer type-checks against the new definition. Add a
  `Type_of` derived aspect beside `E` and state cache invalidation on it. Source language: p6 / p9.
  *Graduated 2026-07-30* as `paper/arbor-stlc.tex` (`decisions.md`, same date). The resolution
  differs from this framing: there is no typecheck gate, since migration commits and reports the
  residual, and there is no cache invalidation to state, since Θ entries are absolute per hash. New
  questions are under "Modeling questions from `arbor-stlc`".
- **Mint / thread identity.** The second identity axis (p10/p11): threads that survive content
  edits, `edit-of-X` vs. fresh ingest, `thread_of` grouping. Formalize the mint as orthogonal to the
  content hash and re-derive migration lineage from it, which is stronger than history-based
  lineage. Unit-scoped following returns here. arbor-core's migration is a whole-store structural
  rewrite (`decisions.md` 2026-07-30), and per-unit intent (selective follow, lineage through
  unnamed intermediates) is what the mint adds back. See also "Provenance without mints" below.
- **Aspects in general.** Generalize `E` (derived) and `N` (asserted) to the full asserted/derived
  aspect store (`docs/design/02`), with procedure identity as the cache key.
- **Translation / multi-language.** `Definition` as a sum over languages, the "no cross-language
  reference" construction-time invariant, and translators as cached derived aspects on the source
  (`docs/design/05`).
- **Holes / incomplete programs.** How a hole participates in the canonical form and the hash
  (`docs/design/open-questions.md` §"Holes"), and whether `wf` relaxes.
- **Branching / merging.** History and namespace lifecycle once more than one namespace exists. It
  now has a target: see "`H` vs. `Causal`" below.

## `mini-unison` — deferred, and questions it opened (2026-08-25)

`paper/mini-unison.tex` models Unison's content addressing and update scheme. It is a sibling, not a
rung: it does not extend `arbor-core`, so it is not on the ladder above. Choices and findings are in
`decisions.md` 2026-08-25.

**Executed 2026-09-02.** `prototypes/p21-unison-model/` implements `def:tokens` through `def:update`
over this object language, with each of the 29 statements as a test that names it.
`scripts/check-labels.sh paper/mini-unison.tex ../prototypes/p21-unison-model/src` keeps the two in
step. Running it changed three statements (`decisions.md` 2026-09-02;
`../docs/prototypes/p21-unison-model/00-scope.md`). The deferred list below is unaffected, since the
prototype models only what the paper models.

**Deferred from v1**, each a candidate successor:

- **The namespace's representation.** `Branch` as a hashed Merkle tree whose `children` field points
  at each child's causal hash, so every level has its own history. mini-unison models the naming
  semantics (names as a relation, unconflictedness as a derived predicate operations demand) because
  `update` cannot be stated without them, and stops there.
- **`Causal` and branching, but no longer merge.** Merge landed 2026-09-03 as §7, a function of three
  namespaces with the common ancestor given. Computing the ancestor is still deferred. arbor's `H`
  cannot do it: `H` is per-name, flat and unhashed, and cannot answer "do these two states share an
  ancestor?". Deferring only the ancestor cost nothing, since every §7 result concerns the diff and
  the naming function, not how the LCA was found.
- **Suffix resolution and the `lib`-depth priority rule.** Names in mini-unison are atomic.
- **Data declarations and `unique`-type minting.** With only `Nat`, `Text` and arrows there are no
  declarations, so `Referent`'s `Con` arm is vestigial and the minting axis is absent. Reinstating it
  would let the mint-stability question be stated on both sides.
- **Abilities, the ANF runtime, `LocalIds` and serialization.**

Synhash was also deferred here first and has since been modelled; see "Provenance without mints".

**Questions it opened:**

- **Does the ordering side condition propagate?** `thm:ordering` guards presentation-independence by
  injectivity of the permutation hash, and every downstream statement about component identity
  inherits that guard. Is there a formulation without it, for instance ordering members with a mint
  as `docs/design/03-content-addressing.md` §"Mutual recursion" proposes? If so, does that make the
  two systems' recursion stories comparable, or only different? Related: "Is `Σ : Hash ⇀ Node` the
  right key type?" below.
- **Is the `Pos = 0` asymmetry load-bearing or incidental?** `prop:transparent` shows transparency
  stops at the head of a component. Is that a deliberate optimization (a singleton component's
  reference costs nothing to inline) or an accident of the encoding? The source comment justifies only
  the `0` case. Ask upstream; do not infer. *(Still open 2026-09-02. `p21-unison-model` shows the
  asymmetry is observable: committing `countDown`/`countUp` puts `countUp` at position 0, so of two
  definitions written symmetrically one is referenced transparently and the other opaquely, with
  nothing in the surface syntax saying which. A reimplementation cannot recover intent.)*
- **Can `prop:print-parse` be proved for a realistic surface language?** mini-unison proves the
  round-trip obligation for its own small language. Unison's is far larger, and the property is
  maintained by a golden corpus. The question is whether the pretty-print environment (`def:ppe`),
  where the difficulty lives, admits a specification under which the property is provable instead of
  testable. *(Data point 2026-09-02: as a qcheck property over `p21-unison-model`'s six term formers,
  it caught a real printer bug on its first run, parentheses omitted around an application in
  argument position. At that scale a property dominates a corpus. The live question is where the
  crossover sits, and whether a property over a generated subset plus a corpus for the rest is the
  right shape.)*

## Modeling questions inside `arbor-core`

- **`hash` injectivity packaging.** Bundle `Hash` + `hash` + `hash-inj` as one record/axiom, or keep
  `hash` a bare postulated function? This affects how cleanly the paper's axiom maps to Agda. Lean: a
  record (one named assumption). *(Settled 2026-08-05: (★) is a field of the `HashModel` record,
  parameterized over the entry functor; `decisions.md` 2026-08-05 items 1 and 5.)*
- **Evaluation under open terms.** p4 classifies a head `Var` as `Stuck` on closed terms "so the
  evaluator is total" (`eval.re`). Since stored defs are closed (`Term 0`), does `Stuck` ever arise?
  If provably never, drop it and simplify T5/T6; if a malformed store can produce it, keep it and let
  `wf` rule it out. *Answered 2026-08-05: yes, it can.* A well-formed store can hold an entry that
  reconstructs to an open term (storage is shallow), so `Stuck` is reachable from `ref h` unless h's
  target is known closed. This is the counterexample refuting `lem:closed-no-stuck` as first stated
  (`agda/Arbor/Core/Counterexamples.agda`). `wf` alone does not rule it out; the closedness premise on
  `Eval` does. `Stuck` stays in `Result`. Follow-ups are under "Questions raised by Milestone 2".
- **`multi_rebind` atomicity vs. `Σ` accumulation.** p11 registers candidate hashes into `Σ` before
  `multi_rebind` may abort, so a failed cascade leaves dead hashes in `Σ` while `⟨N,H⟩` is untouched
  (`update_strategy.re`). The paper models `Σ` as strictly accumulating and only `⟨N,H⟩` as
  transactional (T8). Is that the intended contract or an artifact? *Annotated 2026-07-30:* the typed
  rung confirms accumulate-only is safe. Under the permissive store, retained candidates are harmless
  debris (ill-typed entries are legal, unnamed ones unreachable), and arbor-stlc's Migrate never
  aborts (commit-and-report).
- **Round-trip precision.** `resolve_N ∘ print_N` is identity "up to name choice" (alphabetically
  first among aliases, `pretty.re`). State the equivalence on surface terms precisely (α +
  name-choice), or restrict the round-trip theorem to core terms and treat surface as a quotient.
  *Closed 2026-07-30:* printing is a judgment (`P-Var`/`P-Ref`/`P-Lam`/`P-App`). Print-then-elaborate
  is exact for every printer choice (`prop:print-elab`), and elaborate-then-print holds up to `≈`, the
  kernel of elaboration, generated by capture-avoiding binder renaming and alias swap
  (`prop:elab-print`). See `decisions.md` 2026-07-30.
- **Determinism / confluence.** Is CBV-WHNF evaluation with `Ref`-unfolding deterministic as a
  function? It should be. A small lemma, needed for `E` to be well defined as a partial function and
  not only a relation. *(`lem:determinism` is in Milestone 1, proved 2026-08-05; see "Mechanization
  questions (Agda)".)*

## Modeling questions from `arbor-stlc` (2026-07-30)

- **Sound clean-cache keys.** `clean` is decidable but not stable under store growth (arbor-stlc S8;
  p11's `BLAKE2B(h_old‖h_new)`-keyed write-once cache is unsound). What key makes a follow-clean
  cache write-once-sound? Candidates: a store-generation counter; the callers-closure snapshot
  `(g_old, g_new, callers*(Σ, g_old))` in the key; invalidation on Ingest into the closure (but
  derived aspects are write-once by design). Interacts with `docs/design/02` if the aspect-key schema
  must grow.
- **Type naming and type-level migration.** Type aliases in `N` (`Vector = Bool → Bool`; p9's
  dividend is that alias and spelled-out annotation share a Lam hash), printing types by name (a P-Ref
  for type positions), and migrating a type definition (p11's `Named_type` cascade path). One
  successor bundle. arbor-stlc defers all of it, and its ρ fixes type entries.
- **Richer residual reporting, and whether to model evict-and-render.** The formalism's residual is
  a set of names. The continue-the-refactor UX wants per-name witnesses (expected vs. actual type at
  the failing position). Is that editing-layer elaboration over the same dry run, or part of the
  report? Unison's answer is the strongest available: its residual is the code itself, delivered as a
  working set. mini-unison's `def:update` step 5 formalizes that, arbor has no counterpart, and
  `docs/design/13-editing-context.md` argues the residual is the next frontier. *(Partly answered
  2026-09-02 by building it in `p21-unison-model` (merged here from the mini-unison list). Getting the
  broken code back in the buffer, ready to edit, is more useful than a list of names. It is also
  silent breakage: a failed update removes bindings the user did not ask it to touch, including the
  one being edited, with no confirmation, and `rem:step5` records how wide the removal is. The likely
  synthesis is to deliver the residual as code and make the eviction visible and undoable, which is
  neither system's behaviour. That is what arbor should model.)*

## Divergences from Unison (source read 2026-08-07)

From reading the Unison implementation at commit `db60ce2`. Notes are in `docs/systems/unison/`,
mapped onto these objects in `docs/systems/unison/07-arbor-mapping.md`. Unison is the closest
existing system to this model and has no publication, so these are the only external comparison
points. They are questions, not defects on either side. Each names a fork where the two systems
chose differently and this paper has not said why.

- **Is `Σ : Hash ⇀ Node` the right key type?** Unison hashes a whole strongly connected component, so
  a definition is `(Hash, Pos)` (`codebase2/core/U/Codebase/Reference.hs:114-119`). That is the cost
  of mutual recursion, and it changes the type of the store, not the canonicalizer: `wf`'s no-dangle
  clause, `callers_of`, ρ and `thm:alpha` all move. Its canonical cycle ordering is also partial:
  structurally identical members leave the order underdetermined, and the user must perturb one
  definition (`IncompleteElementOrderingError`, unisonweb/unison#2787). *(Refined 2026-08-25:
  hashing is total and warns; the top-level caller aborts and internal `let rec` does not. See
  `decisions.md` 2026-08-25 and 2026-09-03.)*

  The cost is optional. `docs/design/03-content-addressing.md` §"Mutual recursion: three ways to
  close the cycle" (2026-08-07) closes the cycle with a binder in the object language instead of in
  the hasher: a `Fix` over a product, each member an ordinary definition projecting out of it. The
  bundle's body is closed because `self` is bound, so `Σ : Hash ⇀ Node` is untouched, a reference
  stays a hash, and no metatheory in this paper changes shape. The cost is a `Fix` node with a
  CBV-safe reduction rule.

  The ordering ambiguity remains, with a different consequence, and that is what to model. With a
  positional tuple the bundle's hash is invariant under group automorphisms (renumbering the
  projections undoes the permutation). The collision case then leaves only an arbitrary choice of
  which projection carries which name, giving two structurally distinct but observationally
  identical definitions instead of a rejected ingest. With a labeled record, minted labels make the
  choice. That restores presentation-independence but moves the question to name-keyed mint
  stability, the mechanism of Unison's `loadUniqueTypeGuid`.

  Option B already has a prototype (merged here from "Cross-branch reconciliation", 2026-08-07). The
  design doc says B "stays a thread" because *"No prototype has needed recursion"*, and that
  *"Evaluating B properly wants a prototype with `Fix` plus the existing record machinery."* p19 is
  that prototype: isorecursive `Mu` with explicit `fold`/`unfold`, `Fix` for term recursion, `Record`
  and `Variant`. It reached the same conclusion independently, in a source comment: *"content
  addressing chooses iso over equi: you cannot hash a cycle without a fixpoint or compare two without
  a bisimulation"*, with `Mu(body)` acyclic and hashing fine. The doc's premise is superseded. Before
  promoting B from thread to choice, check whether p19's `Mu`/`Fix` is B1 (positional) or B2 (labeled)
  and whether it exercised the mutual case or only the self-referential one. For the mechanization
  this is mainly reassurance: option A is the one that would move `Σ`, `wf`, `callers`, ρ and
  `thm:alpha`, and the evidence points away from it.

  Three modeling questions follow. **(i)** Which closure does the recursion rung model, given that
  only Unison's perturbs `Σ`? **(ii)** Can the automorphism-invariance claim be stated and proved (*if
  σ is an automorphism of the group's reference digraph, the bundle term is fixed by σ*), since it is
  what makes the positional encoding total? **(iii)** Does `thm:alpha` need restating for groups?
  Hashing collapses α-equivalence and group automorphism, but not observational equivalence of
  members.
- **Hash-transparent `Ref` vs. opaque `Ref`.** Unison's reference node contributes the referent's hash
  to its parent (`unison-hashing-v2/src/Unison/Hashing/V2/Term.hs:135-146`). The source read concluded
  that `y = x` and `x = 1+1` are one object and inlining is hash-preserving. *(Corrected 2026-08-25:
  that holds at term level only; at definition level component hashing separates them,
  `rem:transparency-limit`. See `decisions.md` 2026-08-25 (fourth) and 2026-09-02.)* Both systems keep
  the indirection, so the 2026-07-23 "vacuous without indirection" rationale stands. But arbor's
  `\nRef{h}` has its own encoding, so the factoring the author chose is part of identity. Is that
  deliberate? `docs/design/12-type-abstraction.md`'s projection-inlining dependency model needs
  arbor's answer, and `thm:alpha` should then say it collapses α-equivalence and nothing more. See
  also the `Pos = 0` question above.
- **Type-in-hash vs. Θ.** Unison hashes a term wrapped in its type (`Hashing/V2/Term.hs:92-109`), so a
  signature change is an identity change and a cached type cannot be stale. arbor-stlc's
  observational Θ reads as the obvious choice. It is the choice the holes line forces, since only a
  permissive store can hold `Type_with_holes` and ill-typed terms. One sentence in arbor-stlc §1
  should say so.
- **`H` vs. `Causal`: the branching rung now has a target.** arbor's `H` is per-name, flat and
  append-only. It cannot answer "do these two states share an ancestor?", which every merge asks.
  Unison's `Causal` can, and ships with an algebraic spec its implementors wrote out
  (`parser-typechecker/src/Unison/Codebase/Causal/Type.hs:24-42`): five operations, `before` a partial
  order, `merge` commutative but not associative, `sequence` derived. Two structural choices make it
  work: parents are a set, and a namespace child maps to a child causal, so every subtree has
  independent history the parent pins. The "Branching / merging" rung should be faithful to this.
- **Migration by ρ vs. by print-and-reparse, and a claim worth making.** Unison's `update` renders
  affected dependents to source, re-parses and re-typechecks (`Update2.hs:424-427`, which calls itself
  *"the world's weirdest implementation of AST substitution"*). That is correct only if printing then
  parsing preserves hashes. Unison does not state the property; it tests it over a regression corpus
  (`unison-src/transcripts-round-trip/reparses-with-same-hash.u`). `prop:print-elab` is that property,
  proved. This positioning claim is sharper than "first formal treatment" because it names a mechanism
  in a shipping system. Two caveats: the propositions are stated for arbor's languages, not Unison's,
  so the argument concerns the shape of the guarantee; and arbor's own `Migrate` is a structural
  rewrite that needs no round-trip lemma, so arbor proves something it does not itself depend on.
- **Does `E` need a `checkCacheability`-shaped side condition?** Unison caches a result only if its
  type has no arrows, *"since top-level definitions can't have effects without a delay"*
  (`unison-runtime/src/Unison/Runtime/Interface.hs:463-475`), and never caches effectful watches. `E`
  is total here because the language is pure and a value is just another closed term. A remark should
  say so, since the guard returns once either changes: effects, or a value representation distinct
  from the term representation.
- **Stability under transfer, as its own corollary.** `thm:stability` is stated for store growth.
  Unison relies on the same fact across codebases: test results sync, so "is this branch passing"
  becomes a set intersection instead of a computation (`docs/testing.markdown`). Same theorem,
  different quantifier, and the one the commons argument in `docs/related-work/07-commons.md` needs.
  *(Closed 2026-08-07: proved as `cor:transfer`, `decisions.md` 2026-08-07.)*
- **Provenance without mints.** Unison distinguishes a human edit from an auto-propagated one by
  syntactic hash: the hash after substituting references with names from a pretty-print environment
  (`unison-merge/src/Unison/Merge/Synhash.hs:1-26`). No second identity axis, nothing to keep stable.
  Its stated limit is renames, which is where a mint wins; the two fail in complementary places. The
  mint/thread rung should say why arbor wants an identity axis and not a quotient. Unison's minting
  also needs a side table to survive merge (`namespace_unique_type_guid`), a case p11's threads do not
  handle either. *(Modelled since in mini-unison, merged here from that section: synhash is
  `def:synhash`, the dual of the content hash (same recursion, names substituted for references
  instead of the reverse), and it is never stored. A mint says two definitions are meant to be the
  same thing; a synhash says they look the same. Unison has both, uses the mint only for types, and
  uses the synhash for one decision.)*

## Cross-branch reconciliation (2026-08-07)

Found when the `related-work` line merged into the mechanization line. The p19 / option B item has
moved into "Is `Σ : Hash ⇀ Node` the right key type?" above.

- **A stale note about this directory.** `docs/design/open-questions.md` §"Rich editors" (from p20)
  says *"`formalism/` currently holds only compiled PDFs — `arbor-core.tex` and `macros.tex` appear in
  the build logs but are absent from disk, and the directory is untracked."* That was true when
  written. The sources and a `--safe` Agda development are now tracked. The same bullet defers a
  rich-editor formalism rung "until the shape settles", which still stands, but its stated reason
  should be the shape, not the absence of a formalism.

## Questions raised by the mechanization (2026-08-05)

- **Should `wf` gain a hash-keyed clause?** `thm:mono`'s proof argues "each key is ⌈n⌉ for the very
  node n stored, so any collision on a key is with an identical node", which presupposes that a stored
  entry sits at its own hash. No clause of `def:wf` says so. The Agda development isolates it as
  `HashKeyed` and carries it as a configuration coherence clause, not a store one, since `ingest` and
  the rewrite are the only things that establish it. Options: add it as `wf` clause (0); leave it in
  coherence; or make it unstatable by construction, with a `Store` that pairs each entry with a proof
  it is keyed by its hash. The third would delete the invariant but changes `def:store`.
- **`thm:stability` is stated with two unnecessary premises.** `wf(Σ)` and `wf(Σ′)` are unused; the
  proof needs only `Σ ⊆ Σ′`. Keep the paper's form (harmless, and the premises may matter in a rung
  where evaluation reads more of the store), or weaken the paper to what is true? Leaning: weaken it
  and note the strengthening, which makes the dependence on immutability alone clearer.
- **Two lemmas the paper uses without stating.** `ingest-⇝` (reconstructing an ingested term returns
  it) is used by `Cache-sound`'s premise and by `lem:cascade-order`'s re-ingestion step. `elab-func`
  (elaboration is a partial function) is claimed in `def:elab`'s prose but never numbered. Both are
  proved. Should they be numbered lemmas in the paper?
- **Statable-only-after-construction statements.** `lem:cascade-order` and `thm:migosc` clause (b)
  quantify over the construction of ρ, not its specification, so they have no Agda type until M3.
  `Meta.agda` marks them `[deferred, M3]` instead of giving weaker types that would read as coverage.
  Is that the right convention for the label mirror, or should the paper state them
  construction-independently?

## Questions raised by Milestone 2 (2026-08-05)

- **Should `lem:closed-no-stuck`'s repair be a `wf` clause instead?** The lemma now takes "t's
  references denote closed terms" as a premise. A `wf` clause "every stored hash reconstructs to a
  closed term" is false for shallow storage, since `dom(Σ)` contains open subterm nodes. A third
  option: distinguish, inside `wf`, the hashes that are reference targets (already closed by clause
  (ii)) from arbitrary stored hashes, and restrict evaluation to the former. That is close to
  reintroducing a definition sort, which `decisions.md` 2026-07-30 rejected as mint-flavored. Revisit
  when mints arrive, since the mint successor may want this distinction.
- **Does `Eval`'s new closedness premise belong on `Ingest` too?** `Ingest` already requires its
  term's references closed, so ingest is fine. Nothing in the transition stops a name being bound to a
  hash whose reconstruction is open, but coherence (b) requires `closed_Σ(h)`, which rules it out. The
  invariant holds. The question is whether the three closedness side conditions (`Ingest`,
  `Bind`/`Rebind`, `Eval`) should be factored into one notion. Leaning: yes, once the typed rung shows
  whether its analogue factors the same way.
- **ρ's accessibility-proof dependence.** Defined by `Acc` recursion, ρ takes an accessibility proof,
  so `ρ h` is a function only up to an irrelevance lemma (`rho h a₁ ≡ rho h a₂`, by double `Acc`
  induction). Alternatives: derive a fuel bound from the store's finiteness and define ρ by fuel
  (simpler, but reintroduces the fuel/relation split the development otherwise avoids); or use
  `Acc`-irrelevance from a propositional-truncation argument. Decide when M3 starts. *(M3, 2026-08-07,
  kept `Acc` recursion with the irrelevance lemma `rho-irr`; `decisions.md` 2026-08-07.)*
- **Finiteness as a separate predicate vs. part of `def:store`.** The paper says Σ is a finite partial
  function, but only migration and the decidability propositions use finiteness. `def:cascade` in
  particular quantifies over `dom(Σ)` and registers an image, both of which need enumeration, yet
  finiteness is never invoked as a premise (this merges the M3 sub-question "Does `def:cascade` need
  `Finite Σ` stated as a premise?"). The mechanization adds `Finite σ` (support list + completeness)
  as a separate record assumed where needed, and M3 used it only for registration. Should the paper
  follow and say which results need finiteness? Leaning: yes. It is a real dividing line, and
  `arbor-stlc`'s decidability results sit on it too.

## Mechanization questions (Agda)

- **Store representation.** `Data.AVL` map vs. association list vs. a function with a finiteness
  proof. Trade-off: decidable membership and `wf` decidability vs. proof ergonomics. *Closed
  2026-08-05:* a bare function `Hash → Maybe Node` with decidable key equality. Inclusion then reads
  as `thm:mono`/`thm:stability` state it, and the two halves of `thm:mono` collapse into one.
  Finiteness deferred to M3, where the rewrite must enumerate `dom(Σ)`; a `support` field joins the
  record then. See `decisions.md` 2026-08-05.
- **Intrinsic vs. extrinsic scoping.** Intrinsic `Term : ℕ → Set` (vars `Fin n`) gives closedness and α
  for free but complicates `shift`/`subst` with `Fin` arithmetic. Extrinsic `Term` + a separate
  `wf`/closedness predicate is closer to the p4 code. Lean: intrinsic. *Closed 2026-08-05, against the
  lean: extrinsic.* Shallow storage puts open subterm nodes in `dom(Σ)` (`rem:nosort`), so
  `reconstruct` returns open terms, and an indexed `Term` would force a non-canonical level
  existential into its codomain (`var 5` inhabits `Term 6`, `Term 7`, …). α stays definitional
  regardless, since it comes from de Bruijn, not the indexing. See `decisions.md` 2026-08-05.
- **How much to mechanize first.** T1–T4 (identity, no-silent-breakage, monotonicity, wf) as the first
  milestone; T5–T8 (eval stability, migration) as the second. Confirm staging. *Closed 2026-08-05,
  restaged:* the T-numbering is retired (it appears in neither paper; paper labels are the only names).
  M1 is `thm:alpha`, `thm:nsb`, `thm:mono`, `thm:stability`, `cor:cache` plus `lem:determinism`,
  `lem:elab-premises`, `prop:print-elab`: both headline theorems, because `thm:stability` needs only
  reconstruction monotonicity while `thm:wf` needs the whole acyclicity argument. `thm:wf` moves to M2
  with the safety lemmas; migration is M3. See `agda/README.md`.

## The M3 wall (2026-08-05) — cleared 2026-08-07

**Resolved.** Both halves of the difficulty below were artefacts of an incremental proof strategy,
not facts about the construction (`decisions.md` 2026-08-07; `agda/Arbor/Core/Rewrite.agda`'s
closing note). The fold needs no dependency order: every image is registered somewhere, and
`ingest-tgt-big` pins the conclusion to the final store. ρ's non-injectivity does not block
acyclicity: the proof follows edges forward and never chooses a preimage, and a colliding hash
carries the same node by (★), hence the same out-edges. `lem:cascade-order` is therefore not a
prerequisite. It remains what the paper calls it, a bridge to p11's sequential implementation
(`rem:noorder`). The original entry is kept below.

### As originally recorded (2026-08-05)

ρ is defined (`agda/Arbor/Core/Rewrite.agda`); registering its image and proving `wf(Σ′)` are not.
The blocker is a fact about content addressing, not a proof-engineering annoyance.

**ρ is not injective.** Two distinct entries can rewrite to the same hash. That is content
addressing, and `thm:alpha` is the same phenomenon read positively. So `wf` clause (iii) does not
transfer along ρ: a cycle among ρ-images need not pull back to a cycle in Σ, as it would under an
injective map. The paper's argument is the registration order ("ordering new nodes by registration
extends any topological order of the old reference graph", `thm:wf`'s sketch), so the fold that
registers the image must run in a dependency order (`def:deporder`) and the acyclicity proof must
read that order back. That is `lem:cascade-order`'s machinery, so the two should land together.

Open sub-questions:

- Is there a formulation that avoids the order argument? Candidate: prove acyclicity of Σ′ with a
  rank function (e.g. reference-graph depth in Σ, transported along ρ) that every Σ′-edge strictly
  decreases. This needs ρ-images of distinct-rank entries to stay distinct, which is not obvious, but
  it replaces a global order with a local decrease, which mechanizes far better.
- Does `def:cascade` need `Finite Σ` as a premise? Merged into "Finiteness as a separate predicate"
  under Milestone 2.
- Should `def:deporder` and `lem:cascade-order` be promoted from "bridge to p11's implementation" to
  load-bearing parts of the construction? On this evidence they are load-bearing. *(Answered no by
  the resolution above.)*

## The arbor-stlc reuse boundary — settled (2026-08-07)

**Closed by construction.** The answer is neither option below: abstract the syntax, not the nodes,
and do not duplicate. `Term` is now the fixpoint of a signature (shapes, each with a structural and a
reference arity). The store layer is proved once over it, and arbor-stlc's two-sorted entry is a
second instance. `def:core`'s four constructors survive as pattern synonyms, so neither paper's
syntax changed. See `decisions.md` 2026-08-07 and both papers' mechanization sections. The original
entry is kept below.

### As originally recorded

`decisions.md` 2026-08-05 §6 recorded the boundary as decided: `agda/Arbor/NodeSig.agda`, a record
over an entry's structural children, reference children, and a map over both. Retracted (see
`decisions.md` 2026-08-07, "Retracting the reuse-boundary decision"): the record is instantiated by
the untyped node and consumed by nothing, because every graph-layer lemma routes through `σ ⊢ h ⇝ t`,
whose right-hand side is a deep term. Abstracting over `⇝` (with `⇝-func`, `⇝-mono`, `⇝-∈dom` as
fields) would be consumed.

That leaves M4 with two shapes:

- **Refactor `Term` as the fixpoint of a functor** and share the store layer between rungs. Highest
  payoff, and it makes a node-level signature real, but it touches every proof and all seventeen
  discharged statements would need re-checking. It is not only an engineering question: the paper's
  `Term` would become a fixpoint too, or else a visible gap would open between paper and
  mechanization where there is none.
- **A parallel `Arbor/Stlc/` hierarchy**, sharing `Prelude`, `Hash`, and, with the modest `⇝`
  abstraction, the graph lemmas. Duplicates perhaps 60% of the store layer but leaves M1–M3
  untouched.

Under the second option the duplicated parts are `ingest` and its four `ingest-*` lemmas, and most of
`Preservation`. The typing judgment, Θ, the residual and the clean oracle are new work either way,
and they are the substance of the rung.

A third possibility, if the answer is "neither yet": mechanize arbor-stlc's aspect half first
(`lem:ty-stable`, `thm:typeof-stable`, `cor:aspect-agree`). These are typed twins of lemmas already
proved and need the store layer only as a parameter.

## The Term-as-signature refactor: spike results (2026-08-07)

The shape was spiked before committing: signature fixpoint, `⇝` relationally, `⇝-func`, `ingest`,
`substRefsD` with its membership plumbing. All of it typechecks under `--safe`. The spike settled
three things:

- **Two position counts.** A node's children are all hashes; a term's structural children recurse
  while its reference children stay hashes. The signature carries `sArity` and `rArity` separately.
  `Node H` is the shape with both filled by `H`; `Term` is the shape with structural positions
  recursive. That is the store's shallow/deep split, expressed once.
- **Arities, not position types.** With `SPos s → Term`, functionality of reconstruction needs
  function extensionality to equate two children functions, and `--safe` does not provide it.
  `Vec Term (sArity s)` keeps equality structural and the proof goes through.
- **It is shorter where it matters.** `⇝-func` is 8 lines generically against ~30 concretely, because
  the 16-case absurdity explosion (every pair of distinct constructors) collapses into one
  shape-equality step. We expected the opposite.

**The open question is the paper, not the proofs.** `def:core` is four constructors, and the
mechanization matches it one for one, which is the artifact's main virtue. A signature fixpoint
either drags the paper along, making `def:core` markedly worse to read, or opens a presentation gap
between paper and mechanization. The gap is survivable and documentable, since it is presentational
and the instance is provably the same datatype. It is the cost the shared-store-layer benefit has to
beat, and it should be decided explicitly. Leaning: keep the paper as it is, state the correspondence,
and have the core instance carry a comment showing the four shapes.

Everything about binders stays concrete either way. Scoping (`⊢ₙ`), `shift`, `subst`, `beta` and their
lemmas are language-layer, and the signature does not model binding. The generic store layer must
therefore take `Closed` as a parameter, since `wf` clause (ii) is stated in terms of it.

## Does the two-sorted rung fit the generic layer? (spiked 2026-08-07)

Asked because ordering mattered: with the Core instantiation parked half-done, building arbor-stlc
first would have been costly if it forced a change to `Sig`. Spiked, and it fits unchanged.

- **`Sig` needs no sort index.** Both sorts are shapes over the same hash space. That is the paper's
  position: `def:entry` has one injective ⌈·⌉ over the whole sum and derives the sorts' disjointness
  as a corollary, not an axiom. Nine shapes: seven term, two type.
- **Sort discipline is a predicate, not an index** (`IsTm`/`IsTy`). This diverges presentationally
  from `def:reconstruct`, which folds sort discipline into definedness by giving `reconstruct` and
  `reconstructTy` separate partial functions. One relation plus a predicate says the same thing. The
  rung should carry a remark, not a silent change.
- **"Type entries are isolated vertices of the reference graph"** (`def:reconstruct`) holds by
  construction. Type shapes have reference arity zero, so `refsT` on a well-sorted type is `[]` by
  induction, and they have no callers with nothing assumed.
- **Closedness stays one predicate over both sorts.** A type has no variables, so it is closed at every
  level, and the generic store layer's `Closed` parameter need not split.

Consequence for sequencing: finishing the Core instantiation and building `Arbor/Stlc/` are
independent. What remains of the Core instantiation is mostly not store-layer work (`Eval`, `Naming`,
`Cache`, `History`, `Config` barely touch the generic machinery), so it validates little about the
generic layer and is grind toward a single source of truth. The parked branch is
`formalism-core-instantiation`.
