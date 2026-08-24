# jj mapped onto arbor's formal objects

**Status:** Source read @ `253d3be`. See `00-overview.md` for the pin. This is the payload of the read: what jj's objects are, in arbor's vocabulary, and what changes as a result.

## The mapping

arbor-core's configuration is `C = ⟨Σ, N, E, H⟩` (`formalism/paper/arbor-core.tex`, `def:config`). `../../design/13-editing-context.md` proposed a fifth component `K` for editing contexts. jj has an object for four of the five, and the fifth is the finding.

| arbor | jj | notes |
| --- | --- | --- |
| `Σ` — content-addressed store of definitions | the **commit backend** (`Backend` trait; `GitBackend`, `SimpleBackend`) | jj stores commits/trees/files; arbor stores definitions. Both content-addressed, both immutable, both pluggable in principle — jj's is pluggable in fact, driven by "move storage to the cloud." |
| `N` — namespace, `Name ⇀ Hash` | `View.bookmarks : name → RefTarget` | **jj's is multi-valued.** A `RefTarget` is a `RefConflict` of `removes`/`adds` terms, so a name may denote a conflict and the repo keeps working. |
| `E` — evaluation cache | *(nothing)* | jj has no evaluation. The nearest analogue is the commit index (`default_index/`), a rebuildable derived structure — which is what arbor's aspects are. |
| `H` — binding history, append-only, per name | the **operation log**: a DAG of `Operation`s, each pointing at a `View` | jj's is a DAG over the *whole* configuration, content-addressed, merged by 3-way merge of views. arbor's is per-name, linear, and not hashed. |
| `K` — editing contexts | `View.wc_commit_ids : map<workspace_name, commit_id>` | **A field of the same object as `N`.** Not a fifth component. |

## The finding: `K` is not a fifth component

`lib/src/protos/simple_op_store.proto:93-113` puts `bookmarks` and `wc_commit_ids` in one `View` message, and the `View` is content-addressed and versioned by the operation log. In arbor's terms:

> jj's answer is `⟨Σ, ⟨N, K⟩, H⟩` with `⟨N, K⟩` a single hashed value and `H` a DAG of them — not `⟨Σ, N, E, H, K⟩` with `K` bolted on beside.

Three consequences for `13`.

1. **The stored-or-ephemeral axis (`13` axis 1) resolves toward stored, and cheaply.** If `N` is going to be a versioned object at all — which `H` already implies — then adding a context map to it costs nothing structurally. jj is the existence proof that this is a sane place to put it. The Interface-layer answer (`../../design/06-architecture.md`'s default) is what jj explicitly rejects: *"The repository doesn't know where on disk any working copies live"* — the *desired* state lives in the shared versioned object; only the materialization is local.
2. **Contexts and branching (`13` open question 6) share a home rather than competing.** In jj, "which commit is checked out in workspace *w*" and "where does bookmark *b* point" are fields of one object with one merge algorithm. That is a direct argument against arbor treating namespace branching and editing contexts as separate mechanisms — and it was the leaning in conversation that branching lives at the namespace level.
3. **The conservativity/inertness theorems `13` wants become theorems about one object.** If `K` sits inside `N`, then "opening or closing a context changes no term's meaning" is the same *kind* of statement as `thm:nsb` (name edits don't change stored meaning), over the same component, and probably by the same argument: `Σ` and `E` are untouched.

**Caveat, stated because it is load-bearing.** jj's `wc_commit_ids` maps a workspace to **one commit**, not to a *set* of members. jj gets multi-member working sets from the fact that a commit is a whole tree; arbor's members are individual definitions, so arbor's `K` entry has to carry more — a member set (or, per `02`'s sparse-v2, a *pattern* selecting one) plus the provisional bindings `13` proposes. The *location* transfers; the *payload* does not.

## Three forks worth stating plainly

Places where arbor and jj answer the same question differently, with the trade recorded on both sides.

**1. Where the mint lives.** arbor puts the mint mark **inside** the hashed bytes (`../../design/10-minted-identity.md`). jj keeps the change ID **outside** the commit hash, in a `StackedTable` side file. arbor's choice makes the mint part of identity, so it cannot survive a content edit without an explicit gesture (p11's *edit-of-X*); jj's choice makes the change ID survive every rewrite for free, and pays with a side table plus a real defect it documents — two commits differing only in change ID collide, and jj errors out. Unison pays the same side-table tax for its `unique` GUIDs. **Two independent systems chose outside-the-hash and both built a side table.** That is evidence arbor's line should take seriously without treating it as decisive.

