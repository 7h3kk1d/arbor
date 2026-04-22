# Open Questions — p3-naming-layer

Prototype-specific open items. Distinguishes "positions taken here (see `decisions.md`)" from "intentionally deferred."

## Positions this prototype takes

These are active stances that resolve previously open sub-questions. Each decision is filed in `decisions.md`; summarized here for cross-reference.

- **Naming scope** (`docs/design/open-questions.md:53`). Single namespace, process-local. Multi-namespace deferred until branching.
- **Name resolution timing** (`docs/design/open-questions.md:54`). Strictly edit time, via `Resolver.resolve` between parse and ingest. Display-time reverse lookup (hash → names) is a separate, read-only query used by the printer — *not* a resolution event; stored programs are already name-free.
- **Rename atomic vs derived** (`docs/design/04-naming-layer.md:93`). Derived: `Namespace.rename` is a convenience wrapping unbind + bind with error guards. No stored primitive.
- **Dangling display** (`docs/design/04-naming-layer.md:94`). Unnamed in-store subterms reconstruct as usual; `<missing h:...>` only for hashes not in the store. Matches p2's fallback behavior.
- **`:list` verbosity with a DAG** (`docs/prototypes/p2-structural-sharing/open-questions.md:27`). Partially addressed — name substitution reduces deep-subterm repetition when bindings exist. Full filtering (only top-level ingested terms) remains deferred.
- **Namespace vs aspect store fusion** (`docs/design/open-questions.md:55`). Separate modules. Revisit when branching arrives, per design doc guidance.

## Still open — intentionally deferred

### Language-layer
- **`Ref(hash)` as a first-class AST constructor.** This prototype inlines resolved subtrees before ingest. A later prototype would introduce `Ref(hash)` as an AST node and modify the hashing scheme to support it. That's most of the rest of roadmap Phase 2.
- **Name structure conventions.** Hierarchy (`module.name`), slash-paths, language qualifiers. Substrate treats names as opaque; the editing layer has not picked a convention.
- **Language-qualified names** (`04-naming-layer.md:92`). Only one language exists in this prototype, so the question does not arise in concrete form.

### Namespace semantics
- **Aliasing disambiguation in display.** The printer uses the alphabetically first name when a hash has multiple bindings. The full alias set is surfaced in `:list`'s brackets and in `:name-of`. Worth revisiting if aliases become common enough to clash visually.
- **Ambiguous-name policy in expressions.** A single namespace cannot bind one name to multiple hashes, so ambiguity in resolution doesn't arise. When multi-namespace arrives, resolution order / ambiguity is a new open question.
- **Rename preserving history.** If history tracking is added, is rename a single logged event or two events (unbind, bind)? Decision above defers this.

### Hash input / display
- **Bare-hex disambiguation with names.** `:bind foo f63e7408` binds to the hash prefix, not to a name `f63e7408` (which would be illegal as an identifier starting with a digit anyway, but something like `deadbeef` collides). Bare hex is currently ≥ 4 chars and lowercase-hex to qualify. Worth tightening to "`#` is required for hash prefixes outside the `:eval <arg>`-style commands" if collisions become a problem.

### Display / interface
- **Should `:lookup <prefix>` use the name-aware printer?** Currently uses the raw printer (its purpose is "show me this stored thing literally"). `:list` uses the name-aware printer. Is that split correct? Provisional answer: yes — they serve different users (debugging the store vs reading your bindings).
- **Should `:eval` output pass through the name-aware printer?** Currently shows the raw reconstructed value (e.g., `succ 0` even if `0` has a name). Could substitute on output.
- **Cross-entry sharing visibility.** If `two := succ one` and both are bound, `:list` currently shows two rows — one for `h_one` (unpacked with body `succ 0`) and one for `h_two` (with body `succ one`). An alternative "tree view" could show the containment. Out of scope here.

### Resolver
- **Performance.** Each resolve inlines the full deep AST of each bound name. In a deeply-named chain this duplicates work before ingest hash-cons. Fine at this scale; worth measuring if it becomes a concern.
- **Stale resolution semantics.** When the user types `succ one` after rebinding `one`, resolution uses the *current* namespace (correct by design). If a user wants to re-resolve an older expression to pick up the new binding, they re-type or re-ingest; there is no substrate operation for "re-resolve stored definitions" (that would be "update all callers," which is explicitly an editor operation per `04-naming-layer.md:84`).

### Bootstrap / infrastructure
- **Test generators with names.** QCheck currently generates name-free `Ast.t` values (carried from p2). A generator that produces `Surface_ast.t` with occasional `Name` leaves and a companion-namespace would exercise the resolver more. Not critical; the deterministic tests already cover the important paths.

### Cross-prototype
- **Migration of learnings back to `docs/design/`.** If positions taken here hold up, the rename-is-derived stance, the dangling-display behavior, and the name/keyword collision handling are candidates to graduate from prototype-level to substrate-level decisions. That gating moment is not yet.
