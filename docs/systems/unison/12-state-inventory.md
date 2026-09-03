# Unison: everywhere state is maintained

**Source read** at `db60ce2` (2026-08-07), the same pin as the rest of this directory. Runtime
observations against `ucm release/1.1.1`. Evidence tags follow `00-index.md`.

This is a census rather than a narrative: *what does Unison remember, where, and for how long.* The
organizing axis is *lifetime*, because that is what turned out to be load-bearing — the same
question ("is this rebuildable?") gets a different answer at each tier, and the tiers do not line up
with the module boundaries.

Four tiers, and the surprise is how lopsided they are:

| tier | where | rough size | invalidation |
|---|---|---|---|
| **Content-addressed record** | SQLite: `hash`, `object`, `hash_object`, `text`, `causal` | the codebase | append-only, never deleted |
| **Workspace record** | SQLite: `project*`, `current_project_path`, `config`, `history_comments*` | small | mutated in place |
| **Derived** | SQLite: `dependents_index`, `find_type*`, `namespace_statistics`, `squash_results` | ~1k rows | mostly *no rebuilder* |
| **In memory** | `LoopState`, the runtime `CCache`, ten codebase caches, LSP | **unbounded** | almost entirely **never** |

The last row is the finding. The database is disciplined and well-commented; the in-memory tier is
where the unbounded, never-invalidated state actually lives, and it is invisible from the schema.

---

## 1. SQLite — 43 tables, and seven of them are dead

Schema version **26**. One file, `<root>/.unison/v2/unison.sqlite3`, WAL mode.

**System of record.** `hash` (interned base32, append-only — nothing ever deletes a hash),
`text` (interned strings), `object` (the bytes: term components, decl components, namespaces,
patches), `hash_object` (the many-hashes-to-one-object indirection), `causal` + `causal_parent`
(the history DAG), `watch` + `watch_result` (the evaluation cache), and the workspace tables
(`project`, `project_branch`, `project_branch_parent`, `project_branch_reflog`,
`current_project_path`, `most_recent_branch`, `remote_project*`, `config`, `history_comments`,
`history_comment_revisions`, `key_thumbprints`).

**Derived.** `dependents_index`, `find_type_index`, `find_type_mentions_index`,
`namespace_statistics`, `squash_results`, `namespace_unique_type_guid`.

> **Only one derived table can heal itself.** `namespace_statistics` is recomputed on read when a
> row is missing (`Operations.hs:1205-1213`). The others are built *only* incrementally at
> component-save time; there is no rebuilder anywhere. Drop `dependents_index` and the only recovery
> is a full re-import. That is worth knowing before treating "derived" as "cheap to lose".

**Transient.** `temp_entity`, `temp_entity_missing_dependency` — sync-v1's out-of-order download
staging, drained by a cascade that fires on every object insert. Sync-v2 doesn't use them and
*clears them first* so that per-insert check stays cheap. `merge_branch`, `update_branch`,
`upgrade_branch` — where a failed merge/update/upgrade parks you.

### Dead tables `[source-verified]`

| table | status |
|---|---|
| `causal_metadata` | **Never used, ever.** The string appears in exactly two places in the whole repo, both in `create.sql`. Its own comment says `-- Currently unused.` |
| `namespace_root` | Superseded in schema 17. The root set is now `project_branch.causal_hash_id` — one causal root per branch instead of one global root. Only migration code writes it. |
| `reflog` | Deprecated in favour of `project_branch_reflog`; only the 5→6 migration writes it. Read-only via `deprecated.root-reflog`. |
| `name_lookups`, `scoped_term_name_lookup`, `scoped_type_name_lookup`, `name_lookup_mounts` | **The population code was deleted**, not merely disabled — `c90ec8aa2` (2025-09-18) removed `NamesPerspectives/Queries.hs` (882 lines) and five sibling modules, taking both the read and write sides. No `INSERT INTO` any of the four survives in the Haskell source. The unscoped `term_name_lookup` was `DROP TABLE`d back in schema 12. |

This confirms `05-deletions.md`'s account of the name-index removal from the code rather than from
the commit message.

