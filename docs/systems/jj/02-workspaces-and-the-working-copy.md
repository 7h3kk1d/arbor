# Workspaces, the working copy, and sparse membership

**Status:** Source read @ `253d3be`. See `00-overview.md` for the pin.

## The working copy is a commit

`docs/working-copy.md`: *"Jujutsu will automatically create commits from the working-copy contents when they have changed."* Most commands snapshot first; the snapshot is recorded as an operation, new commits are created in memory and recorded as a new operation, then the working copy is updated to match.

Files are **implicitly tracked by default** — new files are committed on the next command, removed files are untracked — governed by `snapshot.auto-track`.

The core tenets state the position (`docs/core_tenets.md`) `[design doc — the project's own statement of intent]`:

> * User-friendliness: Making the working copy a commit is simpler. This is how the project started.
> * **The repo is the source of truth**: Most commands should operate on the commit graph. The working copy is just one way of editing commits.
> * Make it incredibly hard to lose work in your repo.
> * Allow concurrent edits on any commit, pending or finished.

The third and fourth are the design conversation's "no explicit commit step" requirement, stated as project tenets. Note *how* jj achieves it: not by weakening commit, but by making commit **automatic and continuous**, so it stops being a gesture the user can get wrong. There is no staging area because the working copy is already a first-class object.

## Workspaces

`docs/working-copy.md`: *"You can have multiple working copies backed by a single repo. Use `jj workspace add` to create a new working copy."* Workspaces are **named** (`WorkspaceName`, with a `WorkspaceName::DEFAULT`; `lib/src/workspace.rs:36-37, 364`), and the name is the key in `View.wc_commit_ids`.

`docs/technical/architecture.md` states the split precisely:

> The repo view at the current operation determines the **desired** working-copy commit in each workspace. The `WorkingCopy` determines what is **actually** in the working copy.

So a workspace has two states that can disagree, and the disagreement has a name: **stale**. A workspace becomes stale when its working-copy commit was changed from another workspace (or the updating process crashed), and `jj workspace update-stale` repairs it.

**Relevance.** This is the interference story for `13`'s axis 8 (overlapping contexts), and it is a third answer next to IntelliJ's (detect, mark, offer four resolutions) and Grove's (conflict-free by construction): **let them diverge, name the divergent state, and provide a repair command.** It is cheap precisely because the *desired* state lives in the shared, versioned `View` and only the *materialization* is local.

Also note the layering consequence: *"The repository doesn't know where on disk any working copies live"* (`architecture.md` §ReadonlyRepo). The context is an object in the store; where it is realized is not the store's business. That is `../../design/06-architecture.md`'s upward-only dependency rule holding under exactly the pressure `13` puts on it.

## Sparse patterns — membership as a versioned pattern language

Every working copy is sparse. `docs/technical/architecture.md` §TreeState: *"The `TreeState` type supports sparse checkouts. In fact, all working copies are sparse; they simply track the full repo in most cases."*

The current format is an unordered list of prefix strings, stored in **working-copy state files that are not versioned in the Op Store** — and `docs/design/sparse-v2.md` `[design doc — proposed, not confirmed shipped at this pin]` exists to change that. Its three goals, verbatim:

> 1. Sparse Patterns should be versioned with the working copy
> 2. Sparse Patterns should support more flexible matching rules
> 3. Sparse Patterns should support client path remapping

and the proposed representation:

> Sparse Patterns v2 will be stored as objects in the Op Store, referenced by a `WorkingCopyPatternsId` from the active `View`.

with an *ordered* structure of include/exclude rules over three path types (`Dir`, `Files`, `Exact`) plus a mapping list, and `read_working_copy_patterns` / `write_working_copy_patterns` added to the `OpStore` trait.

**This is the answer to `13`'s open question 3**, and it does not pick a side of curated-vs-inferred. Membership is neither an extensional set nor a degree-of-interest function; it is an **ordered rule program**, stored as a content-addressed object, referenced from the same `View` that holds the namespace, and therefore versioned and merged along with everything else. jj's revsets — *"a functional language for selecting a set of revisions"* (`docs/glossary.md`) — are the same instinct on the commit axis.

Two caveats before arbor copies it. First, sparse-v2 is a **design document**; the read did not confirm which parts have landed. Second, the goal driving it is *materialization* — deciding which files to write to disk — which arbor does not have, since arbor has no working directory. What survives the translation is the *shape*: membership as a stored, versioned rule program hanging off the namespace object.

## Diff editing, as a small proof of the idea

`architecture.md` §Diff-editing: diff editing *"works by creating two very sparse working copies, containing only the files we want the user to edit,"* letting the user edit one side, and snapshotting the result.

Worth recording because it is a working set created **programmatically, for a single interaction, and thrown away** — evidence that once contexts are cheap objects rather than a user-facing mode, the system starts using them internally. arbor's editing-layer gestures (p12's opacity opening, p17's `open as module`) are candidates for exactly this treatment.
