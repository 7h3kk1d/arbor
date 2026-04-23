# p7 open questions

## Positions taken here (recorded for easy reversal)

See `decisions.md` for full rationale on each.

- Browser-only; no server; in-memory only.
- Bonsai for UI; UI in OCaml, substrate in Reason.
- Vendored p6 substrate, frozen except for the `digestif.ocaml` + library-rename dune edit.
- Ingest on every debounced keystroke; Bind is the sole deferred stateful op.
- "Named only" browser filter by default.
- Rebind confirmation dialog enumerating direct callers.
- Clickable child chips via a web-side rich-surface traversal that mirrors `surface_of_hash_ctx`.

## Still open — intentionally deferred

### Name-structure conventions

Are hierarchical names (`math.not`, `bool.true`, `lc.id`) helpful? `04-naming-layer.md:23–26` says the substrate treats names as opaque strings and leaves structure to the editing layer. p7 uses flat names like p1–p6. A flat global namespace becomes unwieldy past ~50 bindings; the browser's search box is the fallback for now. A future prototype may want tree-structured namespaces or language-qualified prefixes.

### Aspects-by-value dedicated pane

p7 surfaces aspect-by-value queries as click-to-filter chips on the Detail view (type chips filter the browser list by type; eval-result chips filter by value). A dedicated pane would let the user start from an aspect value and browse outward. Deferred; the filter-chip shape is cheaper and may be enough.

### Persistence (localStorage / IndexedDB)

Would let sessions survive reload. Serialization needs a canonical form for `Store`, `Namespace`, and `Attachment` — touches `02-definitions-and-derived-data.md` §Serialization, which is itself deferred at the substrate level. Not in p7's scope.

### URL-as-state / deep-linking

`#detail=1ec244c7`, `#filter=lang:stlc+type:Bool->Bool`, etc. `Bonsai_web.Url_var` is the hook. Adds real value for sharing a view with another user (which is itself out of scope today). Deferred.

### Graph / node-link visualization

A sibling view to the list — render the Store as a DAG with hash nodes and reference edges. Quite different UI shape; may deserve its own prototype.

### Rebind-AND-upgrade-callers tooling

`04-naming-layer.md:83–85` and `CLAUDE.md` both explicitly list automated "update all callers" as a non-goal for now. An editor affordance that offers to rebuild each caller against the new hash (producing new caller hashes, potentially re-binding their own names) is refactor territory; real design work. Record as a sign-of-the-times note: p7 demonstrates the invariant, it does not try to repair against it.

### Keyboard accelerators

No shortcut scheme in p7. Real editors want Cmd+Enter, Esc-to-cancel, navigation keys. Deferred.

### Transitive (vs direct) caller scan for rebind dialog

Direct callers only today. A deeply-nested term may have meaningful indirect pinning; surfacing the transitive closure could help or could swamp the dialog. Deferred until we see real data.

### Author-view "commit variants" workflow

Today, Bind clears `author_bind_as` but keeps the buffer, so the user can bind under a second name by typing it and pressing Enter. A dedicated "save as variant" workflow with preview panes for multiple drafts is a real feature; not in p7.

### Un-named hash proliferation — soft-GC or "clear unnamed"

The ingest-on-keystroke decision produces a lot of intermediate hashes. "Named only" filter hides them, but the Store grows monotonically through a session. If this becomes unworkable in practice, a "clear unnamed" action (delete every hash not referenced by a namespace binding, not an aspect value, not a transitive closure of either) may be needed. This may feed back into substrate-level GC design; `03-content-addressing.md` already defers GC, so p7 is on the right side of substrate policy for the moment.

### Feedback-computation reentrancy

Expected to be handled by Bonsai's `Clock.debounce` (coalesce) and the idempotence of `Store.ingest_*`. If we see misfires — double version bumps for the same term, or UI flicker — the mitigation is to track the buffer epoch inside the computation and drop `Feedback_updated`s for stale epochs. Verify during implementation.

### Eventual server tier for Hazel/Grove collaborative editing

`07-hazel-substrate.md` research line §10 (Grove / UID-based edit layers) is the likely destination for multi-user collaborative structure editing. That needs a server, a sync protocol, and — for Grove specifically — a richer data model than p7's single-namespace store. Not for this prototype; mentioned here so a future prototype's scope doc can pick the thread up.

### Bootstrap: single canonical seed vs. selectable scenarios

p7 auto-seeds one canonical demo set on load (lc combinators, stlc fundamentals, aliasing, partial-translator refusal candidate). A more flexible version would offer several scenarios ("empty", "lc basics", "stlc basics", "translator demo") via buttons, or a REPL-style `:load` textarea that accepts arbitrary scripts. The current single-seed is adequate for demonstrating the substrate's story; multi-scenario is deferred until someone wants a specific demo the canonical seed doesn't cover.

### Orphaned aspects on a soft-GC'd hash

If we ever add a "clear unnamed" action (above), what happens to aspect entries attached to the hashes being removed? The attachment store is keyed on `Hash.t`, so entries become garbage-equivalent. Do we delete them eagerly, or leave them to be cleaned up by an orthogonal pass? Tied to the GC question; same answer for now.
