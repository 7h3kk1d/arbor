# Decisions — p4-lambda-calculus

Prototype-specific decision log. Lightweight ADR format. These were settled while planning and scaffolding.

---

## 2026-04-22 — Fresh prototype, not an in-place edit of p3

Binders, de Bruijn indices, and β-reduction are introduced in a new prototype at `prototypes/p4-lambda-calculus/`, inheriting p3's code as a starting point. p3's scope document explicitly lists binders and the λ-calculus language as out of scope; editing p3 in place would corrupt that claim as a historical artifact and blur the disposable-prototype discipline from `CLAUDE.md` and `docs/design/decisions.md`.

**Alternatives considered.** Modify p3 in place (cheaper to diff; rejected for the reason above). Consolidate two prototypes — keep arithmetic alongside λ-calculus and tackle translation in the same prototype (deferred; that's roadmap Phase 4's job, and coupling three new concerns is the opposite of the disposable-prototype posture).

---

## 2026-04-22 — De Bruijn indices internally; named surface syntax

Internal `Ast.t` is name-free and in de Bruijn form (`Var(int) | Lam(t) | App(t, t)`). Surface input keeps string names (`Var(string) | Lam(string, t) | App(t, t)`), eliminated at Resolver time. This is the substrate design position from `docs/design/09-roadmap.md:84-107` made concrete. α-equivalent surface terms canonicalize to identical `Ast.t` values, hence identical hashes after `Store.ingest`.

**Rationale.** Canonicalization-at-hash-time is the substrate's claimed answer to the α-equivalence-under-content-addressing question. De Bruijn is the standard way to make that answer operational; it is also what Unison (our closest reference implementation) chose. p4 exists to confirm the answer holds up in code.

**Alternatives considered.** ABTs á la Unison (more work, nothing to gain in a prototype without binding-structure operations beyond simple substitution). Nominal syntax with a separate α-canonicalization pass (extra module, same end state, no learning benefit here).

---

## 2026-04-22 — Continue inlining resolved names into subtrees; still no `Ref(hash)`

Resolver looks up a namespace-bound name, calls `Store.reconstruct` to produce a deep `Ast.t`, and inlines it. The stored DAG continues to use p3's shallow `Node.t` with `Hash.t` children; no `Ref(hash)` constructor is introduced. p3's rationale carries over: this prototype's thesis is binders and de Bruijn, not roadmap Phase 2's unfinished `Ref(hash)` scope.

Stored definitions are closed LC terms (they have no free variables), so inlining a closed subtree into any binder depth at the use site is safe — no de Bruijn index shifting needed. The closed invariant is a load-bearing detail of this decision.

**Consequence.** Re-using a deeply-named function term in an expression reconstructs and re-encodes the function's subtrees each time. Hash-cons still prevents duplication in the Store. A later prototype that introduces `Ref(hash)` amortizes the reconstruction cost; this is noted in `open-questions.md`.

---

## 2026-04-22 — Context-threaded resolver; innermost binder wins

`Resolver.resolve` threads a `list(string)` of enclosing binder names, most recent first. At each `Surface_ast.Var(name)`, lookup searches the context first; on hit, emits `Ast.Var(i)` where `i` is the position. On miss, the Namespace is consulted; on hit, the reconstructed stored subtree is inlined. On miss at both, `Unbound_name(name)` is reported.

**Rationale.** Classical lexical scoping. Shadowing is the usual rule programmers expect — `\x. x` inside a scope that also has `x` bound in the namespace must bind the inner `x`. Embedding this as a resolver invariant (rather than a special case elsewhere) keeps the core claim legible: bound variables and definition-level names are orthogonal mechanisms at orthogonal layers.

**Alternatives considered.** Sigil-prefixed definition names (`@id` for namespace, bare `id` for bound) — rejected because the substrate has no opinion on name structure and sigils would leak a one-prototype convention into the language. Warning on shadowing — rejected as noisy UX for a feature that's intuitive.

---

## 2026-04-22 — CBV, weak head reduction, 10000-step default budget

Evaluation is call-by-value (reduce argument before β) and stops at weak head normal form (a `Lam` at the head). β is TAPL's de-Bruijn formulation: `(λ.t) v → ↑_{-1}^0 ([0 ↦ ↑_1^0(v)] t)`. Each β counts as one step; the budget default is 10000. `StepLimit` results are not cached because they are partial — a future call with a larger budget might produce a `Value`.

**Rationale.** CBV matches OCaml's own semantics, so the evaluator code reads naturally to someone reading Reason. WHNF is the standard LC interpreter shape; normalizing under binders (NF) adds scope juggling without pedagogical value in a prototype. Substitution-based β over de Bruijn is shorter and more legible than environment-and-closures.