### Details worth recording

- **`hash_version` is a dead column pinned at `2`.** Every write hardcodes it, with a standing TODO.
  Version `1` existed on schema-1 codebases and was deleted wholesale by migration 3, for a
  documented reason: a sync race could leave two databases mapping the same v1 hash to two
  *different* v2 objects, which crashed sync. So the indirection `rem:version` describes is real,
  designed, exercised exactly once, and then deliberately emptied.
- **No duplicate rows, despite missing constraints** `[verified empirically]`. `dependents_index`
  and `find_type_mentions_index` have no `UNIQUE` constraint, so their `ON CONFLICT DO NOTHING`
  clauses can never fire. On a real codebase, 169/169 and 607/607 rows are distinct — the upstream
  guards hold.
- **The hash table is a superset of object hashes** `[verified empirically]`: of 708 hashes, 274 are
  object primaries, 218 are causal self-hashes, and **216 are neither** — derived type references
  recorded by the type indices, plus the one unnamed watch expression.

---

## 2. In memory — where the unbounded state actually is

### The runtime code cache is the big one

`CCache` (`Machine/Types.hs:161-180`) is twelve `TVar`s holding compiled bytecode, ANF source,
sandbox facts, optimizer facts, and reference↔index maps. `EvalCtx` (`Interface.hs:163-170`) adds
five more monotonic maps. Both live in the `Runtime` closure for the whole process, and **two
independent copies exist** — sandboxed and not.

Three things about it:

1. **It is a real content-addressed compiled-code cache.** Keyed by `Reference`, checked by
   `loadCode` (`Interface.hs:415-424`), which skips anything already in `refTm`. Sound, because a
   changed definition gets a new hash.
2. **It also caches *evaluated values*.** `checkCacheability` marks a definition cacheable iff its
   type has no arrows; `preEvalTopLevelConstants` then eagerly evaluates every such definition at
   load time and rewrites the bytecode to point at a `CachedVal`. This is the in-memory sibling of
   `watch_result`, and it is not visible anywhere in the schema.
3. **Nothing ever invalidates or evicts any of it.** There is no bound, no eviction, no `clear`.
   A long session that repeatedly edits and runs a scratch file accumulates one full compiled copy
   per distinct hash, permanently. `debug.clear-cache` clears the SQLite watch table *only* —
   **there is no command that clears any in-memory state.**

### Pre-evaluation is eager over the closure `[verified empirically]`

Measured by putting an expensive arrow-free constant in a closure where nothing forces it:
`expensive : Nat` (~5.8 s), reachable only through the body of `usesExpensive : Nat -> Nat`, which is
the sole element of `boxE : [Nat -> Nat]`. Three fresh processes against one saved codebase:

| watch | closure contains | result | elapsed |
|---|---|---|---|
| `> List.size boxC` | `cheap` | `1` | 0.12 s |
| `> List.size boxE` | `expensive` | `1` | **5.87 s** |
| `> expensive` | `expensive`, forced | the number | 11.57 s |

Row 2 returns the length of a one-element list and pays 5.87 s for a value it never reads. So the
arrow-free subset of the *whole transitive closure* is evaluated before the program starts, driven by
reachability in the code rather than by what the run needs.

**The two caches do not talk, but one short-circuits the other.** After that run, `watch_result` holds
exactly one row — the watch's own result — and `expensive`'s value is nowhere; the constant cache is
in-memory only. Yet a second run in a *new* process took 0.11 s, because the watch was a cache hit and
`evaluateWatches` returned **before any code loaded**, so pre-evaluation never ran. The persisted cache
therefore does not cache constants; it skips the load that would have computed them, all-or-nothing on
the top-level expression. Note that `run main` can never benefit: `RunWatch` is *"never cached — it
re-evaluates on every save."* A `run` always pays the full eager pass.

This also settles the ordering: the watch-cache lookup precedes code loading.

### A watch expression is cached more durably than a definition `[source-verified]`

`evaluateWatches` (`Runtime.hs:106-161`) builds `m` from `UF.hashTermsId tuf` — **every** term in the
file, plain definitions included — and then consults the persisted cache for all of them:

