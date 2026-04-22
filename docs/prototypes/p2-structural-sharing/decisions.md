# Decisions — p2-structural-sharing

Prototype-specific decision log. Lightweight ADR format. These were settled in the plan before scaffolding began.

---

## 2026-04-22 — Shallow `Node.t` stored; ingest walks the deep AST bottom-up

The Store holds shallow nodes (one constructor + children-as-hashes), not full subtrees. An `ingest` primitive walks the parser's deep `Ast.t` bottom-up, registering each subterm and returning the top hash.

**Rationale.** This is how Merkle-tree content addressing actually looks; structural sharing falls out automatically because equal subterms produce equal hashes. Also tests whether the DAG storage model composes cleanly with the rest of the substrate's primitives.

**Alternatives considered.** Hash-consing inside a deep AST (same API as p1, with interning behind the scenes) — cheaper to retrofit but doesn't exercise the DAG in the Store, which is the whole point.

---

## 2026-04-22 — Evaluator returns `Hash.t`; intermediate values are registered in the Store

`eval : Store.t → Attachment.t → Hash.t → eval_result` where `eval_result = Value(Hash.t) | Stuck(Hash.t)`. When evaluation produces a value that isn't already in the Store (e.g., the `succ 0` that emerges from `pred (succ (succ 0))`), the evaluator registers it and returns the new hash.

**Rationale.** Keeps the substrate invariant that every term is hash-identified. Makes cross-expression cache hits observable end-to-end: a later `ingest(succ 0)` lands on the same hash that the earlier eval wrote, and a subsequent eval is a cache hit.

**Alternatives considered.** Returning a separate `value` sum type that is never stored — simpler, but breaks the "everything is a hash" invariant and prevents the natural cross-expression cache-hit demo.

**Consequence.** Evaluation mutates the Store. Documented as expected; Store registration is still idempotent, so this is benign.

---

## 2026-04-22 — Introduce the full Attachment layer, minimally populated

The Attachment module implements the API sketched in `docs/design/06-architecture.md:34-42`: descriptors (aspect-id, disposition, applicable languages), `(target, aspect, procedure) → value` entries, and bidirectional queries. Only one descriptor is registered in this prototype — the eval cache — but the API shape is the substrate-faithful one.

**Rationale.** The prototype's thesis is dual: structural sharing *and* the aspect-store API shape. Introducing the full API up front lets the first real aspect stress-test the design, and leaves less to invent later. The extra code (~100-150 lines) is bounded.

**Alternatives considered.** A plain `Hashtbl(Hash.t, eval_result)` inside the Store module — fewer moving parts, tighter focus on structural sharing alone. Rejected because deferring the aspect shape means we'd learn less about whether the design holds up.

---

## 2026-04-22 — Procedure identity as tag+version string

Procedure identity for the eval cache is the literal string `"arith:eval:v1"`. Bumping the version invalidates all eval-cache entries produced by the old evaluator.

**Rationale.** Matches the near-term path in `docs/design/02-definitions-and-derived-data.md:67-71`. Content-addressed procedure identity is deferred per the design doc.

**Alternatives considered.** Opaque procedure handles (a `type procedure_id = private ...`). Stringly-typed is simpler and equivalent at this scale; the cache key is opaque to Attachment either way.

---

## 2026-04-22 — Stuck terms are cached

`Eval.Stuck(h)` results are attached to the eval-cache aspect just like `Eval.Value(h)` results. A second evaluation of a stuck expression hits the cache.

**Rationale.** Stuckness is a deterministic function of (term, evaluator version). Caching it costs nothing and keeps cache semantics uniform — "eval was run, here's the result" — without a special case for failures.

---

## 2026-04-22 — Hash scheme differs from p1

p2 hashes a node as `digest(tag_byte ++ child_digests...)` where `child_digests` are raw bytes. p1 hashes as `digest(tag_byte ++ encoded_children...)` with children encoded inline. Atoms (`True`, `False`, `Zero`) hash identically in both; non-atoms diverge.

**Rationale.** The two schemes encode different storage models. p1's hash has to carry the full tree's content; p2's hash only needs to carry child identities, because the children are themselves content-addressed. Trying to make the hashes coincide would force one scheme to impersonate the other and lose information.

**Consequence.** A p1 hash for `succ (succ 0)` is not a p2 hash for the same term. Cross-prototype hash comparison is not a goal.