**Consequence.** The Y combinator's fixpoint production diverges under CBV (standard undergraduate result). Evaluating `Y f` hits the step limit. This is informative, not a bug.

**Alternatives considered.** Call-by-name (preserves Y-fixpoint terminations; rejected because it's also a divergence factory, and CBV is the more faithful match to how programmers think about ordinary code). Environment+closures (faster, but WHNF + CBV is fast enough at prototype scale). Full β-normal form (requires renaming-under-binders machinery that de Bruijn doesn't need; not pedagogical for this prototype).

---

## 2026-04-22 — Fresh display names at render time; deterministic per term

The name-aware printer picks binder names from a fixed alphabet (`x, y, z, a, b, …, w`) with a numeric suffix when the alphabet is exhausted in a given scope (`x1, y1, …`). It always picks the first element not already in scope. Same inputs produce the same output, even though the names are not persisted anywhere.

**Rationale.** Roadmap Phase 3 explicitly says "picking fresh, readable names" for the renderer (`docs/design/09-roadmap.md:98`). Determinism preserves testability and avoids surprising the user with per-call name churn. Starting with `x, y, z` matches conventional LC notation.

**Consequence.** The name a user *types* into the REPL is not preserved through a round trip. Binding `:bind id \foo. foo` followed by `:list` will show `\x. x`, not `\foo. foo`. Surprising on first encounter, but consistent with the "binders have no names in the stored form" design claim.

**Alternatives considered.** Preserve user-supplied binder names as an asserted aspect — rejected as speculative infrastructure; names are a display choice and an aspect for them is unmotivated until we see a concrete problem. Random names per render — rejected as destroying testability and user orientation.

---

## 2026-04-22 — Child subterms collapse to namespace names only when non-Var

The pretty-printer collapses a child hash to `Var(first-alphabetical-name)` when the child has a namespace binding AND the stored node is `Lam` or `App`. A `Var(k)` child never collapses to a namespace name, even if the variable's hash is coincidentally bound.

**Rationale.** Collapsing a `Var` child to a namespace name is semantically confusing: a variable reference's meaning is context-dependent (which binder), while a namespace name's meaning is closed-term. Collapsing them together loses legibility and enables confusing renderings. Collapsing `Lam` / `App` children preserves p3's structural-sharing-legibility win (named subterms read as names).

**Alternatives considered.** Collapse uniformly per p3's rule (simpler, but the rare Var-bound-to-a-name case produces unreadable output). Never collapse (rejected: loses the whole point of the name-aware printer).

---

## 2026-04-22 — `StepLimit` evaluation results are excluded from the cache

The `Eval` module only writes `Value` and `Stuck` results to the Attachment store. `StepLimit` is returned but never attached. The cache disposition is `Derived`; if `StepLimit` were attached, a later call with a higher budget would read the cached `StepLimit` and never re-evaluate, pinning the term as divergent forever.

**Rationale.** `StepLimit` is a partial observation — "the evaluator did not converge under this budget." A bigger budget might. Treating it as a derived aspect (immutable for a given procedure) would be semantically wrong because the procedure identity (`lc:eval:v1`) does not include the step budget. Versioning the procedure by budget (`lc:eval:v1:steps=10000`) would bloat the aspect store with budget-specific caches.

**Alternatives considered.** Cache with a separate `Asserted` disposition (mutable; rejected as a categorical mismatch — this is a derived computation, not an asserted fact). Include the step budget in the procedure identity (bloats the store and makes cache hits rare).

---

## 2026-04-22 — `Node.Var(int)` encodes the index as 8 big-endian bytes

Variable indices in `Node.encode` are serialized as 8 bytes (64-bit big-endian) rather than as a variable-width encoding. Tag bytes are single characters (`\x01`, `\x02`, `\x03`).

**Rationale.** Future-proofs against large indices without dividing implementation effort between two encoding schemes. Fixed-width encoding also keeps the encoding pipeline simpler and easier to audit for hash stability across OCaml platforms. The extra 7 bytes per Var node is negligible at prototype scale.

**Alternatives considered.** Variable-length integer (saves bytes; rejected for complexity). Machine-word encoding (`Int64.of_int` directly to little-endian native bytes; rejected because little-endian is platform-dependent in principle, and big-endian is the canonical serialization convention).

---

## 2026-04-22 — Namespace's reserved-keyword list is drained, not removed

