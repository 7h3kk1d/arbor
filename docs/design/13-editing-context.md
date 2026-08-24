# Editing Contexts

**Status:** Notes. Opened 2026-08-19; extended the same day after a design conversation. Nothing here is a commitment — this file holds the shape of the idea, the positions taken so far, and the questions still open. Sources: `../systems/unison/11-scratch-file.md` (source read of Unison's scratch file), `../related-work/12-editing-contexts.md` (literature, including Jujutsu and IntelliJ changelists).

## The ask, in three claims

An **editing context** is what a scratch buffer is trying to be. Three separable claims:

1. **A collection of definitions you are simultaneously looking at and editing**, held open across many operations, members visible to each other, **updating live with no explicit commit gate** — so a mistake in one term does not cost you the others.
2. **The context supplies the evaluation environment.** Live feedback is computed against context-bound inputs.
3. **Migration policy is scoped by what you are currently interacting with** — what propagates, what does not, and where the breakage lands.

Claim 2 was originally written here as "the system optimizes for what is checked out," and that was wrong; see §"Claim 2 is a binding question, not a caching question."

## Name collision to resolve

**arbor already uses "editing context" for something else.** p12 introduced it and p13–p17 carry it: an *open set of abstract-type hashes*, which decides whether an `Opaque{mint, witness}` unfolds and hence whether a commit produces a `Seal` (`../prototypes/p12-abstract-types/00-scope.md`, `12-type-abstraction.md`). That is a **capability**, not a working set.

**Position taken (2026-08-19):** the two are plausibly in 1-1 correspondence but need not be forced into it, and the question is left partially open — with the observation that the real work here is **UX/visual**: making it obvious which representations the current context has opened, alongside what it contains. IntelliJ's changelist marking (a yellow stripe on files belonging to an inactive changelist, red filenames in tabs and the tree) is the shipped precedent for exactly this kind of always-visible context indicator; `../related-work/12-editing-contexts.md` §"Concurrent uncommitted edits".

## What the repo already says

- `open-questions.md:74` already carried an **Editing context** entry — "a set of bindings the user is treating as simultaneously in-flight before any are committed to the namespace," motivated by co-dependent edits (`f` and `g` changing together). This document is that entry, opened up.
- `open-questions.md:66` asks what the first editor-shaped interface is, listing "scratch-buffer-style" first.
- `../systems/unison/06-pl-notes.md` flagged the scratch file as the same job as p12's editing context and deferred the read. Done: `../systems/unison/11-scratch-file.md`.
- `04-naming-layer.md:130-137` frames update strategies on **scope** × **user-in-loop**. Claim 3 is the observation that *a context is a natural value for the scope axis* — a strategy between Pin (scope: none) and Follow (scope: all reachable callers), namely **scope: what I have checked out**.

## No commit gate — the central requirement

The requirement stated most strongly in conversation: you want to **live-update several definitions and watch values update without going through an explicit commit step**, so that a mistake in one term does not break the others or force a rollback.

Two systems already do this, from opposite directions:

- **Jujutsu** removes the gate by making the working copy *a commit that is continuously re-snapshotted* — "Jujutsu will automatically create commits from the working-copy contents when they have changed." There is no staging area; commit is not an event. Recovery comes from the **operation log**, a DAG over repo operations, so nothing can be lost by editing.
- **Hazel** removes the gate by making incomplete programs *meaningful* — a broken subterm becomes a hole and everything else keeps evaluating (`../related-work/02-structure-editors.md`; p8/p9 carry this into arbor).

**arbor's version writes itself, and it is cleaner than either.** Because the store is immutable and ingest is permissive (`feedback: visible breakage, not gates`), *everything is already committed to the store the moment it is typed.* Nothing can be destroyed, so nothing needs to be gated. What remains uncommitted is not the definition — it is the **name binding**. So:

> **A context holds hashes (always ingested) plus provisional name bindings not yet published to the namespace.** Live evaluation resolves through the provisional bindings. "Commit" means publishing some of them. A mistake in one term is local because the old hash is still bound in `N` and the store never lost anything.

This matches the position taken on ingest: *ingest everything; what may be uncommitted is names.* It is also the sharpest available argument that arbor's permissive-ingest posture pays for itself — the thing that looked like a philosophical stance turns out to be the mechanism that makes gate-free live editing sound.

## Two sets, not one: the frontier and the commit set

The conversation drew a distinction the original notes did not have. A migration might not commit at all; it might instead **add to the context** until you are done. And "what you currently see" and "what you are committing" should be **slightly distinct**, so you can watch the frontier and its impact, and commit as you go.

So there are two sets, and conflating them is the mistake:

- **The frontier / view** — what is visible and live: your edits *plus* their blast radius. It is largely **derived** (dependents of what changed), it **grows** as a migration discovers more, and it is what live feedback is computed over.
- **The commit set** — what you intend to land. Largely **curated**, a subset of the frontier, and committable **incrementally** rather than all at once.

Precedents, all three of which keep two sets:

- **IntelliJ**: changelists are curated commit sets; problems and inspections are the derived view. Exactly one changelist is *active* (the default target for new modifications) — a nice answer to "which set does an edit join by default."
- **jj**: `@` (the auto-snapshotted working copy) versus **revsets** (queries selecting sets of revisions). Membership by query, not only by enumeration.
- **Unison**: the live region above the fold versus the whole buffer — and, on a failed `update`, the broken dependents are *removed from the namespace and written into the buffer*, which becomes their only home (`../systems/unison/11-scratch-file.md`). The frontier is delivered *as* the working set. That is claim 3 already shipped, and it is undocumented in Unison.

This also settles what happens to arbor-stlc's **residual**: it is not an error report, it is *the next frontier*. Pijul and jj both make conflicts first-class values in the store rather than markers in a file; the same move.

## Claim 2 is a binding question, not a caching question

The original note framed this as "the system optimizes for what is checked out," was skeptical, and cited Denning. The conversation supplied the actual motivation, and it is a different claim:

> If I have a dataset-analysis algorithm and it runs over 5 PB, that takes a while. But if the dataset in my editing context is 5 rows, that is what I run on every keystroke.

That is not caching. Running against a 5-row sample is not an approximation of running against 5 PB — it is a **different computation with different inputs**. Which means:

- **It cannot threaten evaluation stability.** A context-scoped optimization keyed on caching would have to be sound independent of the context or `thm:stability` breaks. Input binding raises no such obligation: the result is keyed on the hash of *the term applied to the context's inputs*, which is a different hash. The eval cache does not need to know contexts exist.
- **The right citation is Lerner's loop seeds, not Denning.** "Focused Live Programming with Loop Seeds" (UIST 2020) names the **loop-datavoid problem** — live programming needs runtime values, and for incomplete code no test input produces useful ones — and answers it with a small user-supplied **seed**. That is this claim, published. Projection Boxes (CHI 2020) is the display half.
- **arbor already has a rung of this.** p15–p17's `test` aspect is an interface-level convention evaluated live. A context-supplied sample input is the same shape generalized: the context binds names to *small* values, and live feedback evaluates against those bindings.

The caching version of claim 2 is not dead, but it is a separate, later, and weaker claim, and it should be argued from a measured slow query rather than assumed.

## The simplest mechanism

Per `CLAUDE.md`'s posture, the minimum that answers all three claims:

> **A context is a named set of hashes plus a set of provisional name bindings, with a migration strategy attached.**

Everything else derives: the resolution overlay (provisional bindings shadow `N`, as Unison's `Names.shadowing` does), co-dependent edits (from the overlay), migration scope (the set is the value of `04-naming-layer.md`'s scope axis), residual delivery (a failed migration extends the frontier), evaluation environment (provisional bindings include sample inputs), and — if the collision resolves that way — p12's opacity open set as one more field.

Whether the object lives in the Store (a sort, content-addressed), in the Attachment layer (an aspect, or a map beside `N`), or in the Interface layer (`06-architecture.md`'s default) is the first real design question. Interface-only is defensible for a first prototype and forecloses persistence, sharing, and agent handoff — which is what CitC and jj workspaces exist to provide.

## Axes, with positions where taken

1. **Stored or ephemeral.** Leaning **stored**, after the jj read: jj keeps the context map inside the same content-addressed `View` as the namespace, and states the layering rule that makes it work — *"the repository doesn't know where on disk any working copies live"*, so the *desired* state is shared and versioned while only the *materialization* is local. CitC (server-side named workspaces) is the same answer at scale.
2. **Membership: explicit, inferred, or queried.** *All three worth exploring* — but the jj read supplies a fourth answer that subsumes them, and it is in a design document rather than a guess. `docs/design/sparse-v2.md`'s first goal is *"Sparse Patterns should be versioned with the working copy,"* stored in the op store and referenced by a `WorkingCopyPatternsId` from the active `View`, with an **ordered include/exclude rule program** rather than a set. Membership as a stored, versioned rule program hanging off the namespace object. (Caveat: proposed, not confirmed shipped; and its motivation is *materialization*, which arbor does not have.) Revsets are the same instinct on the commit axis.
3. **One or many, named or anonymous.** **Many, definitely** — agents need concurrent unrelated contexts. Naming follows. This is the axis IntelliJ changelists and jj workspaces both answer, and they answer it differently: changelists share one working copy and disambiguate by an *active* list; jj workspaces get separate working copies over one repo, with **staleness** as the interference primitive.
4. **What is a member: a hash, a name, or a binding?** Current lean: **hashes**, with a migration strategy attached — but the two-set distinction above means the frontier may need to carry *bindings*, since what breaks is a name's meaning. Note that **jj's change ID is arbor's mint thread, shipped**: an identifier that "tracks a commit as it evolves through rewrites," with a named *divergent* state when one change has several visible versions. If contexts need to track a definition across edits, this is the prior art, and it re-opens the mint/thread axis p12–p17 dropped.
5. **Uncommitted state.** **Settled: ingest everything.** What can be uncommitted is names, not definitions. See §"No commit gate".
6. **Relationship to branching.** Branching is expected to live **at the namespace level**; a context is what a person is actively working on and the feedback they get in-editor. The relation is real and under-explored — jj (workspaces vs. bookmarks vs. the op log) and the VCS literature are where to dig.
7. **Atomicity of commit.** Not atomic. Incremental, per the frontier/commit-set split, which matches the recorded commit-and-report posture.
8. **Overlap between contexts.** Open, with three shipped answers to choose among. IntelliJ: **detect, mark persistently, offer named resolutions** (shelve / move to active / switch active / ignore). jj: **let them diverge, name the state** (*stale*), **provide a repair command** — cheap because the desired state is in the shared `View` and only materialization is local. Grove: conflict-free by construction. jj additionally shows a fourth possibility one level down — make the *name* multi-valued so overlap is representable rather than exceptional; see below.

## Toward a formalism (later)

arbor-core's configuration is `⟨Σ, N, E, H⟩`. The natural extension is `K` — a finite map from context name to (a member set, a set of provisional bindings, a strategy).

**Where `K` goes was answered by the jj read (2026-08-19).** jj's `View` object holds `bookmarks` (name → target) and `wc_commit_ids` (workspace name → checked-out commit) as **sibling fields of one content-addressed value**, versioned by the operation log — i.e. `⟨Σ, ⟨N, K⟩, H⟩`, not `⟨Σ, N, E, H, K⟩` with `K` bolted on. `../systems/jj/04-arbor-mapping.md`. Consequences: axis 1 resolves toward *stored* almost for free (if `N` is versioned at all, a context map costs nothing structurally); contexts and namespace branching share a home rather than competing (open question 6); and the inertness statement below becomes the same *kind* of statement as `thm:nsb`, over the same component. The caveat: jj's map sends a workspace to **one commit**, because a commit is a whole tree. arbor's members are individual definitions, so the *location* transfers but the *payload* does not.

Statements worth wanting:

- **Conservativity of overlay resolution.** A name that resolves outside every context resolves the same way inside one, so `thm:nsb` survives contextualization.
- **Inertness.** Opening, closing, or renaming a context changes no term's meaning — contexts are as inert as names are. If this fails, the object is wrong.
- **Scoped migration.** `04-naming-layer.md`'s Follow restricted to a context. arbor-stlc's `thm:migosc` (residual exactness) should become a *restatement* with the scope instantiated to a context, not new content — that would be strong evidence the object is the right one.
- **Non-interference of disjoint contexts.** Two contexts touching disjoint parts of the store cannot affect each other. This is the arbor-shaped version of patch independence, and the place to borrow from: Mimram & Di Giusto's pushout characterization (MFPS 2013) and Angiuli et al.'s homotopical patch theory (ICFP 2014).

Claim 2 imposes no soundness obligation on the cache, per §"Claim 2 is a binding question" — worth stating explicitly in the eventual paper, since the opposite is the natural worry.

## Multiplicity belongs to namespaces, not to bindings

The jj read raised a question independent of editing contexts: jj's namespace is **multi-valued** — a bookmark's target is a `RefConflict` of `removes`/`adds` terms, so a name moved to B in one operation and C in a concurrent one is recorded as *"moved from A to B or C"*, shown `??`, and only commands needing one answer fail. Unison is the same shape: `Names` is a `Relation Name Referent`, conflict is representable, and unconflictedness is a *derived view* operations demand when they need it (`../systems/unison/03-namespace-and-history.md` §"Names map to sets").

**Position taken (2026-08-20): `N` stays single-valued.** A name denotes one hash. Where multiplicity is needed it belongs to a **family of namespaces** — one per branch, and/or one per agent/user, giving each participant its own single-valued view — not to a family of targets inside one namespace. If conflict resolution then needs a construct that holds several candidates, that construct is its own thing and is deferred; it is not `N`.

Three pieces of evidence for the factoring, all from systems already read:

- **Unison built multi-valued and had to add the gate back.** `handleUpdate2` asserts unconflictedness up front and bails with `Output.ConflictedDefn` (`.../Update2.hs:99-102`). A representable state that half the operations refuse is a gate wearing a different hat.
- **Unison's own next-generation format drops it.** `BranchV3`: *"A V3 branch is a trimmed-down V2 branch: **Names can't be conflicted.** Metadata doesn't exist. Patches don't exist"* (`codebase2/codebase/U/Codebase/BranchV3.hs:16-30`). All three removals are on the same list as the machinery Unison spent three years deleting (`../systems/unison/05-deletions.md`).
- **jj already has the family, and the conflict exists only where the family doesn't.** `View.remote_views` is a list of *named* namespaces, each with its own bookmarks and tags (`lib/src/protos/simple_op_store.proto:119-123`). The conflicted `RefTarget` is needed for the **local** map specifically because it is singular and absorbs concurrent operations. Make the family primary and most of the pressure for conflicted bindings goes away.

**What this changes for `K`.** It sharpens the jj mapping rather than contradicting it. jj's `⟨N, K⟩`-in-one-object becomes, for arbor, a family: a context (or branch, or agent) names *both* a member set and the single-valued namespace that resolution goes through. The overlay in §"No commit gate" — provisional bindings shadowing `N` — is then not a special case; it is what it means to be a distinct namespace in the family, and `Names.shadowing` is the composition operator.

**What stays open.** Whether contexts, branches, and per-agent views are one mechanism or three; and what the conflict-resolution construct is, if one is needed. Both deferred, deliberately.

## Open questions

1. Does p12's opacity open set become a field of this object, or a separate one in 1-1 correspondence? Partially open by choice; the deliverable is **UX** — how to make the current context's contents *and* its opened representations obvious at a glance.
2. Stored or ephemeral — and if stored, in the Store (a sort) or the Attachment layer?
3. Is membership an extensional set, a degree-of-interest function, or a **query language** over the store? (jj's revsets make the third live.)
4. Are frontier members hashes or bindings? Does tracking a definition across edits force the mint/thread axis back in — and is jj's change-ID-plus-divergence the model?
5. What exactly is the boundary between the frontier and the commit set, and what is the gesture that moves something across it?
6. Contexts vs. namespace branching: what is the relation, and does one subsume the other? Needs the VCS dig.
7. What does a user see when two contexts overlap? (IntelliJ's four resolutions are the starting point.)
8. Does an agent get a context, and is "hand this task to an agent" the same gesture as "open a context"? (`../related-work/08-agentic-vc.md`)
9. Where do a context's sample inputs come from, and are they ordinary provisional bindings or a distinct kind?
10. Is the caching form of claim 2 ever worth building, and what measured workload would justify it?
11. Are contexts, namespace branching, and per-agent views **one mechanism or three**? The single-valued-`N` position (above) pushes multiplicity into a *family* of namespaces; it does not say what indexes the family. Deferred, and it is the same question as open question 6.
12. If conflict resolution needs an object that holds several candidates, what is it and where does it live? Explicitly **not** `N`. Deferred.
