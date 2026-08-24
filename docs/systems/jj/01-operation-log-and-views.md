# The operation log and the View

**Status:** Source read @ `253d3be`. See `00-overview.md` for the pin.

## The two-object model

jj's most reusable structural idea, stated in one sentence by its own docs (`docs/technical/concurrency.md`):

> The operation log is similar to a commit DAG […] but each commit object is instead an "operation" and each tree object is instead a "view".

So there are two parallel content-addressed DAGs:

| commit layer | operation layer |
| --- | --- |
| commit → tree | operation → view |
| commit → parent commits | operation → parent operations |
| tree = the state of the files | view = the state of *the repo* |

The consequence worth taking: **the whole mutable configuration is reified as a hashed value, and its history is a DAG of hashed values.** Not a log of edits — a log of *states*, each addressed by content, with operations as the edges carrying metadata.

## What is actually in a View

`lib/src/protos/simple_op_store.proto:93-113` `[source-verified]`:

```proto
message View {
  repeated bytes head_ids = 1;
  bytes wc_commit_id = 2 [deprecated = true];
  map<string, bytes> wc_commit_ids = 8;
  repeated Bookmark bookmarks = 5;
  repeated Tag local_tags = 6;
  repeated RemoteView remote_views = 11;
  repeated GitRef git_refs = 3;
  ...
}
```

Read that against arbor's configuration:

- `bookmarks` — **name → target**. This is `N`.
- `wc_commit_ids : map<string, bytes>` — **workspace name → the commit currently checked out there.** This is `K`, and it is a *sibling field of the namespace inside one object*, not a separate component.
- `head_ids` — the visible heads, i.e. what is reachable/displayed. There is no separate "root set"; visibility is a field.

`docs/operation-log.md` says the same thing in prose: the view *"contains information about where each bookmark, tag, and Git ref […] pointed, as well as the set of heads in the repo, and the current working-copy commit in each workspace."*

The `Operation` message (`:120-127`) is the edge:

```proto
message Operation {
  bytes view_id = 1;
  repeated bytes parents = 2;
  OperationMetadata metadata = 3;
  repeated CommitPredecessors commit_predecessors = 4;
  bool stores_commit_predecessors = 5;
}
```

and its metadata (`:135-145`) carries `start_time`, `end_time`, `description`, `hostname`, `username`, a free-form `attributes` map, an `is_snapshot` flag, and an **optional `workspace_name`** — so an operation knows which context produced it, and whether it was an automatic working-copy snapshot rather than a user gesture.

## Conflicted names

The finding arbor should sit with longest. A bookmark's target is not a hash; it is a **`RefTarget`**, and a `RefTarget` is a **`RefConflict`** — a pair of `removes` and `adds` term lists (`simple_op_store.proto:26-41`) `[source-verified]`:

```proto
message RefConflict {
  message Term { optional bytes value = 1; }
  repeated Term removes = 1;
  repeated Term adds = 2;
}
```

`docs/technical/concurrency.md` explains the semantics:

> if bookmark `main` was moved from commit A to commit B in one operation and moved to commit C in a concurrent operation, then `main` will be recorded as "moved from A to B or C".
>
> Because we allow bookmarks (etc.) to be in a conflicted state rather than just erroring out when there are multiple heads, the user can continue to use the repo, including performing further operations on the repo. Of course, some commands will fail when using a conflicted bookmark.

The user surface is a `??` suffix in `jj log` and a note in `jj status`.

**Why this matters to arbor.** arbor's recorded posture is *commit-and-report, never abort*: substrate operations commit and expose the damage through residual/broken queries rather than refusing. arbor-core's `N : Name ⇀ Hash` is single-valued, so an edit that would produce two answers has nowhere to go — the posture is stated but the datatype cannot express it. jj's `N` is **multi-valued by construction**, and the failure is deferred to whichever *query* actually needs one answer. That is a datatype-level implementation of the posture arbor currently only asserts, and it is directly relevant to the residual and to `13`'s overlapping-contexts question.

## Lock-free concurrency

The design rationale is written down, which is rare (`docs/technical/concurrency.md`):

> Unlike those DVCSs, Jujutsu treats concurrent edits the same whether they're made locally or remotely.

Two stated reasons for avoiding lock files: they fail on distributed filesystems, and fine-grained locking is complex ("you need to verify that any assumptions you made before locking are still valid after you take the lock").

The protocol:

> When a command starts, it loads the repo at the latest operation. Because the associated view object completely defines the repo state, the running command will not see any changes made by other processes thereafter. When the operation completes, it is written with the start operation as parent. **The operation cannot fail to commit** (except for disk failures and such). It is left for the next command to notice if there were divergent operations.

Divergence is repaired by **3-way merge of the view objects** against their common ancestor, with conflicts recorded *in the resulting view* — i.e. the merge is total, because its codomain admits conflicts.

Storage of the op-log head is a small trick worth recording (`concurrency.md` §Storage): the current head IDs are **filenames in a directory, with no contents**; a new operation writes a new file then deletes the old one, and "writing the new file is what makes the operation visible." Atomicity from `create` + `unlink`, no lock.

**Relevance.** `../../related-work/08-agentic-vc.md` records STORM's argument that git-worktree-per-agent "defers conflict resolution to a post-hoc merge step where recovery is expensive," and that write-time mediation beats it. jj is a third position: **no mediation, no locking, and a merge that cannot fail** — because the state space includes conflicts. For arbor, whose store is immutable and append-only, the cost of that third position is lower still: only `N` and `K` can conflict at all.

## Time travel

`--at-op=<id>` loads the repo at any operation, and running a mutating command there **forks the operation log** — *"running commands on a repo loaded at an earlier operation works the same way as if the operations had been concurrent"* (`docs/operation-log.md`). `jj undo`, `jj op revert`, and `jj op restore` are the user-facing operations over that DAG.

Note what this buys, against the core tenet *"Make it incredibly hard to lose work in your repo"*: because the working copy is auto-snapshotted into the op log (`03-change-ids-and-conflicts.md`), and the op log is a content-addressed DAG, **there is no state a user can reach by editing that is not recoverable**. That, and not the commit command, is what removes the need for a commit gate.
