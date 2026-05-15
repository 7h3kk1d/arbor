# Phase 11 — Mint threads, binding history, and update strategies

**Status:** Scope doc for the eleventh prototype.
**Design context:** `../../design/10-minted-identity.md` §"Marks that survive content edits" (mint marks as a thread-identity preserved across edits), `../../design/04-naming-layer.md` §"Update strategies" (pin / follow / explicit recast as points in scope × user-in-loop, with substrate primitives), `../../design/open-questions.md` §"Update strategies" and §"Minted identity", `../../design/06-architecture.md` (four-layer decomposition).
**Prototype design lives here:** `docs/prototypes/p11-mint-threads/`.
**Implementation code lives at:** `prototypes/p11-mint-threads/`.

## Thesis

p10 implemented the *simple* reading of minted identity — every Term/Type/Label mints a fresh mark on creation, an edit produces a new definition with a new mark. p11 picks up the *harder* reading both `10-minted-identity.md` and `04-naming-layer.md` leave explicitly unexercised: the mint mark as a **thread identity** the user can preserve across content edits, plus the substrate primitives needed for **update strategies** to be real operations rather than editor reconstructions.

Three concepts become first-class substrate machinery:

1. **Mint threads.** An explicit *edit-of-X* gesture ingests a new definition whose mint mark is inherited from X. Two definitions with the same mark but different content hashes are *versions of one thing*. The substrate exposes the grouping (`Store.thread_of`); the editor uses it. Plain re-ingest under the same name *still mints fresh* (the explicit gesture is the signal).
2. **Append-only binding history.** Every namespace name carries `list((option(Hash.t), float))` — bind, rebind, and unbind all append. Orphans (hashes appearing in history but not current) render as `name(vN)` in pretty output, making history load-bearing for the interface (per `04-naming-layer.md` §"Update strategies": "rendering it as a bare hash is hostile").
3. **Update strategies as substrate ops.** Pin (orphan), Follow (auto-cascade), Explicit migrate (per-site subset) all sit on three primitives:
   - `Store.callers_of(hash)` — reverse-DAG query maintained as an ingest-time side-index.
   - `Store.multi_rebind(updates)` — atomic namespace bundle.
   - `follow-clean:v1` derived aspect — per-(h_old, h_new) cached dry-run result, short-circuited by type-preserving edits.

The prototype's job is to find out whether mint threads + binding history + cascades produce a coherent feel: does editing `Math.add` and watching `Math.inc` follow automatically actually work? Where does the cascade abort? Does orphan accumulation clutter the namespace tree, and does the `latest-only` toggle redeem it?

Lineage: p10 (mint-by-default, labels, records, tuples, lists, Bonsai UI) → p11 (fork, add edit-of gesture, mint threads, binding history, three update strategies).

## Questions this prototype should answer

1. **Does the explicit "edit of X" gesture feel natural, or do users want rebind-preserves-mint as the default?** p11 commits to explicit-gesture-only; the open question is whether that's the right call or whether it forces too much ceremony.
2. **Is `follow` usable in practice?** When `Math.add` is edited to a type-preserving form, does the cascade through `Math.inc`, downstream user terms, etc. run silently? Where does it abort?
3. **Does `follow-clean:v1` pay off as a cached aspect, or is it cheap enough to recompute every time?** The dry-run cascade is non-trivial; caching is the obvious move, but mint-by-default already accumulates aspect rows quickly.
4. **What does the namespace tree look like with orphans visible?** Does `Math.add(v1)` / `Math.add(v2)` / current `Math.add` clutter the tree? Does the latest-only toggle redeem it, or is a thread-grouping view needed?
5. **Does explicit migration's per-site toggle UI scale beyond toy call-graphs?** Or do we need rules/filters layered on top of per-site choices once the graph is more than a handful of nodes?
6. **Are mint threads visible enough?** Should the detail pane's mint-thread section be the primary affordance, or does the namespace tree itself want a thread-grouping mode?

## New substrate concepts (deltas from p10)

### Mint threads

A *mint thread* is the set of definitions sharing a mint mark. p10 already encodes the 16-byte mark in node bytes; p11 adds:

- **Edit gesture.** `Store.edit_of(old_hash, new_surface_ast)` ingests `new_surface_ast` but uses `old_hash`'s mint mark instead of drawing a fresh one. Produces a new hash (different content → different hash by construction); the two hashes share a mark. Failure modes: `old_hash` not found; sort mismatch (cannot edit a Type into a Term); ingest errors propagate as usual.
- **Reverse-mint query.** `Store.thread_of(hash) : list(Hash.t)` returns all hashes sharing this mark, in ingest order. Implementation: a `mark → list(hash)` side-index maintained at ingest. A singleton thread (the common case) returns `[hash]`.
- **Mint mark visibility.** Marks remain opaque (16 bytes). The substrate exposes only thread membership and a stable short rendering (`m_<hex8>`) for UI labels — never the raw mark bytes to user code.

