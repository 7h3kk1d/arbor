# Unison — The branching model

**Status:** Source read @ `db60ce2`, schema 26. `[source-verified]` unless tagged. Extends
`03-namespace-and-history.md` §"Causal" (the history DAG) and `08-merge-and-branching.md` (which
covers merge and treats branching only in passing). Written because understanding conflicts turned
out to require understanding branches first.

## A branch is a pointer to a tree of pointers

`project_branch.causal_hash_id` is a single causal-hash id — exactly a git ref. The `ProjectBranch`
record carries no tree at all (`ProjectBranch.hs:14-22`: `projectId, branchId, name, parentBranchId,
isMerge, isUpdate, isUpgrade`).

What it points at is a `Causal` whose value is a namespace node whose **children are themselves
causals**:

```haskell
data Branch0 m = Branch0
  { _terms :: Star Referent NameSegment,
    _types :: Star TypeReference NameSegment,
    -- | Note the 'Branch' here, not 'Branch0'.
    -- Every level in the tree has a history.
    _children :: Map NameSegment (Branch m), ... }
```
`Branch/Type.hs:99-121`. And the *namespace* hash commits to each child's *causal* hash
(`Convert.hs:415-418`), so:

**Every namespace node has its own independent history, and touching a leaf changes the hash of
every node above it.** Writes cons a new causal at each modified level (`consBranchSnapshot`). This
is why `squash` has to recurse over the whole tree, and why a change deep in `lib` shows up at the
root as one opaque step.

Users meet the per-node history in exactly three places: `history lib.base` walks *that node's*
chain; `reset`/`fork` onto a sub-path clobbers that subtree's history and conses only its ancestors;
and a bare `reset` after `deprecated.cd` resets where the cursor is, not the branch.

## The causal, precisely

A causal hash is SHA3-512 over **the namespace hash followed by the sorted parent causal hashes**
(`Hashing/V2/Causal.hs:12-18`) — and nothing else. No author, no timestamp, no message, no branch
name. Two consequences:

- **History is content-deduplicated.** Redoing the same edit from the same parent yields the same
  causal hash; `saveCausal` is `ON CONFLICT DO NOTHING` and skips writing parents when the row
  already existed.
- **Who/when/why live outside the DAG** — in `project_branch_reflog` (time, reason) and, since
  schema 23-26, `history_comments` (a signed, syncable comment).

Parents are a **set** at every representation. More than two is representable and storable but
nothing in UCM produces one: `Branch.mergeNode` is explicitly binary, `Causal.one` and
`squashCausal` produce zero, `cons` produces one.

## Every command that moves a branch head

Two chokepoints, `Queries.insertProjectBranch` and `Queries.setProjectBranchHead`, and **both
unconditionally write a reflog row**.

The distinction that matters is **cons vs. repoint**. A cons adds a `causal_parent` edge, so the old
state stays reachable through history. A repoint does not, so the old state survives *only* in the
reflog.

| | effect on the head | new parent edge? |
|---|---|---|
| `branch`, `branch.empty` | new row at the source's causal (or the empty causal) | no |
| `switch` | unchanged — only `most_recent_branch` and `last_accessed` | no |
| `fork`, `reset`, `branch.squash`, `sync.from-*` | **repoint** | **no** |
| `update`, `delete`, `move.*`, `alias.*`, `lib.install`, `builtins.merge` | cons | yes |
| `merge` (success) | cons a **two-parent** merge node | yes |
| `merge` (fast-forward) | repoint to the other side | no |
| `pull` | as merge; `pull.without-history` conses a snapshot ignoring the LCA | mixed |
| `push` | nothing local — only `project_branch_remote_mapping` | no |
| `undo` | repoint to the parent; refuses past a merge or the start | no |
| `delete.branch` | row deleted; children reparented to the grandparent | no |

**`reset` is a repoint, and that is the whole hazard.** No `causal_parent` row links the pre-reset
head to the post-reset head, so `history` cannot find it. Only `reflog` can — and the reflog display
shows only the *destination* of each move, so recovering "just before the merge" means picking the
row *below* the merge row. `from_root_causal_id` is written on every head move and never displayed
for project branches.

## Temporary branches: a failed operation becomes a branch

