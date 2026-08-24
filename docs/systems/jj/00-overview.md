# Jujutsu (`jj`), read in source — Overview

**Status:** Source read. Conducted 2026-08-19 against `github.com/jj-vcs/jj` at commit **`253d3be2621b4a48c1fdce088dacf8eeb2e79683`** (`253d3be`, 2026-08-18, HEAD of `main`). Evidence conventions in `../00-index.md` §"Evidence conventions". Findings are inputs, not policy.

Re-check the pin with `git -C ~/Projects/jj rev-parse HEAD`. If it has moved, every citation below is still true *at `253d3be`*; it is dated, not wrong.

## Why read jj

`../../design/13-editing-context.md` opened the question of an **editing context** — a named collection of definitions in flight, with no commit gate, whose members update live and which scopes migration. jj was named in that conversation as a system to learn from, and it turns out to be the closest existing implementation of *four* things arbor is simultaneously reaching for:

1. a **working set that is itself a stored object**, not a scratch area outside the store;
2. **many concurrent contexts** over one repository, with a defined interference story;
3. an **identifier that survives content edits** (arbor's mint thread), shipped, with divergence as a tolerated state rather than an error;
4. a **versioned history of the configuration itself**, content-addressed — arbor's `H`, generalized.

It is also the system whose *shape* is closest to arbor-core's `⟨Σ, N, E, H⟩`. That mapping is the payload of this read: `04-arbor-mapping.md`.

Unlike Unison, jj has **substantial written design documentation** (`docs/technical/`, `docs/design/`) with rationale, so less of this read is inference. Like Unison, it has **no peer-reviewed publication**; the source and its docs are the citable artifact. `../../related-work/12-editing-contexts.md` records it as an artifact.

## Headline findings

Eight, in descending order of how much they should change what arbor writes down.

1. **jj puts the namespace and the working sets in the *same* content-addressed object.** A `View` holds `bookmarks` (name → target) *and* `wc_commit_ids : map<workspace_name, commit_id>` side by side (`lib/src/protos/simple_op_store.proto:93-113`). arbor's proposed fifth component `K` is, in jj, a **field of `N`** — and the pair is versioned together. This is the single most consequential finding for `13`. `01-operation-log-and-views.md`.

2. **A name may be *conflicted*, and the repo keeps working.** `RefTarget` is a `RefConflict` with `removes`/`adds` terms (`simple_op_store.proto:26-41`); a bookmark moved to B in one operation and C in a concurrent one is recorded as "moved from A to B or C," rendered `??` in the log, and only the commands that actually need a single answer fail. This is arbor's *commit-and-report, never abort* posture implemented **at the namespace level**, and it is the strongest existing precedent for a multi-valued `N`. `01`.

3. **The operation log is arbor's `H`, generalized from names to the whole configuration and made content-addressed.** Operations relate to views exactly as commits relate to trees; both are stored in content-addressed storage "just like Git commits are... That makes them safe to write without locking" (`docs/technical/concurrency.md`). arbor's `H` is per-name, append-only, and linear; jj's is a DAG over `⟨N, K⟩`, merged by 3-way merge of views. `01`.

4. **Working-set membership is becoming a versioned, content-addressed object referenced from the `View` — and it is a *pattern language*, not a set.** `docs/design/sparse-v2.md` states as goal 1: *"Sparse Patterns should be versioned with the working copy,"* stored in the Op Store and referenced by a `WorkingCopyPatternsId` from the active `View`, with ordered include/exclude rules and path remapping. This is a direct answer to `13`'s open question 3 (extensional set vs. degree-of-interest vs. query). `02-workspaces-and-the-working-copy.md`.

5. **The change ID is arbor's mint thread, shipped — and jj pays the same tax Unison does.** A change ID "tracks a commit as it evolves through rewrites"; the commit ID identifies one version. Because Git's object model has nowhere to put it, the `GitBackend` keeps change IDs and predecessor lists in a **side table** at `.jj/repo/store/extra/` (`docs/technical/architecture.md` §GitBackend). Unison needed `namespace_unique_type_guid` for the same reason (`../unison/04-caching-update-merge.md` §"Minting"). Two independent systems, same side table. **A divergent change** — one change ID with several visible commits — is a *named, labeled state*, not an error. `03-change-ids-and-conflicts.md`.

6. **Concurrency is optimistic and structurally cannot fail.** *"Each operation sees a consistent view of the repo and is guaranteed to be able to commit their changes"*; divergence is detected by the **next** command, because it has to be able to anyway (the concurrent operation may have arrived over a distributed filesystem). Combined with the core tenet *"Make it incredibly hard to lose work in your repo,"* this is the concurrency story `../../related-work/08-agentic-vc.md` wants and STORM argues against. `01`.

7. **Conflicts are lazy algebraic values with a simplification law.** A conflicted commit points at an odd-length list of trees read as `A+(C-B)+(E-D)`; contents are *computed on demand*; `Merge::flatten()` and `Merge::simplify()` cancel terms, so rebasing a conflict onto a new base yields `D+(B-A)` rather than a nest (`docs/technical/conflicts.md`). If arbor's migration residual becomes a first-class value, this is the shape — and it has laws. `03`.

8. **Five pluggable backends, each named in a `type` file.** Commit, operation, op-heads, index, and working copy are separate storage-independent interfaces (`docs/technical/architecture.md`), driven by the requirement to move storage to the cloud. That is `../../design/06-architecture.md`'s layering with a declared swap point, and it is why "the working copy is stored by the working copy backend" is even sayable.

## What was read, skimmed, and not read

~265,000 lines of Rust plus a substantial docs tree. Stating the boundary matters.

**Read closely** — `docs/technical/architecture.md` and `concurrency.md` in full; `docs/technical/conflicts.md`; `docs/operation-log.md`; `docs/working-copy.md`; `docs/glossary.md`; `docs/core_tenets.md`; `docs/related-work.md`; `docs/design/sparse-v2.md`; `lib/src/protos/simple_op_store.proto` in full (the `View`/`Operation`/`RefTarget` schema); `lib/src/content_hash.rs`'s hashing discipline; the workspace-naming surface of `lib/src/workspace.rs`.

**Skimmed** — `lib/src/evolution.rs` (module header and types only), the `lib/src/` module inventory, the revset evaluation pipeline as described in `architecture.md` §Revsets rather than in `revset.rs`.

**`[unread]`** — the revset implementation and its index integration (`revset.rs`, `default_index/`); `merge.rs` and `merged_tree.rs` (the conflict algebra's implementation, as opposed to its documented laws); `local_working_copy.rs` and the snapshot/fsmonitor path; the Git interop layer (`git.rs`, `git_backend.rs`) beyond the change-ID side table; the CLI crate entirely; `absorb.rs`, `annotate.rs`, `bisect.rs`, `fix.rs`, and the other command-support modules.

**Not attempted** — any claim about performance, and any claim about how jj behaves in practice at Google scale. The core tenets assert the scaling requirement; nothing here verifies it.

## Files

- `01-operation-log-and-views.md` — the `View`, the operation DAG, lock-free concurrency, conflicted names.
- `02-workspaces-and-the-working-copy.md` — the working copy as a commit, workspaces, staleness, sparse patterns as versioned membership.
- `03-change-ids-and-conflicts.md` — change ID vs commit ID, divergence, evolution, first-class conflicts and their algebra.
- `04-arbor-mapping.md` — **the payload**: jj's objects against arbor's `⟨Σ, N, E, H⟩`, and what it says about `K`.
