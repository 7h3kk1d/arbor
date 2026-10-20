# Editing Contexts

**Status:** Notes, opened 2026-08-19 and extended the same day after a design conversation. Nothing here is a commitment. The file records the shape of the idea, the positions taken so far, and the open questions. Sources: `../systems/unison/11-scratch-file.md` (source read of Unison's scratch file) and `../related-work/12-editing-contexts.md` (literature, including Jujutsu and IntelliJ changelists).

## The ask, in three claims

An **editing context** is what a scratch buffer is trying to be. It makes three separable claims:

1. It is a collection of definitions you are looking at and editing together, held open across many operations, with members visible to each other and updating live with no explicit commit gate. A mistake in one term does not cost you the others.
2. The context supplies the evaluation environment. Live feedback is computed against inputs the context binds.
3. Migration policy is scoped by what you are working on: what propagates, what does not, and where breakage lands.

Claim 2 was first written as "the system optimizes for what is checked out." That was wrong; see §"Claim 2 is a binding question, not a caching question."

## Name collision to resolve

arbor already uses "editing context" for something else. p12 introduced it and p13–p17 carry it: an open set of abstract-type hashes, which decides whether an `Opaque{mint, witness}` unfolds and so whether a commit produces a `Seal` (`../prototypes/p12-abstract-types/00-scope.md`, `12-type-abstraction.md`). That is a capability, not a working set.

**Position taken (2026-08-19):** the two are plausibly in 1-1 correspondence, but that need not be forced, and the question stays partly open. The real work is UX: making it obvious which representations the current context has opened, alongside what it contains. IntelliJ's changelist marking (a yellow stripe on files in an inactive changelist, red filenames in tabs and the tree) is the shipped precedent for an always-visible context indicator; see `../related-work/12-editing-contexts.md` §"Concurrent uncommitted edits".

## What the repo already says

- `open-questions.md:74` carried an **Editing context** entry: "a set of bindings the user is treating as simultaneously in-flight before any are committed to the namespace," motivated by co-dependent edits (`f` and `g` changing together). This document expands that entry.
- `open-questions.md:66` asks what the first editor-shaped interface is, and lists "scratch-buffer-style" first.
- `../systems/unison/06-pl-notes.md` flagged the scratch file as doing the same job as p12's editing context and deferred the read. The read is now `../systems/unison/11-scratch-file.md`.
- `04-naming-layer.md:140-147` frames update strategies on two axes, **scope** × **user-in-loop**. Claim 3 says a context is a natural value for the scope axis: between Pin (scope: none) and Follow (scope: all reachable callers) sits **scope: what I have checked out**.

## No commit gate — the central requirement

The requirement stated most strongly in conversation: live-update several definitions and watch values change without an explicit commit step, so that a mistake in one term neither breaks the others nor forces a rollback.

Two systems already do this, from opposite directions:

- **Jujutsu** makes the working copy a commit that is continuously re-snapshotted: "Jujutsu will automatically create commits from the working-copy contents when they have changed." There is no staging area and commit is not an event. Recovery comes from the **operation log**, a DAG over repo operations, so editing cannot lose anything.
- **Hazel** makes incomplete programs meaningful. A broken subterm becomes a hole and everything else keeps evaluating (`../related-work/02-structure-editors.md`; p8/p9 carry this into arbor).

arbor's version follows directly from what it already has. The store is immutable and ingest is permissive (`feedback: visible breakage, not gates`), so every definition is in the store as soon as it is typed. Nothing can be destroyed, so nothing needs a gate. The only uncommitted thing is the **name binding**:

> **A context holds hashes (always ingested) plus provisional name bindings not yet published to the namespace.** Live evaluation resolves through the provisional bindings. Commit publishes some of them. A mistake in one term stays local because the old hash is still bound in `N` and the store has lost nothing.

This matches the ingest position: ingest everything; only names may be uncommitted. It is also the strongest argument that permissive ingest pays for itself, since it is the mechanism that makes gate-free live editing sound.

## Two sets, not one: the frontier and the commit set

A migration need not commit at all. It can add to the context until you are done. What you see and what you are committing should differ slightly, so you can watch the frontier and its impact and commit as you go. That gives two sets:

- **The frontier (view)** is what is visible and live: your edits plus their blast radius. It is mostly derived (dependents of what changed), it grows as a migration discovers more, and live feedback is computed over it.
- **The commit set** is what you intend to land. It is mostly curated, a subset of the frontier, and committed incrementally.

All three precedents keep two sets:

- **IntelliJ**: changelists are curated commit sets; problems and inspections are the derived view. One changelist is active and is the default target for new modifications, which answers "which set does an edit join by default."
- **jj**: `@`, the auto-snapshotted working copy, versus **revsets**, queries selecting sets of revisions. Membership can be by query as well as by enumeration.
- **Unison**: the live region above the fold versus the whole buffer. On a failed `update`, the broken dependents are removed from the namespace and written into the buffer, which becomes their only home (`../systems/unison/11-scratch-file.md`). The frontier is delivered as the working set. This is claim 3 already shipped, and Unison does not document it.

This also settles arbor-stlc's **residual**: it is the next frontier, not an error report. Pijul and jj make the same move when they store conflicts as first-class values instead of markers in a file.

## Propagation is not commit

Unison has a concrete failure here whose cause is not the obvious one. Full evidence is in `../systems/unison/11-scratch-file.md` §"Known problem: the overlay reaches names, not stored references".

Unison's buffer shadows **names**. A definition already in the codebase resolved its dependencies once and holds them as hashes, so the overlay has no name in it to reach. Commit `double = n -> scale n`, then put a new `scale` and `> double 3` in the buffer, and the watch prints the old answer, 6. Only `update` changes it, by rebuilding `double` into a different definition.

**What content addressing forces.** References are frozen at ingest, so propagating an edit means constructing new objects. The old `double` means 6 forever; that is immutability working as intended. The question is what constructing the new one costs.

**The cost is the commit gate.** In Unison a definition's identity comes from typechecking, and getting it into the codebase is `update`, which is also the commit. Propagating and committing are one gesture, so a propagated value cannot be previewed: when the watch runs, the only `double` that exists is the old one. In arbor ingest is unconditional and destroys nothing, and only the name binding is provisional (§"No commit gate"). So `double'` can be built and evaluated with nothing committed. The frontier holds it, live feedback resolves through it, and the user sees 15.

This is a third argument for permissive ingest, and the strongest. The other two are about not losing work and not needing a rollback. This one is about seeing the consequence of an edit at all: Unison's gate delays the answer as well as the commit.

**What has to be computed.** Not a global re-propagation on every keystroke. Two things suffice: a **reachability test** (does the dependency closure of the displayed expression touch anything the context has provisionally rebound?) and a rebuild of only the displayed things for which it does. Reachability is a query against the reverse index, and the rebuild is bounded by what is on screen, not by codebase size. p11 has both primitives (`Store.callers_of`, `follow-clean:v1`). The shape is known; the constant is unmeasured.

**One predicate, two uses.** The same reachability fact is what a system would report if it chose not to rebuild: a value marked as computed against a superseded definition. One computation serves both policies, so it belongs in the substrate. The substrate decides whether an edit reaches a value; the editing layer decides whether to rebuild it, mark it, or hide it. This is the posture of `feedback: visible breakage, not gates`.

**Why the placement matters.** Unison has the predicate. [PR #5964](https://github.com/unisonweb/unison/pull/5964) added `checkStale`, which walks the dependents graph and prints the path from the thing you ran to the uncommitted edit beneath it. It is wired to `run` only, so the same buffer gets a hard refusal from `run` and a silent 6 from a watch. This is the second instance in the codebase. `doHashCycle` reports a hash-ordering ambiguity that the top-level caller turns into a crash and the internal `let rec` caller discards, so structurally identical code is refused in one position and silently given an order-dependent identity in another (`../systems/unison/02-store-and-hashing.md` §"One caller does not abort"). Both times a computable substrate-level predicate was left to per-call-site policy, and the call sites diverged. The authors understood the system fully; the detector exists and reaches one caller. A condition the substrate can compute should be computed by the substrate. That rule is cheap to follow from the start and expensive to retrofit.

## Claim 2 is a binding question, not a caching question

The original note framed claim 2 as "the system optimizes for what is checked out," was skeptical of it, and cited Denning. The conversation supplied the actual motivation:

> If I have a dataset-analysis algorithm and it runs over 5 PB, that takes a while. But if the dataset in my editing context is 5 rows, that is what I run on every keystroke.

Running on a 5-row sample does not approximate running on 5 PB. It is a different computation with different inputs. Consequences:

- **It cannot threaten evaluation stability.** A caching optimization scoped to a context would have to be sound independently of the context, or `thm:stability` breaks. Input binding has no such obligation: the result is keyed on the hash of the term applied to the context's inputs, which is a different hash. The eval cache need not know contexts exist. The eventual paper should state this, since the opposite is the natural worry.
- **The right citation is Lerner's loop seeds, not Denning.** "Focused Live Programming with Loop Seeds" (UIST 2020) names the **loop-datavoid problem** (live programming needs runtime values, and for incomplete code no test input yields useful ones) and answers it with a small user-supplied **seed**. That is this claim, published. Projection Boxes (CHI 2020) is the display half.
- **arbor already has a rung of this.** p15–p17's `test` aspect is an interface-level convention evaluated live. A context-supplied sample input generalizes it: the context binds names to small values, and live feedback evaluates against those bindings.

A caching version of claim 2 is a separate, later and weaker claim. It should be argued from a measured slow query, not assumed.

## The simplest mechanism

Following `CLAUDE.md`'s posture, the minimum that answers all three claims is:

> **A context is a named set of hashes plus a set of provisional name bindings, with a migration strategy attached.**

The rest derives from it:

- the resolution overlay (provisional bindings shadow `N`, as Unison's `Names.shadowing` does);
- co-dependent edits, from the overlay;
- migration scope, since the set is a value of `04-naming-layer.md`'s scope axis;
- residual delivery, since a failed migration extends the frontier;
- the evaluation environment, since provisional bindings include sample inputs;
- if the name collision resolves that way, p12's opacity open set as one more field.

The first real design question is where the object lives: in the Store (a content-addressed sort), in the Attachment layer (an aspect, or a map beside `N`), or in the Interface layer (`06-architecture.md`'s default). Interface-only is defensible for a first prototype but forecloses persistence, sharing and agent handoff, which CitC and jj workspaces exist to provide.

## Axes, with positions where taken

1. **Stored or ephemeral.** Leaning stored, after the jj read. jj keeps the context map inside the same content-addressed `View` as the namespace, and states the layering rule that makes this work: "the repository doesn't know where on disk any working copies live." The desired state is shared and versioned; only materialization is local. CitC (server-side named workspaces) gives the same answer at scale.
2. **Membership: explicit, inferred or queried.** All three are worth exploring. The jj read supplies a fourth answer that subsumes them. `docs/design/sparse-v2.md` in jj states as its first goal "Sparse Patterns should be versioned with the working copy": patterns live in the op store, referenced by a `WorkingCopyPatternsId` from the active `View`, as an ordered include/exclude rule program instead of a set. Membership becomes a stored, versioned rule program attached to the namespace object. Caveat: this is proposed, not confirmed shipped, and its motivation is materialization, which arbor does not have. Revsets apply the same idea on the commit axis.
3. **One or many, named or anonymous.** Many: agents need concurrent unrelated contexts, and naming follows. IntelliJ changelists and jj workspaces answer this differently. Changelists share one working copy and disambiguate by an active list. jj workspaces get separate working copies over one repo, with **staleness** as the interference primitive.
4. **What is a member: a hash, a name or a binding?** Current lean: hashes, with a migration strategy attached. The frontier/commit-set split may require the frontier to carry bindings, since what breaks is a name's meaning. jj's change ID is arbor's mint thread, shipped: an identifier that "tracks a commit as it evolves through rewrites," with a named *divergent* state when one change has several visible versions. If contexts must track a definition across edits, this is the prior art, and it reopens the mint/thread axis that p12–p17 dropped.
5. **Uncommitted state.** Settled: ingest everything. Names can be uncommitted; definitions cannot. See §"No commit gate".
6. **Relationship to branching.** Branching is expected to live at the namespace level. A context is what a person is actively working on and the feedback they get in the editor. The relation is real and under-explored; jj (workspaces, bookmarks, the op log) and the VCS literature are where to look.
7. **Atomicity of commit.** Not atomic. Commit is incremental, per the frontier/commit-set split, which matches the commit-and-report posture.
8. **Overlap between contexts.** Open, with three shipped answers. IntelliJ detects overlap, marks it persistently and offers named resolutions (shelve, move to active, switch active, ignore). jj lets contexts diverge, names the state (*stale*) and provides a repair command, which is cheap because the desired state is in the shared `View` and only materialization is local. Grove is conflict-free by construction. jj also shows a fourth option one level down: make the name multi-valued so overlap is representable. arbor declines it; see §"Multiplicity belongs to namespaces, not to bindings".

## Toward a formalism (later)

arbor-core's configuration is `⟨Σ, N, E, H⟩`. The natural extension adds `K`, a finite map from context name to a member set, a set of provisional bindings and a strategy.

**Where `K` goes (jj read, 2026-08-19).** jj's `View` object holds `bookmarks` (name → target) and `wc_commit_ids` (workspace name → checked-out commit) as sibling fields of one content-addressed value, versioned by the operation log. In arbor's terms that is `⟨Σ, ⟨N, K⟩, H⟩`, not `⟨Σ, N, E, H, K⟩` with `K` added on the side (`../systems/jj/04-arbor-mapping.md`). Three consequences follow. Axis 1 resolves toward stored almost for free: if `N` is versioned at all, a context map costs nothing structurally. Contexts and namespace branching share a home (axis 6). The inertness statement below becomes the same sort of statement as `thm:nsb`, over the same component. One caveat: jj maps a workspace to one commit because a commit is a whole tree. arbor's members are individual definitions, so the location transfers but the payload does not.

Statements worth wanting:

- **Conservativity of overlay resolution.** A name that resolves outside every context resolves the same way inside one, so `thm:nsb` survives contextualization.
- **Inertness.** Opening, closing or renaming a context changes no term's meaning; contexts are as inert as names. If this fails, the object is wrong.
- **Scoped migration.** `04-naming-layer.md`'s Follow, restricted to a context. arbor-stlc's `thm:migosc` (residual exactness) should become a restatement with the scope instantiated to a context, not new content. That would be strong evidence the object is right.
- **Non-interference of disjoint contexts.** Two contexts touching disjoint parts of the store cannot affect each other. This is arbor's version of patch independence. Sources to borrow from: Mimram & Di Giusto's pushout characterization (MFPS 2013) and Angiuli et al.'s homotopical patch theory (ICFP 2014).

## Multiplicity belongs to namespaces, not to bindings

The jj read raised a question independent of editing contexts. jj's namespace is multi-valued: a bookmark's target is a `RefConflict` of `removes`/`adds` terms, so a name moved to B in one operation and to C in a concurrent one is recorded as "moved from A to B or C", shown as `??`, and only commands that need one answer fail. Unison has the same shape: `Names` is a `Relation Name Referent`, conflict is representable, and unconflictedness is a derived view that operations demand when they need it (`../systems/unison/03-namespace-and-history.md` §"Names map to sets").

**Position taken (2026-08-20): `N` stays single-valued.** A name denotes one hash. Where multiplicity is needed, it belongs to a **family of namespaces** (one per branch, and/or one per agent or user, each a single-valued view), not to a family of targets inside one namespace. If conflict resolution needs a construct holding several candidates, that construct is separate and deferred. It is not `N`.

Three pieces of evidence support this factoring, all from systems already read:

- **Unison built multi-valued names and had to add the gate back.** `handleUpdate2` asserts unconflictedness up front and bails with `Output.ConflictedDefn` (`.../Update2.hs:99-102`). A representable state that half the operations refuse is still a gate.
- **Unison's next-generation format drops it.** `BranchV3`: "A V3 branch is a trimmed-down V2 branch: **Names can't be conflicted.** Metadata doesn't exist. Patches don't exist" (`codebase2/codebase/U/Codebase/BranchV3.hs:16-30`). All three removals belong to the machinery Unison spent three years deleting (`../systems/unison/05-deletions.md`).
- **jj already has the family, and conflict appears only where the family is absent.** `View.remote_views` is a list of named namespaces, each with its own bookmarks and tags (`lib/src/protos/simple_op_store.proto:119-123`). The conflicted `RefTarget` is needed for the local map because that map is singular and absorbs concurrent operations. With the family primary, most of the pressure for conflicted bindings goes away.

**What this changes for `K`.** It refines the jj mapping. jj's `⟨N, K⟩`-in-one-object becomes, for arbor, a family: a context (or branch, or agent) names both a member set and the single-valued namespace that resolution goes through. The overlay of §"No commit gate", where provisional bindings shadow `N`, is then what it means to be a distinct namespace in the family, and `Names.shadowing` is the composition operator.

**What stays open:** whether contexts, branches and per-agent views are one mechanism or three, and what the conflict-resolution construct is, if one is needed. Both are deferred.

## Open questions

1. Does p12's opacity open set become a field of this object, or a separate object in 1-1 correspondence? Partly open by choice. The deliverable is UX: making the current context's contents and its opened representations obvious at a glance.
2. Stored or ephemeral? If stored, in the Store (a sort) or in the Attachment layer?
3. Is membership an extensional set, a degree-of-interest function, or a query language over the store? jj's revsets make the third live.
4. Are frontier members hashes or bindings? Does tracking a definition across edits bring back the mint/thread axis, and is jj's change ID with divergence the model?
5. Where is the boundary between the frontier and the commit set, and what gesture moves something across it?
6. How do contexts, namespace branching and per-agent views relate: is it one mechanism or three, and does one subsume another? The single-valued-`N` position pushes multiplicity into a family of namespaces but does not say what indexes the family. Needs the VCS dig. Deferred.
7. What does a user see when two contexts overlap? IntelliJ's four resolutions are the starting point.
8. Does an agent get a context, and is "hand this task to an agent" the same gesture as "open a context"? (`../related-work/08-agentic-vc.md`)
9. Where do a context's sample inputs come from? Are they ordinary provisional bindings or a distinct sort of binding?
10. Is the caching form of claim 2 ever worth building, and what measured workload would justify it?
11. If conflict resolution needs an object holding several candidates, what is it and where does it live? Not `N`. Deferred.
12. **What does live propagation cost?** §"Propagation is not commit" argues the work is a reachability query plus a rebuild bounded by what is on screen, so only the constant is unknown. The experiment: edit a definition with a large dependent cone, with a display showing a handful of values scattered through it, and time a keystroke. p11 has `Store.callers_of` and the `follow-clean:v1` cached dry-run aspect and has measured neither.
