# Unison — Followups

**Status:** Running list. Opened 2026-08-07 from the source read at `db60ce2`.

Threads this read opened and did not close. **Add, don't silently remove**; when one closes, say what closed it and leave the entry. Items graduate the same way everything else here does — into `../../design/open-questions.md` if the question is arbor's, into `../../design/decisions.md` if it hardens, into a theme file in `../../related-work/` if it turns out to be a citation.

Three kinds of entry, tagged inline:

- **`[read]`** — more Unison source or history would answer it. Bounded and cheap.
- **`[arbor]`** — a design or modeling question the read raised. Not answerable by reading more Unison.
- **`[verify]`** — a claim made here on thin evidence that should be checked before it is leaned on.

---

## Open

### Store and hashing

- **`[arbor]` Evaluate the fixpoint-bundle encoding for mutual recursion.** Worked out in `../../design/03-content-addressing.md` §"Mutual recursion: three ways to close the cycle" — a `Fix` over a product, members as projections, leaving `Σ : Hash ⇀ Node` unchanged. Two variants: **positional** (reuses p17's `Pair`/`Fst`/`Snd`, no mints) and **labeled** (reuses p16/p17 records, one mint per member). Needs a prototype to be more than an argument: a `Fix` node with a CBV-safe reduction rule plus the existing product machinery. The specific things to find out: does the eval cache memoize the bundle usefully, or does every call to a member re-run the fixpoint; what does `callers_of` look like through a projection; how bad is the nesting for an *n*-member group given that `Tnode.Product` is binary; and does the labeled variant's mint-per-member feel real or nominal.
- **`[verify]` The automorphism-invariance claim.** The positional variant's whole advantage rests on: *renumbering the projections exactly undoes a permutation of the tuple slots, so a group automorphism fixes the bundle term.* Verified by hand on the symmetric two-member case only. It should be stated and proved properly — `formalism/open-questions.md` carries it as modeling question (ii) — and it should be checked against a case with a nontrivial automorphism on more than two members before it is relied on.
- **`[arbor]` Is the residual ambiguity worth surfacing?** In the positional variant the bad case yields two structurally distinct, observationally identical definitions, and the substrate picks arbitrarily. That is `feedback_visible_breakage`'s commit-and-report posture applied to canonicalization — but nothing currently says whether the editing layer should *report* that a group had a symmetry and the assignment was arbitrary, or silently proceed.
- **`[arbor]` Does the coarsening actually bite?** Under *all three* recursion options, editing one member of a group changes every member's identity. That is stated as a wash in the comparison table, but none has been exercised. If it turns out to be painful, it is an argument for finer-grained addressing that no approach provides.
- **`[read]` Graph canonization is the right frame and has no entry in `../../related-work/`.** Canonically ordering a recursive group is assigning canonical indices to the vertices of a labeled digraph; the erase-and-sort trick is one round of colour refinement, with the same known incompleteness. That is currently asserted in `03-content-addressing.md` from reasoning, with no citation. `01-content-addressing.md` should probably carry the canonical-labeling / Weisfeiler–Leman line, if only to say precisely why Unison's failure is intrinsic.
- **`[read]` What does Unison's `Cycle` ABT constructor do that the hashing cycle-frame does not?** `U/Core/ABT.hs` has a `Cycle` constructor distinct from the hashing environment's `Left [v]` frame. The relationship between the two was not traced.
- **`[verify]` Is `IncompleteElementOrderingError` reachable in practice, or only in principle?** The failure needs two cycle members structurally identical modulo intra-cycle references. Issue #2787 exists, which suggests someone hit it. Worth reading the issue before citing it as a live defect rather than a theoretical one.
- **`[arbor]` Hash-transparent vs. opaque `Ref`, decided rather than defaulted.** `formalism/open-questions.md` §"Divergences from Unison" carries the question. The concrete thing to settle: does `../../design/12-type-abstraction.md`'s projection-inlining dependency model actually *require* opacity, or does it only need the dependents index?

### Update, merge, branching

- **`[arbor]` Adopt the rendered residual?** `08-merge-and-branching.md` §"How update picks dependents" argues Unison's failure handling is better than arbor's: it evicts broken dependents from the namespace and hands them back as editable source on a dedicated branch, rather than reporting a set of names. `formalism/open-questions.md` already asks whether richer residual reporting is editing-layer elaboration or part of the report; this is the strongest available answer (the residual *is* the code) and it should be argued for or against explicitly.
- **`[arbor]` arbor's update-strategy space has no prior art — say so.** Unison ships Follow and nothing else; there is no pin, and no per-unit choice at selection time (`08-merge-and-branching.md`). `../../design/04-naming-layer.md` §"Update strategies" and `project_update_strategies` should not be read as describing a landscape. Either find a system that does offer pin/explicit, or state that the 2-axis framing is arbor's own.
- **`[arbor]` Can `N` as a partial function represent a merge in progress?** It cannot represent a conflicted name, and Unison's merge produces exactly that state before resolution. The lesson in `08-merge-and-branching.md` §"On conflicts" is that the *intermediate* state of a merge violates the invariants the steady state satisfies. Bears directly on the branching rung in `formalism/open-questions.md`.
- **`[arbor]` Should arbor attempt structural merge of definitions at all?** Unison never does — definitions are opaque to merge, conflicts are per-name, and anything ambiguous becomes text. arbor's stored ASTs would *permit* structural merge in a way Unison's components do not. `../../related-work/05-naming-versioning.md` has the structured-diff literature and `08-agentic-vc.md` records that structural convergence still leaves 5–10% semantic conflicts. Genuine fork, unexamined.
- **`[read]` How does `upgrade` differ from `update`?** `HandleInput/Upgrade.hs` and `upgrade_branch` (`sql/019`) were noted but not read. It is the dependency-bump path and probably the closest thing Unison has to a scoped migration.
- **`[read]` What does `todo` report?** `HandleInput/Todo.hs` is the "what work remains" surface and was not read. Likely the nearest analogue to arbor's `broken` standing query.
- **`[read]` Squash semantics.** `Causal/Squash.hs` and the `squash_results` memo table were noted, not traced. Relevant to whether arbor's `H` needs a compaction story.

### Side-structure and aspects

- **`[arbor]` Weight the asserted/derived split in `02-definitions-and-derived-data.md`.** The core claim from `09-side-structure.md`: what survived in Unison is keyed by **hash** and written by **the system**; what died was keyed by **name** and written by **the author**. arbor's existing aspects are all the surviving kind. The question is whether `02` should say that the two halves want different mechanisms, and whether an author-asserted name-shaped fact should just be a definition with a name.
- **`[arbor]` Say explicitly that arbor's aspects are outside the content hash.** `09-side-structure.md` argues that one of the four forces that killed Unison's metadata — being *in* the namespace hash, so annotating became a structural change — does not apply to arbor, because `E` and `Θ` are modeled as separate derived stores. That is currently an accident of the model rather than a stated property. Cheap to write down; makes the comparison defensible.
- **`[arbor]` What happens to categories that lose their home?** Unison dropped authorship and licensing outright when metadata went — `create.author` still produces definitions with nothing to attach them to. If arbor concludes "name-shaped facts should be names," it owes an answer for the categories that then have nowhere to live, rather than repeating the silent drop.
- **`[verify]` The entire "why" in `09-side-structure.md` is inference.** No design rationale was written down anywhere in the Unison tree; the argument is built from what was removed, what replaced it, and the design doc's own motivating list. Two specific weak points: "nobody used it" is inferred, not observed; and the counterfactual (would it have survived if hash-keyed and outside the namespace hash?) is untested. Worth asking the Unison developers directly before the argument is leaned on in a paper.
- **`[read]` Were `link`/`unlink` ever widely used?** Would settle force 3 in `09-side-structure.md`. Public codebases on Share, or the removal PR's discussion (#4574), would show it.

### Formalism

- **`[arbor]` Stability under transfer as a corollary.** `thm:stability` is stated for store growth; Unison relies on the same fact across codebases, which is what the commons argument actually needs. Noted in `formalism/open-questions.md`; cheap to add beside `cor:cache`.
- **`[arbor]` Does `E` need a `checkCacheability`-shaped side condition?** Filed. The answer is probably "not until effects or a distinct value representation arrive," but the paper should say that rather than be silent.
- **`[arbor]` Adopt Unison's round-trip corpus as an executable check.** `prop:print-elab` is proved and untested; Unison tests the same property over a regression corpus and does not prove it. A print-then-parse-then-compare-hashes test over a prototype's bootstrap definitions is nearly free and would catch printer regressions the proof cannot.
- **`[verify]` The print/re-parse positioning claim.** `07-arbor-mapping.md` §"Migration" argues arbor has proved the lemma Unison's `update` needs. The caveats are already stated there (arbor's propositions are about arbor's languages) but the claim is load-bearing enough that it should be re-read critically before it appears in a paper.

### Minting

- **`[arbor]` Mint stability under merge.** Filed at `../../design/open-questions.md` §"Minted identity". Unison needed `namespace_unique_type_guid` plus a `makeUniqueTypeGuids` pass; p11's threads have the same exposure and no answer.
- **`[read]` What happens when two branches mint different GUIDs for the same type name and then merge?** The table exists to prevent it, but the actual conflict behavior was not traced.

### Unread areas, if they become relevant

- **`[read]`** The typechecker proper (`Typechecker/Context.hs`, 3,786 lines) beyond its ability-row interface — relevant if arbor's checker grows effects or higher-rank polymorphism.
- **`[read]`** The ANF → MCode → interpreter pipeline — relevant only if arbor needs a real runtime.
- **`[read]`** The Share sync protocol (`Unison.Sync.*`, `SyncV2`) — relevant if distribution ever stops being a non-goal, and the place where `LocalIds` localization pays off.
- **`[read]`** LSP and MCP implementations — relevant to `../../design/06-architecture.md`'s Interface layer and to `../../related-work/08-agentic-vc.md`.

---

## Closed

*Nothing yet. When an entry closes, move it here with what closed it and the date.*