```haskell
  m' <- fmap Map.fromList . for (Map.toList m) $ \(v, (r, t)) -> do
    o <- if isRunWatch v then pure Nothing else evaluationCache r
```

But its result map is an intersection with the watches only:

```haskell
  watchMap = Map.intersectionWithKey go (Map.fromList (toList watches `zip` results)) m'
```

and `Load.hs` writes `putWatch` over exactly that map. So **for a top-level definition the read path
exists and the write path does not.** The only writers of `watch_result` are `Load.hs` (watches),
`Tests.hs` (tests, under the definition's own reference), and `RuntimeUtils.hs` (an ad-hoc evaluated
term, keyed by `hashClosedTerm`).

The consequence reads backwards and is worth stating plainly: **a one-off `> expr` you typed and
deleted has its value on disk, while a named constant you committed does not.** Committing changes
nothing about a constant's value — only its code. A plain non-test constant's value lives *only* in
the in-memory `CCache`, whether or not it has ever been committed.

The definition-side lookup is not dead code: it is what finds a committed *test*'s cached result,
since `Tests.hs` does write under a definition's own reference. Plain non-test constants fall in the
gap between the read and the write.

### Pre-evaluation does not benefit from its own results `[verified empirically]`

`> expensive` cost 11.57 s — twice a single evaluation — and the reason is an ordering in
`preEvalTopLevelConstants` (`Machine.hs:1659-1679`):

```haskell
  for_ (EC.mapToList cacheableCombs) \(w, _) -> do
    let hook xstk = ... modifyTVar evaluatedCacheableCombsVar $ EC.mapInsert w ...
    apply0 (Just hook) cc activeThreads w        -- ← against cc AS IT IS
      `catch` ...
  evaluatedCacheableCombs <- readTVarIO evaluatedCacheableCombsVar
  let allNew = evaluatedCacheableCombs <> newCombs
  -- Rewrite all the inlined combinator references to point to the
  -- new cached versions.
  atomically $ modifyTVar (combs cc) (...)         -- ← only after the loop
```

Every cacheable comb is evaluated against the **un-rewritten** cache, with results accumulating in a
*local* `TVar` that nothing consults during the loop. The `CachedVal` rewrite lands only afterwards.
So a cacheable constant that references another cacheable constant re-evaluates it from scratch, and
the pass never benefits from work it has just done.

For `> expensive` the cacheable set has two members: `expensive`, and the watch expression itself —
whose type is `Nat`, hence arrow-free, hence cacheable — whose body is a reference to `expensive`.
Both are evaluated independently.

Measured on a chain `base ← k1 ← k2 ← k3 ← k4` of arrow-free `Nat`s, each defined as the previous,
with `base` costing ~1.45 s. A watch reaching depth *k* through a never-applied lambda:

| closure reaches | elapsed | multiple of base |
|---|---|---|
| `base` | 1.58 s | 1.00 |
| `k1` | 3.04 s | 2.01 |
| `k2` | 4.47 s | 3.00 |
| `k3` | 5.92 s | 3.99 |
| `k4` | 7.34 s | 4.98 |

Exactly the number of cacheable constants in the closure. Where only the base is expensive this is
linear in chain length; where each level does its own costly work it is quadratic, since evaluating
the *k*-th constant re-runs all *k* below it.

The simplification looks deliberate rather than accidental — `EC.mapToList` gives no topological
order, so installing `CachedVal`s incrementally would need one, and doing the rewrite in a single
batch afterwards is correct and much simpler. The cost appears not to have been noticed. A
topological pass with incremental installation would make it linear in the *number* of constants
rather than in the number times the depth. **This looks worth reporting upstream**; we have not
checked whether it already is.

Also confirmed empirically: `add` writes no `watch_result` rows. Committing evaluates nothing.

### `LoopState` — six fields, threaded, and rolled back on error

