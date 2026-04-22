# Open Questions — p4-lambda-calculus

Prototype-specific open items. Distinguishes "positions taken here (see `decisions.md`)" from "intentionally deferred."

## Positions this prototype takes

These are active stances that resolve previously open sub-questions. Each decision is filed in `decisions.md`; summarized here for cross-reference.

- **De Bruijn canonicalization** (`docs/design/03-content-addressing.md`). Internal AST is de Bruijn; hash is computed on that form; α-equivalence collapses at hashing time. The canonicalization-via-de-Bruijn claim is now operational.
- **Evaluation strategy** (`docs/design/09-roadmap.md:92`). Call-by-value, weak head normal form, bounded by a step budget. Non-termination produces `StepLimit` rather than blowing the stack.
- **Step-budget caching** (new for p4). `StepLimit` results are not cached; only `Value` and `Stuck` are.
- **Resolver shadowing rule** (new for p4). Innermost binder wins; namespace names fall through only for unshadowed identifiers. Validates the design claim that definition names and bound variables are orthogonal.
- **Display-name strategy** (`docs/design/09-roadmap.md:98`). Fresh, deterministic names at render time from a fixed alphabet (`x, y, z, a, …`); user-supplied binder names are not preserved through a round trip.
- **`Ref(hash)`** (`docs/prototypes/p3-naming-layer/open-questions.md:19`). Still deferred — p4 continues p3's inlining-at-resolution approach. Closed-term inlining needs no index shifting.

## Still open — intentionally deferred

### Language-layer

- **`Ref(hash)` as a first-class AST constructor.** p3 and p4 both inline resolved subtrees before ingest. Introducing `Ref(hash)` as an AST node (with a hashing scheme that treats references as indirection, not inlined content) is most of the rest of roadmap Phase 2. A later prototype will take this on. It becomes more attractive as function definitions grow, because p4's approach re-encodes the function body each time it is used via a name.
- **Normal-order reduction and full β-normalization.** p4 only reduces at the head; inside binders stays unreduced. A future prototype comparing CBV-WHNF with normal-order-NF would make the trade-offs concrete. Until then, expressions like `\x. (\y. y) (\y. y)` display as-stored (the inner redex unreduced).
- **η-equivalence.** `\x. f x` and `f` are β-equivalent (η-related) but produce different de Bruijn terms and hence different hashes. Collapsing η at hashing time would be an extension of canonicalization; unclear whether desirable for every language. A language decision, not a substrate one.
- **Named holes / hole-aware evaluation.** Future Hazel integration territory. Not this prototype.
- **Cross-definition references within a λ-term.** When the user types `(\x. x) id`, the resolver inlines `id`'s body into the stored expression. A `Ref(hash)` model would leave a reference instead. Today this is only a size concern, not a correctness one; moves to "language-layer" above when a prototype revisits it.
- **Recursive let / mutual recursion.** There is no primitive for defining a recursive function; the Y combinator is provided in the bootstrap script as a demo, and diverges under CBV. Mutual recursion canonicalization is flagged in the substrate-level `open-questions.md`; this prototype is not a forcing function.

### Evaluator

- **Step-budget-aware caching.** The current stance is "don't cache `StepLimit` at all." A richer scheme would cache "at least N steps without convergence" and accept further work on re-call. Over-engineered for this prototype. Note if it comes up in practice.
- **Step accounting granularity.** Every β counts one step. Sub-evaluations share the budget. Alternative: separate budgets per sub-evaluation, or a fuel-based model where shift/subst operations consume fuel proportional to their argument size. Fine as-is for Omega-style demonstrations.
- **Observability of the evaluator.** `:eval` returns a single-shot answer. A `:step` command that shows one β at a time (and prints the new state) would teach the semantics; absent here.
- **Stuck terms on closed inputs.** In untyped LC on closed input, a `Stuck` result only arises from applying a `Var(k)` to something, which shouldn't happen if resolution and β are correct. The `Stuck` variant is kept for totality. Worth instrumenting if it triggers in practice.

### Resolver

- **Performance.** Each resolution of a deeply-named chain reconstructs full subtrees. Fine at prototype scale. Measure if it starts mattering.
- **Stale resolution semantics.** p3's behavior carries over: rebinding a name does not re-resolve any previously stored expressions. User-level re-resolution is a text re-entry, not a substrate operation. This remains correct for p4.
- **Identifier collisions with hash prefixes.** Bare hex strings of ≥ 4 characters are interpreted as hash prefixes by `:bind`, `:lookup`, etc. The LC grammar has no restriction on identifier content beyond starting with `[a-zA-Z_]`, so names like `deadbeef` are valid identifiers but would trip the prefix heuristic when used as an argument to hash-accepting commands. Same call-to-ambiguity as p3; worth tightening if it bites.

### Pretty-printer

- **Binder-name stability across related terms.** `\f. \x. f x` displays as `\x. \y. x y`. After β-reducing one β in some surrounding expression, the body might display with different names. This is expected (names are a rendering artifact) but potentially confusing when comparing two renderings by eye. Tooling could mark matching structures; out of scope.
- **Free-variable display.** The renderer prints `$k` when it encounters a de Bruijn index out of scope. This happens for intermediate stored subterms (e.g., the body of a `Lam` rendered on its own, which contains a free index at depth 1). `$` is not a valid identifier character in the lexer, so `$k` cannot round-trip back through the parser — fine for a display-only sentinel, since closed top-level terms never produce one.
- **Fresh-name collision with namespace names.** A binder display name may coincide with a namespace-bound name (e.g., fresh-gen picks `x`, but the namespace also binds `x`). The renderer will not confuse the two at *render* time (binder positions and child-hash collapsing are separate code paths), but a *reader* might misread. Avoiding namespace-bound names in the fresh-gen alphabet is a quick mitigation not yet implemented.

### Namespace

- **Aliases as a first-class concept.** `const`, `konst`, `tru` all resolve to the same hash in the bootstrap script — visible as `[const, konst, tru]` in `:list`. Is this alias density legible, or would users benefit from distinguishing "intentional alias" from "accidental α-collision"? Not forced here; revisit with a larger corpus.
- **Multi-namespace + branching** (`docs/design/open-questions.md:55`). Still out of scope per roadmap Phase 3's scope, same as p3.

### Cross-prototype

- **Migration of learnings back to `docs/design/`.** Candidate graduates from p4:
  - The fresh-names-per-render strategy for languages with binders (could become a substrate convention).
  - The "no cross-collapse: Var children never become namespace names" rule (could migrate to the naming-layer doc as guidance).
  - The "StepLimit-like partial results are never cached" pattern (could generalize to other non-terminating procedures).
  None are forced yet; migration is the discipline from the design doc's non-goals ("learnings migrate back when they refine the substrate design"), not a cleanup task.

### Bootstrap / infrastructure

- **Dedicated opam switch.** The prototype currently symlinks p3's switch. Materializing a dedicated switch (copy or fresh `opam switch create`) is the end-state position per the decision log; the symlink exists only for bringup. Tracked as a to-do so a future engineer knows the symlink is not a design choice.
- **QCheck generator shape.** The closed-LC generator uses depth + size parameters. Works, but biased toward certain shapes (Lam-heavy at shallow depth). A shrinker that preserves closedness would sharpen failure messages if / when a property test fails. Not critical today.
- **Test input diversity.** Parser tests exercise specific syntax forms; the property tests exercise structure. Neither covers "programs with many definitions used together." A richer end-to-end test using the bootstrap script plus assertions on hash-sharing patterns would catch integration regressions. Manual today.
