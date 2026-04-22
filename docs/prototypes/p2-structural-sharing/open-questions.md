# Open Questions — p2-structural-sharing

Prototype-specific open items. Will grow as implementation surfaces them.

## Attachment API shape

- **Where do aspect descriptors live?** For now, registered on an `Attachment.t` instance at construction time. A global registry is tempting but premature — this prototype only has one descriptor.
- **Derived-entry re-attach semantics.** A derived entry is immutable for a given `(target, aspect, procedure)` triple. If `attach` is called twice with different values, the plan is to make the second call a no-op. An alternative is to raise — worth deciding once the eval cache surfaces a real scenario where this might happen (it shouldn't, since eval is deterministic).
- **Bidirectional index cost.** The `by_value` query currently means "scan entries and filter." If the cache grows, a reverse index keyed on `(aspect, procedure, value)` will pay for itself. Deferred until the cost is visible.
- **`stats` payload.** Right now just `{entries: int, hits: int, misses: int}`. What else would be useful? Per-aspect breakdowns if we ever have more than one aspect.

## Evaluator

- **Recursion vs iteration.** Deeply nested terms (e.g., `succ (succ (succ ... 0))`) evaluate via natural OCaml recursion. Fine at prototype scale; if we hit stack depth limits with pathological qcheck inputs, convert to explicit stack.
- **Order of eval-cache consultation.** Current plan: check the cache on entry for the input hash, and recurse on child hashes (which also check cache). This means the cache is consulted both top-down (quick exit if the whole term has been seen) and bottom-up (via the child recursion). Worth confirming the interaction has no redundant work once it's running.
- **Should we attach the hash of the *value* as well, or only the `(target, aspect)` mapping?** A value like `succ 0` is itself a term in the Store. Whether the evaluator *also* attaches `Eval.Value(h) → h` (self-evaluation) on value nodes is an optimization question: it short-circuits re-evaluating known values. Probably yes; revisit if it feels over-eager.

## Store / ingest

- **`ingest` idempotence on a single subtree.** Ingesting the same AST twice should register exactly the same set of node hashes with no duplicates. Verified in tests; mentioned here because any change to the hashing scheme needs to preserve it.
- **Reconstruct vs lazy rendering.** `reconstruct` eagerly builds a deep `Ast.t` before pretty-printing. For small programs this is fine. If a prototype later renders large, deeply shared DAGs, streaming from hashes directly through the pretty-printer (without materializing the full tree) would avoid duplicating shared subterms in the output. Not needed here.
- **Hash-based equality in the Store is stringly-typed.** `Hash.t = string` carries no invariants beyond "produced by `Hash.of_node`." Tightening to `private string` (or a `Hash.t = Bytes.t` wrapper) is on the table if downstream consumers accidentally compare hashes to unrelated strings.

## Interface details

- **`(cached)` marker granularity.** Plan: mark only the top-level cache hit. Subterm hits are summarized by `:stats`. If users want per-subterm visibility during eval, `:trace` would be the right command — not in this prototype, maybe next.
- **`:list` display with a DAG.** Listing every node in insertion order prints every subterm, including atoms like `Zero`. That's verbose but honest. Options: filter to "top-level ingested" terms, or group by referring-term. Deferred; the verbose view is probably most informative for now.
- **`:lookup <prefix>` on a subterm hash.** Already works — subterms have hashes too, and the DAG reconstructor turns them into a displayable AST. Nice consequence of uniform hash-addressing.

## Cross-prototype

- **Can p1 and p2 share the hash scheme retroactively?** No (they chose different encodings on purpose; see `decisions.md`). Would be a substrate-level choice, not a prototype one. If we ever standardize a hash scheme in `docs/design/`, that's a design-level change, not an edit to either prototype.