**2. Single-valued vs multi-valued `N`.** jj's `RefTarget` is a `RefConflict` of `removes`/`adds` terms, so a name may denote a conflict; merge is total because the codomain admits conflicts, and only queries needing one answer fail. Unison is the same shape (`Names` is a `Relation`).

**arbor's position (2026-08-20): `N` stays single-valued**, and multiplicity belongs to a **family of namespaces** — per branch, and/or per agent — each single-valued. See `../../design/13-editing-context.md` §"Multiplicity belongs to namespaces, not to bindings" for the argument and the three supporting data points, the sharpest of which is that Unison's own `BranchV3` format *removes* conflicted names, and that jj already has a namespace family (`View.remote_views`, `simple_op_store.proto:119-123`) — the conflicted target is needed only for the **local** map, precisely because it is singular. Any construct holding several candidates for conflict resolution is a separate object, deferred.

**3. `H` as a per-name log vs a DAG over the configuration.** arbor's `H : Name ⇀ (Hash∪{⊥} × Time)*` records what each *name* was. jj's operation log records what the *whole view* was, addressed by content, with parents, and merges by 3-way merge. arbor's is cheaper and answers "how did this name get here"; jj's answers "how did the repo get here," supports `--at-op` time travel, and is what makes lock-free concurrency and universal undo possible. Given `../../related-work/08-agentic-vc.md`'s framing — agents as autonomous concurrent users — jj's shape is the one that scales to the target. This is a substantive question for the formalism, not a detail.

## What arbor has that jj does not

Worth saying, so the read is not read as "adopt jj."

- **The store holds *definitions*, not files.** jj's granularity is a tree; a "change" is a whole snapshot. arbor's is a definition, which is why arbor can talk about *callers*, *dependents*, and *type-preserving migration* at all. jj has no analogue of `Σ`'s reference edges, `callers_of`, or the residual.
- **Types, and typing as an observational aspect.** jj is language-agnostic and has no semantics for what it stores. arbor-stlc's local soundness and residual exactness have no jj counterpart, and cannot.
- **`E`, and a theorem about it.** jj has no evaluation and therefore no eval cache and no `thm:stability`. Unison has the cache but no theorem; arbor has both.
- **α-canonicalization.** jj hashes syntax as bytes. `thm:alpha` has no jj analogue.

Conversely, jj's `docs/technical/` is a standing reproach: **rationale is written down**, including for choices that were rejected. `../unison/09-side-structure.md` had to reconstruct Unison's reasoning entirely by inference. jj did not make that necessary.

## Corrections owed

Nothing in `docs/` is *wrong* about jj — jj was not previously cited anywhere in the repo. Two additions rather than corrections:

- `../../related-work/12-editing-contexts.md` records jj as an artifact with the working-copy-as-commit, workspaces, change IDs, and revsets. It should now also point here, and pick up **GitButler** (multiple virtual branches simultaneously, first-class conflicts, operations history — from jj's own `docs/related-work.md`), which is the closest thing to IntelliJ changelists taken to their conclusion.
- `../../design/open-questions.md` §"Mint stability under merge" cites only Unison's side table. jj is a second data point with a *different* resolution (never re-derive; tolerate divergence as a labeled state) and the same side-table cost.

## Open threads

- **`[read]` The revset implementation.** `13`'s membership-as-query idea rests on revsets, and this read took them from `architecture.md`'s five-stage pipeline description rather than from `revset.rs` and `default_index/`. If membership-as-query is going to be proposed for arbor, the evaluation strategy is the part that decides whether it is affordable.
- **`[read]` `merge.rs` / `merged_tree.rs`.** The conflict algebra's *laws* are documented; the implementation is unread. If arbor's residual becomes an algebraic value, this is the prior art to read properly.
- **`[verify]` How much of sparse-v2 has landed.** `docs/design/sparse-v2.md` is a design document; the read did not check the implementation against it. The claim "membership is a versioned object referenced from the View" is currently **proposed**, not verified as shipped.
- **`[arbor]` Does `N` become multi-valued?** The largest question this read raises, and it is independent of editing contexts. It touches `def:config`, `thm:nsb`, `thm:histcoh`, and arbor-stlc's residual.
- **`[arbor]` Does `H` become a DAG over the configuration?** Entangled with namespace branching, which `../../design/04-naming-layer.md` currently defers.