### Append-only binding history

- **`Namespace.history(name) : list((option(Hash.t), float))`** — every `bind` / `rebind` appends `(Some h, now())`; `unbind` appends `(None, now())` (a tombstone so the log records the unbind moment). Order is newest-first.
- **Orphan rendering.** A hash that appears in `name`'s history but is not the current binding renders as `name(vN)` where `N` is its 1-indexed position counting *from the oldest binding forward*. Implemented in `Pretty` via `Namespace.reverse_lookup_with_version : Hash.t → option((string, int))`.
- **No commit-message annotations in v1.** History rows are `(option(hash), timestamp)` only. Per-row annotations as aspects on history entries are deferred per `04-naming-layer.md` §"Update strategies" — "annotations layer as aspects on history entries, not built into the entry itself."

### Update strategies

Three operations on top of three primitives.

**Primitives:**

- `Store.callers_of(hash) : list(Hash.t)` — reverse-DAG query. Maintained as a side-index on Node ingest: every time a Node is registered, each child hash gets an entry pointing back to the new parent's hash. For Definitions, the immediate parent is the Definition's hash; for shared sub-DAG nodes, multiple parents accumulate. Result is deduplicated.
- `Store.multi_rebind(updates: list((name, Hash.t))) : result(unit, string)` — atomic. All-or-nothing: validate each `(name, hash)` (hash exists in Store; if name already bound, allow the rebind; if name unbound, treat as fresh bind), then commit history rows for the whole batch under a single namespace transaction.
- `follow-clean:v1` aspect. `Aspect_value.Follow_clean(bool)` keyed by `(h_old, h_new)`. Computed by dry-run cascade: for each direct caller of `h_old` (and recursively), substitute `h_new`, re-canonicalize, re-typecheck the new caller. If every recursion typechecks, the result is `true`; on first failure, `false`. **Type-preserving short-circuit:** if both `h_old` and `h_new` carry the same `Type_of` aspect (same type hash) *and* the edit was via `Store.edit_of` (pure mark-preserving substitution), the result is `true` without running the cascade. Cached in the aspect store on first compute, keyed by pair (encoded canonically).

**Strategies (in `Update_strategy` module):**