`Cli/Monad.hs:192-217`. `projectPathStack`, `latestFile`, `latestTypecheckedFile`, `lastInput`,
`numberedArgs`, `lastRunResult`. Threaded functionally from command to command; on SIGINT or an
uncaught exception the driver **discards the result state and re-loops with the previous one**, so
an aborted command's state changes roll back.

Two notes. The module's own export header labels this section `-- * Immutable state`, which is
exactly backwards. And it still carries the commented-out `-- , _activeEdits :: Set Branch.EditGuid`
with a `-- TBD` — the reified-edit machinery deleted in 2025 (`05-deletions.md`), fossilised in
place.

`label`/`returnEarly` smuggle `LoopState` through a Haskell exception and recover the payload with
`unsafeCoerce`, disambiguated by a `Data.Unique`.

### Codebase caches

Ten, created at codebase open, process lifetime (`SqliteCodebase.hs:154-176`): a weak-reference
branch cache invalidated only by GC, and nine fixed-size semispace caches (10 to 8192 entries) whose
only eviction is the semispace flip. Plus `termBuffer`/`declBuffer`, staging for the v1-writes-one-
definition / v2-writes-a-component impedance mismatch.

The SQLite prepared-statement cache is **per transaction**, not per session — `runTransaction`
opens and closes a connection each time.

### LSP

Per client connection. Thirteen mutable fields (`LSP/Types.hs:74-96`). It is the **only subsystem
with a real invalidation protocol**: a `Signal` (a value plus a generation counter) fired by UCM's
`cd`/`switch`/`pop`, which causes `ucmWorker` to refresh four name caches and mark every open file
dirty.

- **It cannot write the codebase.** Every `runTransaction` under `LSP/` is a read; the one
  "write" command sends `workspace/applyEdit` back to the *editor*.
- **But it shares UCM's non-sandboxed `Runtime`** for rendering hover docs. So it can mutate the
  shared code cache UCM is using, and run effectful Unison code, without touching the database.
- Two leaks: closing a file does not remove its entry from `checkedFilesVar`, so a full typechecked
  `UnisonFile` is retained per URI ever opened; and `markAllFilesDirty` iterates VFS keys only, so
  those entries go permanently stale rather than being dropped.

### Global mutable state

**Exactly one** `unsafePerformIO`/`NOINLINE` global in the whole codebase: the credential manager.
Everything else in that idiom is a frozen environment-variable snapshot — about thirty of them,
read once at first force, so changing the environment mid-session has no effect.

`unison-merge/` and `codebase2/` contain **zero** mutable references `[verified by absence]`. The
merge algorithm is entirely pure. The typechecker is too — all its state (`freshId`, the ordered
context, union-find kind constraints, pattern-coverage constraints) is functionally threaded, and
all of it is per-typecheck.

---

## 3. On disk, outside the database

| what | where | notes |
|---|---|---|
| lockfile | `.unison/v2/unison.lockfile` | 0 bytes, pure `flock` target |
| migration backups | `.unison/v2/unison.sqlite3.v<N>.<epoch>` | `VACUUM INTO` before each migration. **Never deleted.** |
| scratch file | `./scratch.u` | written by *prepending*, never truncating, via an atomic temp-file rename |
| REPL history | **`$PWD/.unisonHistory`** | relative path, so it lands in the working directory; read and rewritten **on every prompt** |
| credentials | `$XDG_DATA/unisonlanguage/credentials.json` | includes a full **Ed25519 private JWK** used to sign history comments; an unparseable file is silently wiped |
| compiled programs | `<name>.uc` | version string + short hash + entry point + a `StoredCache`. **Not content-addressed** — the filename is the user's |
| sync files | `<name>.usync` | gzip'd CBOR of the same entity stream the network protocol uses |
| merge/diff tool inputs | `$TMPDIR/unisonNNNN/*.u` | **never cleaned up** |

**There is no configuration file.** `.unisonConfig` appears only in help text; there is no config
parser and no `configurator` dependency. Configuration moved *into* SQLite, and the entire set of
settings is one key: `author.name`. Everything else is an environment variable — about 30 of them,
`UNISON_*` and `UCM_*`.