Three tables, one pattern. `sql/017`'s comment is the clearest statement: *"We put you on a new
'update branch' whenever an `update` fails (from a non-update branch)."*

| table | created by |
|---|---|
| `merge_branch` | a merge or pull whose result doesn't typecheck |
| `update_branch` | an `update` that doesn't typecheck, **or a `delete` that would break dependents** |
| `upgrade_branch` | a failed `lib.upgrade` |

Being on one changes six things: the prompt gains a `🧩`; the scratch file is diffed against the
*parent* branch rather than the current one (`Load.hs:107-118` — because dependents have been pulled
out of the namespace, they would otherwise all look new); the slurp message says "and complete the
merge"; `update` *finishes* the operation instead of starting one (merging into the parent and
deleting the temp branch); `upgrade` is refused; and unique-type GUIDs are recovered from the parent
so re-saving a type doesn't mint a fresh one.

Nothing tracks a stranded temp branch. Abandon it and it sits in `branches` forever with no marker —
the `🧩` disappears the moment you switch away.

## LCA

A recursive CTE that materializes the **complete** ancestor closure of both sides, intersects them,
and keeps the maximal elements (`Queries.hs:2307-2345`). There is **no early termination** — it walks
both histories to the root even when the LCA is one step back. The in-memory version
(`Causal/Type.hs:96-127`) is a bidirectional BFS that stops when the frontiers touch; the SQL one is
not.

With criss-cross merges there can be several maximal common ancestors and `LIMIT 1` picks
arbitrarily. `[inferred]` — latent nondeterminism, not a crash.

**No common ancestor** is not an error: the LCA is treated as the *empty namespace*, so every
definition on both sides looks added and the merge becomes a two-way add/add. That is why merging
unrelated histories produces conflicts on every shared name rather than a refusal.

## Squash

`squashCausal` recursively rebuilds the tree with `parents = mempty` at every level, preserving
terms, types, patches and metadata verbatim. It must recurse because a child's causal hash is part of
its parent's namespace hash. Results are cached in `squash_results`, keyed by the **namespace** hash
(correct — squashing depends only on contents).

Called by exactly two things: `branch.squash`, and `lib.install.local`, so an installed local
dependency doesn't drag its history into your `lib`. With an existing destination it *repoints* to a
parentless causal, orphaning whatever was there.

## Push and pull are asymmetric

**Push refuses on divergence.** `wouldNotBeFastForward` is `not <$> Queries.before remote local`, and
it returns "refuse" if either hash is unknown locally. The actual write is a compare-and-set against
the expected remote head.

**Pull merges on divergence** — it computes the LCA and calls the same `doMerge`, so `pull` can leave
you on a `merge-remote-…-into-…` temporary branch with conflicts in your scratch file. It never
refuses and never force-resets.

Remote mappings walk *up* `project_branch_parent` looking for the nearest ancestor with a mapping
(`Queries.hs:3529-3596`), returning a depth; at depth > 0 only the remote *project* is inherited, not
the branch. That is why a fresh topic branch knows where to push but derives its own remote name.

## What `history` does not show

- **Anything past a merge.** `MergeTail` lists the parent hashes and stops. `undo` likewise refuses
  with `CantUndoPastMerge`. A merge is a wall in both directions.
- **Anything before a `reset`, `fork`, or `squash`** — those repoint, so the prior head is not a
  parent and does not exist in the chain.
- **Who, when, or why** — none of it is in the causal.
- **Content diffs.** Each step shows the *name-level* diff only: which names were added and removed.

## For arbor

The load-bearing observation is that **a branch head is a pointer and a namespace is a hashed value**,
which is what makes fast-forward a hash comparison, LCA a graph search, and "is this the same
namespace" free. `03-namespace-and-history.md` already records that arbor's `N`-beside-`Σ` gets none
of these; this file adds the sharper version: *every level of the tree is independently versioned*,
and that single decision is what forces squash to recurse, makes ancestor hashes change on any
descendant edit, and gives sub-namespaces their own history whether or not anyone wanted it.

The second observation is about **cons vs. repoint**. Unison has two classes of head movement with
very different recoverability, and the distinction is invisible in the UI — `reset` and `update` look
equally routine, but only one leaves a trail in history. If arbor's `H` grows branching, this is the
axis to be deliberate about, because the reflog is doing load-bearing recovery work that the DAG
cannot.