- `pin(name, new_hash)` — a plain rebind. Old hash becomes orphan; binding history grows by one. The substrate's resting behavior; the user is making the active choice not to propagate.
- `follow(name, new_hash)` — compute reverse-DAG closure from old hash; for each reachable caller, build a substituted variant via `edit_of` (preserving the caller's own mint thread!), typecheck, accumulate as `(name', new_hash')` pairs. Then `multi_rebind` the whole batch. Aborts on any typecheck failure — partial state never reaches the namespace. Returns the list of names that would have been touched on success.
- `explicit_migrate(name, new_hash, sites: set(Hash.t))` — same machinery as `follow` but restricted to the user-selected subset. Unselected callers stay pinned to the old hash; binding history surfaces the divergence.

### Sort guarantees on `edit_of`

`edit_of(old, new_surface)` requires `old` and the result of ingesting `new_surface` to share the same `Definition.t` constructor (`Term`, `Type`, or `Label`). Cross-sort edit is rejected at the Store boundary, not silently allowed.

## Architecture mapping

Four layers, deltas from p10:

- **Store.**
  - `mark → list(hash)` side-index (mint thread membership). Maintained when a Definition is ingested.
  - `child_hash → list(parent_hash)` side-index (reverse-DAG). Maintained when a Node is ingested; dedup at query time.
  - `Store.edit_of(old, surface)` ingest variant that reuses `old`'s mark.
  - `Store.multi_rebind` atomic bundle (composes existing per-name rebind).
- **Attachment.**
  - `Aspect_value` extends with `Follow_clean(bool)`.
  - Pair-keyed aspect support for `follow-clean:v1`. Today's key is `(hash, aspect-id)`; the simplest extension is encoding `(h_old, h_new)` as `BLAKE2B(h_old || h_new)` and storing under that synthetic key. Decision: synthetic-key encoding (lightest touch; no schema change).
  - `Namespace` adds `history` table: `name → list((option(Hash.t), float))`. `bind`/`rebind`/`unbind` all append. `reverse_lookup_with_version` walks history.
- **Language.** Unchanged at parser/checker level. p10's surface stays. Surface additions are at the editor pane, not the grammar:
  - **Edit button** on a selected definition: loads the current binding's source into the buffer (re-derived via `Pretty`) and switches editor to "edit mode" — the next ingest from this buffer goes through `Store.edit_of(current_hash, ...)` rather than `Store.ingest(...)`.
  - **Strategy chip** (Pin / Follow / Migrate) next to the bind button. Default: Pin.
- **Interface.** Bonsai + jsoo, three-pane structure unchanged. Deltas:
  - **Namespace tree.** Orphans render as collapsed children of their name (`Math.add` shows `(v1)`, `(v2)` chips beside the current entry). A **latest-only toggle** at the top of the tree hides orphans.
  - **Editor pane** gains an **Edit** button on the detail pane that triggers edit-mode for the selected definition. A mode chip on the editor shows "new ingest" vs "editing `Math.add` (thread `m_8a3...`)".
  - **Strategy chip** (Pin / Follow / Migrate) next to the bind button, defaults to Pin.
  - **Detail pane** gains three sections:
    - **Mint thread** — all hashes sharing this mark, with current/orphan markers, click-to-jump.
    - **Binding history** — for the name currently rendering this hash (if any), `list((option(hash), timestamp))` as rows.
    - **Callers** — reverse-DAG query result, click-to-jump.
  - **Migrate dialog** — when strategy is Migrate, the bind action opens a modal listing reachable callers with checkboxes; commit triggers `multi_rebind` on the chosen subset.
  - **Follow-clean indicator** — when strategy is Follow, the bind button shows a green badge ("cascade verified, will run silently") or amber badge ("cascade requires intervention; switch to Migrate?") based on `follow-clean:v1`.

## Module additions / changes

```
prototypes/p11-mint-threads/src/
  mint.re                  # API to "use this specific mark" added (otherwise unchanged from p10)
  store.re                 # + edit_of, callers_of, thread_of, multi_rebind; mark→hashes index, child→parent index
  attachment.re            # + Follow_clean(bool); + pair-keyed aspect support (synthetic-key encoding)
  namespace.re             # + history table; + reverse_lookup_with_version
  update_strategy.re       # NEW — pin / follow / explicit_migrate
  follow_clean.re          # NEW — derived aspect; dry-run cascade with type-preserving short-circuit
  resolver.re              # + ingest_as_edit_of(old_hash, surface_ast)
  pretty.re                # + orphan rendering as `name(vN)`
  (other files copy from p10 verbatim — language tag byte bumps 'Q' → 'R')

prototypes/p11-mint-threads/web/
  state.ml                 # + editor mode (new vs edit-of), + strategy selection
  editor.ml                # + edit button hook, + mode chip, + strategy chip
  detail.ml                # + mint thread / history / callers sections, + Edit action
  migrate_dialog.ml        # NEW — per-site toggle modal
  namespace_tree.ml        # + orphan rendering, + latest-only toggle
  bootstrap.ml             # seeds an edit chain on Math.add so the UI demos on first load
```

## Bootstrap seeds (UI demo material)

The bootstrap should make the new affordances visible without typing:

- `Math.add` exists in three versions in the same mint thread (initial `\x:Int -> \y:Int -> x + y`, a refactor with `let`, a hole-free final form). Binding history has three rows; namespace tree shows the latest plus `(v1)` and `(v2)` orphan chips.
- `Math.inc` calls `Math.add` and has been auto-followed across one of those edits. Reverse-DAG from any version of `Math.add` shows `Math.inc` (and any user-created callers).
- Everything else from p10's bootstrap (`Geom.Point`/`Vector`, `List.range`/`sum`, `String.greet`, primitives, `Draft.todo` holey) is preserved.
- One additional **holey orphan** (`Draft.todo(v1)`) demonstrates orphan rendering with a has-holes badge.

## In scope

- Edit-of gesture preserving mint marks across content edits.
- Append-only binding history per name; orphan rendering as `name(vN)`.
- Reverse-DAG (`callers_of`) and mint thread (`thread_of`) side-indexes maintained at ingest.
- Three update strategies: Pin (resting), Follow (auto-cascade via atomic multi-rebind), Explicit migrate (per-site toggle UI on top of the same machinery).
- `follow-clean:v1` derived aspect with type-preserving short-circuit.
- UI: edit button, strategy chip, migrate dialog, mint thread + history + callers sections in the detail pane, latest-only namespace toggle.
- Substrate test suite covering: mint-thread preservation under `edit_of`, history append on bind/rebind/unbind, callers_of correctness across the bootstrap DAG, follow cascade end-to-end on a type-preserving edit, follow abort on a type-breaking edit, explicit-migrate partial cascade leaves selected sites updated and unselected sites pinned, `follow-clean` cache hit on a repeated dry-run.

## Out of scope

- **Rebind-preserves-mint as default.** p11 commits to explicit gesture only.
- **Commit-message-shaped annotations on history rows.** Just `(option(hash), timestamp)`.
- **Patch as a content-addressed artifact.** The migrate dialog's working set is in-memory editor state.
- **Rules-and-filters over per-site toggles** (e.g., "migrate all callers under `Math.*`"). Per-site only in v1.
- **Persistence.** Bonsai bundle is stateless across reloads; bootstrap re-seeds with fresh marks each load. History accumulates within a session only.
- **Cross-namespace branching.** Single namespace, like p10.
- **Garbage collection of orphan hashes.** Orphans accumulate.
- **Editor history beyond binding history.** No undo/redo; no Grove-style edit-layer UIDs (future prototype per `07-hazel-substrate.md`).
- **Translation / multi-language.** Single language; hash space tag bumps to `'R'` to mark incompatibility with p10's `'Q'`.

## What "done" looks like

- `dune build && dune runtest` green; ≥40 substrate tests including the cases above.
- `scripts/build-web.sh` produces `public/p11.js`. Opening `public/index.html` shows the three-pane app with the seeded mint chain visible.
- Manual UX checks:
  - Bootstrap loads; `Math.add` shows in the tree with current binding plus `(v1)`/`(v2)` orphans; detail pane on the current `Math.add` shows three mint-thread rows and three history rows.
  - Click an orphan: detail pane renders the orphan; binding history shows the rebind that orphaned it.
  - Edit a term: select `Math.inc`, click **Edit**, change body, choose Follow, click bind. Cascade runs silently; `Math.inc` and downstream callers get new hashes inheriting their respective mint marks; binding history grows by one for each affected name.
  - Edit with a type-breaking change + Follow: cascade aborts pre-commit; UI surfaces which caller broke; suggests switching to Migrate.
  - Migrate opens dialog; user unchecks one caller; commit updates only the checked subset; unchecked caller remains pinned to the old hash.
  - Pin (default) on a rebind: old hash orphans, namespace tree adds a `(vN)` chip, no cascade runs.
  - Toggle "latest only" in the namespace tree: orphans hide; current bindings remain.
  - Same source ingested twice via `Store.ingest` (no `:edit`) produces two distinct hashes in two distinct mint threads — p10's mint-by-default semantics unchanged.

## Directory layout

```
prototypes/p11-mint-threads/
  dune-project
  p11_mint_threads.opam
  _opam/                            # symlink to p7-web-interface/_opam/_opam
  src/
    (everything from p10 verbatim, plus update_strategy.re, follow_clean.re,
     and edits to store.re / namespace.re / attachment.re / resolver.re / pretty.re / node.re)
  web/
    (everything from p10 verbatim, plus migrate_dialog.ml and edits to
     state.ml / editor.ml / detail.ml / namespace_tree.ml / bootstrap.ml)
  bin/
    p11_main.ml
  test/
    test_p11.re                     # ≥40 tests
  scripts/
    build-web.sh                    # dune build + cp to public/p11.js
  public/
    index.html
    styles.css
    p11.js
```

## Tech stack

Identical to p10: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.17, Menhir 3.0 (`--table`), menhirLib, ppx_deriving, digestif (`digestif.ocaml` under jsoo), js_of_ocaml ≥ 5.6, Bonsai/Virtual_dom/Core v0.17, ppx_jane, alcotest, qcheck, qcheck-alcotest. `_opam` symlinks to `p7-web-interface/_opam/_opam`.

## Open threads to track

These start in `open-questions.md` for the prototype and may bubble up to substrate docs as findings:

- Rebind-preserves-mint as a default once users have lived with explicit-gesture and orphan rendering. Possibly the right answer flips once orphan UX is in hand.
- `follow-clean:v1` cache value vs. recompute cost under mint-by-default's hash churn.
- Migrate dialog "preview cascade output" affordance (diff view) before commit.
- Orphan accumulation: at what scale does the namespace tree become unusable, and what's the right pruning gesture?
- Rules-and-filters over per-site toggles: when does this stop being optional?
- Mint thread surfacing: detail-pane section vs. namespace-tree thread-grouping mode.
- Whether `follow-clean` should be a stored aspect at all, or only session-computed (per `open-questions.md` §"Update strategies").
- What happens when `edit_of` is invoked across sort boundaries — currently rejected at the boundary, but worth surfacing the error in the UI cleanly.