**There is no logging to disk, no crash reporting, and no telemetry** `[verified by absence]`.
Debug output goes to stderr; a crash prints a GitHub issue URL and transmits nothing.

**The transcript `-c` flag is ignored.** Plain `ucm transcript` always creates a *new* codebase at
its working directory (`Main.hs:507`); only `transcript.fork` copies the source. Separately, bare
`--save-codebase` (no path) falls through to the delete branch, so it **deletes** the codebase it
claims to save — the flag's own help text says the opposite. A regression from `8f16699f5`.

---

## 4. Remote

Sync-v1 is JSON, sync-v2 is CBOR streams; both over HTTPS with a bearer token. Nothing is cached on
the filesystem — downloaded entities go straight into SQLite.

The two versions stage differently, and it is a clean illustration of the same problem solved twice.
**v1** downloads out of order and parks entities it cannot yet store in `temp_entity`, with a row per
missing dependency carrying the Share-issued JWT; a cascade on every object insert drains it.
**v2** has the server stream entities *dependency-first*, commits them in batches inside real
transactions, and uses no staging at all — so an interrupted v2 pull leaves a valid complete
sub-DAG, and "resume" is just running it again.

History comments are content-hashed and **signed** with the personal key from `credentials.json`, so
they can move peer-to-peer through Share. Editing appends a signed revision; deleting appends one
with `hidden = TRUE`, because *"in a distributed system you really can't ever truly delete comments,
but you can ask to hide them."*

---

## 5. Measured

`[verified empirically]`, `ucm release/1.1.1`, one expensive watch (`loop 0 200000000`, ~5.8 s cold):

| | elapsed |
|---|---|
| one watch, cold | 5.79 s |
| the **same** watch, saved twice in one session | 5.82 s |
| two **different** watches of equal cost (control) | 11.45 s |
| the same watch, **new process**, warm codebase | **0.084 s** |

So the evaluation cache is consulted both across saves and **across processes**, reading
`watch_result` out of SQLite. The control rules out a timing artifact.

*Method note:* the cross-process figure was initially measured wrong, and wrongly reported as
inconclusive, because `ucm transcript -c` silently ignores `-c` — the "warm" run was actually
building a fresh codebase and evaluating cold. `transcript.fork -c` is the flag that reads an
existing codebase.

---

## 6. What this means for arbor

- **The interesting state is the state that is not in the store.** Unison's database is careful,
  commented, and largely append-only; the unbounded never-invalidated state is all in RAM, and none
  of it is reachable from the schema. arbor's `⟨Σ, N, E, H⟩` models the disciplined half. A
  substrate that intends to be *live* will grow the other half, and should decide deliberately
  whether it is a cache (evictable, rebuildable) or a tier of the store.
- **"Derived" is not the same as "rebuildable".** Five of Unison's six derived tables have no
  rebuilder. `02-definitions-and-derived-data.md`'s asserted/derived split is about *provenance*;
  this adds a second axis, *recoverability*, and they come apart.
- **The reverse index is the one that cannot be recovered by walking forward**, which is why Unison
  deleted its forward name index and kept `dependents_index`. That sharpens `05-deletions.md`'s
  finding into a rule with a reason.
- **Evaluated-value caching happens twice, at two tiers**, with two different cacheability rules:
  `watch_result` in SQLite (keyed by a reference that need not denote anything stored — see
  `formalism/paper/mini-unison.tex` `prop:watchdom`) and `preEvalTopLevelConstants` in the runtime
  (gated on "no arrows in the type"). arbor's `E` models the first and has no counterpart to the
  second. The measurement above is the reason to care: the second tier is *eager over the closure*,
  so the cost of loading code is proportional to the arrow-free part of everything reachable, not to
  what runs. A substrate that wants live feedback over a large codebase has to decide whether it is
  doing that, and Unison's answer is only tolerable because the persisted tier usually skips the load
  entirely.
- **One editing context is in memory; a second, incompatible one is in the LSP.** Neither is a
  stored object. `13-editing-context.md`'s "stored or ephemeral" axis has a shipped data point:
  ephemeral, singular, and duplicated by accident.