`Namespace.reserved_names` is now `[]`. The surface grammar for untyped λ-calculus has no keywords, so there is nothing to reserve at the language level. The reservation *mechanism* (the `is_reserved_name` helper, the `Name_reserved` exception, the checks in `bind` / `rebind` / `rename`) is kept intact so that a REPL host (or a later language extension) can repopulate the list without API churn.

**Rationale.** The design doc treats names as opaque strings and puts no constraints on their content (`04-naming-layer.md:24`). Empty-list preserves the invariant for this language while leaving the shape available for re-use.

**Alternatives considered.** Remove the mechanism entirely — rejected to avoid API churn across prototypes. Pre-populate with REPL command words (`:bind`, `:list`, etc.) — rejected because the command prefix `:` already excludes those from being valid identifiers; reserving them twice would be redundant.

---

## 2026-04-22 — Shared opam switch during bringup; dedicated switch at the end

p4 initially uses a symlink at `prototypes/p4-lambda-calculus/_opam → ../p3-naming-layer/_opam` to share p3's local switch. This matches p3's own pattern (see p3 `decisions.md` 2026-04-22 "Dedicated local opam switch") — bringup leans on an existing switch; a dedicated switch materializes once the prototype stabilizes and its dependencies are known to match p3 exactly.

**Rationale.** Cold-starting a full OCaml + Menhir + Reason + ppx_deriving + digestif + alcotest + qcheck switch takes several minutes. Symlinking is instant and lets the build loop iterate at full speed while the scaffold is in flux. Once the prototype is stable, a dedicated switch is a recorded decision for reproducibility.

**Consequence.** Moving or deleting p3's switch while p4 uses the symlink breaks p4's build. Materializing a dedicated switch for p4 is tracked in `open-questions.md` under "Bootstrap / infrastructure."

---

## 2026-04-22 — Namespace-collapse in the renderer requires a closed subterm

The name-aware printer collapses a child hash to `Var(first-alphabetical-name)` only when the subterm has a namespace binding AND is closed (no free de Bruijn indices) AND is `Lam` or `App`. If a namespace name points at an open subterm (rare but possible via `:bind-hash <name> #<open-subterm-prefix>`), the renderer expands the subterm inline instead.

**Rationale.** A namespace name in a rendering reads as "this is a self-contained definition." Applying that reading to an open subterm is misleading — the term only makes sense relative to its surrounding binders. Expanding inline preserves readability of the containing term; the name (which still exists in the namespace) remains accessible via `:name-of <hash>` and `:list` for the open-subterm row. The closedness check is linear in the reconstructed subtree size; acceptable at prototype scale. See `open-questions.md` if it needs caching.

**Alternatives considered.** Allow open collapse (simple rule; rejected for the misleading-reading reason). Strip namespace bindings for open hashes at bind time (rejected — users may deliberately name open subterms for debugging; silently dropping the binding is surprising).

---

## 2026-04-22 — `:list closed` filters to closed-term rows

`:list` enumerates every stored node, including intermediate open subterms produced by `Store.ingest` walking a deep AST bottom-up. `:list closed` is a new variant that filters the rows to hashes that reconstruct to closed terms. Gives a definition-level view of the Store.

**Rationale.** After a few bindings and a β-reduction or two, `:list` produces many rows that are internal plumbing (`\x. $1`, `$1`, etc.) alongside the handful of rows a user actually cares about. `:list closed` is the "what have I defined?" view; `:list` remains the "what is in the Store?" view.

**Alternatives considered.** Only store top-level ingested terms (rejected — would break structural sharing). Use name-presence as the filter instead of closedness (rejected — a bound name on an open subterm is possible and legitimate for debugging, so such rows should still appear under the "everything" view).

---

## 2026-04-22 — REPL command set mirrors p3, plus `:step-limit`

All p3 REPL commands (`:bind`, `:rebind`, `:unbind`, `:rename`, `:names`, `:name-of`, `:hash-of`, `:list`, `:list raw`, `:dag`, `:lookup`, `:show`, `:eval`, `:stats`, `:load`, `:help`, `:quit`) are carried with lexicon retuned. One new command: `:step-limit [n]` queries or sets the current β-reduction budget. A `--step-limit N` CLI flag sets the initial value.

**Rationale.** Command consistency across prototypes minimizes cognitive overhead. `:step-limit` is the one concept genuinely new at the interface layer — non-termination is an untyped-LC concern that did not arise for arithmetic.

**Alternatives considered.** An infinitely-running `:eval` with an interrupt — rejected because SIGINT handling in an OCaml REPL is a rabbit hole unrelated to the prototype's thesis. A global flag rather than a REPL command — rejected because users naturally want to experiment with budgets inside one session.
