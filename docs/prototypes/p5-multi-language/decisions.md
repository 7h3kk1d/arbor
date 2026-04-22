# Decisions — p5-multi-language

Prototype-specific decision log. ADR-lite. Append-only; reversals get new entries.

---

## 2026-04-22 — Fresh prototype, not an in-place edit of p4

A new prototype at `prototypes/p5-multi-language/` inheriting both p3 and p4 source as starting points, rather than edit-in-place on p4. p4's scope doc says "Multiple languages in one Store or translation between them (roadmap Phase 4)" is explicitly out of scope. Editing p4 in place would corrupt that as a historical claim and blur the disposable-prototype discipline.

**Alternatives considered.** Modify p4 in place (cheaper diff; rejected for the reason above). Split into two prototypes — one for multi-language storage, another for translation — rejected because the translation phase is what makes multi-language storage non-trivial (otherwise you'd never need to check language compatibility in the Resolver or the Store).

---

## 2026-04-22 — `Definition.t` becomes a sum

`Definition.t = Arith(Arith_node.t) | Lc(Lc_node.t)`. Was an alias for `Node.t` in p3 and p4. This is the concrete realization of the "multi-language Store" claim from `docs/design/00-overview.md` — `Store` holds the sum; pattern-matching fans out per-language operations.

**Rationale.** Carries typed values at the Store's boundary (the `feedback_typed_boundaries` memory notes this). Adding a third language is one new variant plus per-language modules, with no change to Store / Attachment / Namespace shapes.

**Alternatives considered.** A GADT parameterized by a language-tag phantom type — powerful but overkill for two languages and would complicate Attachment's `aspect_value` which doesn't currently care about languages. One hash-table per language — rejected because then cross-language lookup becomes two lookups, and Attachment keys would need to know which language's table to consult.

---

## 2026-04-22 — One-byte language-tag prefix in every Node encoding

`Arith_node.encode` prepends byte `'A'` (0x41); `Lc_node.encode` prepends `'L'` (0x4C). Every hash computation runs through the prefixed encoding.

**Rationale.** Without the prefix, arith's `True` tag `\x01` and lc's `Var(0)` tag `\x01` + 8 zero bytes are structurally similar byte sequences. Different-length inputs produce different BLAKE2B digests anyway, but the *explicit* language tag makes the "no cross-language references" invariant (`03-content-addressing.md`) visible at the lowest level — one byte, one grep away. Also makes future generalizations (e.g., language IDs as registered strings) cheap.

**Consequence.** p3 and p4 hashes would NOT match p5 hashes even for the same stored term. This is fine because p5 is a fresh prototype; the design discipline is that prototypes don't share persistent state.

**Alternatives considered.** No tag byte (smaller hashes by one byte; rejected for the explicitness argument above). Use a registered-language-ID byte — equivalent in spirit, deferred until a third language arrives.

---

## 2026-04-22 — "No cross-language references" enforced at Store register time

`Store.register_arith_node` and `Store.register_lc_node` check every child hash: if the child exists in the Store and its `Definition.language` differs from the parent's, raise `Language_mismatch`. Child hashes not yet in the Store pass (the usual case during `ingest_*`, which registers bottom-up).

**Rationale.** The invariant from `docs/design/03-content-addressing.md` and `05-translation.md`: Store-level definitions do not reference across languages. Translators are the only bridge. Enforcing at register time catches manual misuse (e.g., constructing an `Arith_node.Succ(h_lc)` in a test), which a type-level check would also catch — but `Hash.t` is just a string, so type-level doesn't get us there without adding a phantom language parameter.

**Alternatives considered.** No runtime check, rely on the type system alone — insufficient (`Hash.t` is `string`). Check at ingest but not at register — rejected because `register_*_node` is also the entry point for evaluators producing value hashes, and those should also fail loudly on a mismatched child.

---

## 2026-04-22 — Resolver gains a language-guard error

`Resolver.resolve_arith` and `Resolver.resolve_lc` both check `Store.language_of(store, h)` before inlining a namespace-bound hash. A mismatch produces `Error(Language_mismatch(name, expected, actual))`.

**Rationale.** The "no cross-language references" rule ought to be visible to the user at the edit-time boundary, not just at Store registration. A user who writes `succ id` in arith mode (with `id` bound to an lc term) sees a clear "id is bound to an lc definition; expected arith here" rather than an internal exception from the Store. The error's `name / expected / actual` structure matches the three pieces of context the user needs to fix it.

**Alternatives considered.** Allow cross-language inlining with implicit translation — rejected because (a) implicit translation is explicitly against `05-translation.md:§Running a translator is an explicit action`, and (b) choosing *which* translator to call implicitly is unacceptable ambiguity even with a single translator registered today.

---

## 2026-04-22 — `Attachment.aspect_value` grows `Translation_target(Hash.t)`

New variant added to the existing `aspect_value` sum. No other Attachment API changes.

**Rationale.** Translations fit the existing aspect-value shape naturally: a source hash gets a derived entry whose value references a target hash. The shape already existed for `Eval_value(Hash.t)` — this is the same pattern, different meaning. No new descriptor field, no new Attachment API — one sum variant.

**Alternatives considered.** Split the Attachment module into per-aspect tables — rejected as speculative infrastructure. Use a fresh `translations` Hashtbl keyed by `(aspect_id, procedure_id, source_hash)` — equivalent on the surface but duplicates Attachment's descriptor / disposition / dispatch machinery.

---

## 2026-04-22 — Translator identity: `arith-to-lc-church:translate:v1`

The Church-encoding translator's procedure identity is exactly this string. Tag describes language pair and elaboration strategy; `translate` names the operation; `:v1` is the manually-bumped version.

**Rationale.** Matches the naming pattern from `docs/design/02-definitions-and-derived-data.md` and from the existing `arith:eval:v1` / `lc:eval:v1` identities. Including the strategy (`church`) in the tag leaves room for a peer `arith-to-lc-scott:translate:v1` later without a rename.

**Alternatives considered.** Hash-address the translator (Unison-style) — correct future direction, deferred per the substrate's "tag + version for now" stance. Omit the strategy and let the version differentiate (`arith-to-lc:translate:v1` and `:v2`) — rejected because different encodings aren't versions of a single strategy; they're distinct translators that should coexist.

---

## 2026-04-22 — Eager transitive closure is trivially satisfied — noted in open-questions for `Ref(hash)`

The design doc (`05-translation.md:§Transitive dependencies`) commits to eager closure: translating D_L1 translates every L1 definition it transitively depends on. In p5, p3/p4's inline-at-resolution stance means stored arith definitions are already closed deep trees with no cross-definition references. So a single-definition walk is enough; no dependency-closure recursion is needed.

**Consequence.** If a later prototype introduces `Ref(hash)` as a first-class AST node, the translator will have to walk refs and translate transitively. Today it doesn't. Tracked in `open-questions.md`.

**Alternatives considered.** Implement the closure walk speculatively — rejected; no ref nodes exist to test it against, and unused closure-walk code rots.

---

## 2026-04-22 — Mode-based language parsing in the REPL, not per-expression

REPL holds a `current_lang: ref(string)` toggled by `:lang arith` / `:lang lc`. Bare expressions parse in the current language. `:bind`, `:rebind`, `:register-only`, `:eval-expr` all use the current language to parse their expression argument.

**Rationale.** Minimizes typing. Matches the user's natural workflow ("I'm working on arith for now, then I'll switch to lc"). The prompt includes the current language (`(arith) > ` vs `(lc) > `) so mode is legible.

**Alternatives considered.** Per-expression language prefix (`:arith succ 0` vs `:lc \x. x`) — rejected as verbose for common case. Language heuristics (try arith first, fall back to lc) — rejected as unprincipled and hard to explain when it fails. Separate REPL sessions per language — rejected; translation needs both languages in the same Store.

---

## 2026-04-22 — Translation commands are `:translate` (invoke) and `:translations` (view)

`:translate <name|#hash>` runs the translator on an arith source; prints the translator-identity-tagged `src -[id]-> tgt` line, with a `cached` prefix on a hit or `translated` otherwise. `:translations` with no argument lists every cached pair (sorted by source hash); with an argument, prints the one for that source or "(no cached translation for ...)".

**Rationale.** Invoking-versus-viewing is the natural split. The identity tag is printed in the output so the user can see which translator's cache they're reading. The cached/translated distinction makes the "derived aspect" story legible.

**Alternatives considered.** A single `:translate` that prints current state on no-arg — rejected; invoking a translation should be a deliberate action, not a side effect of listing. Name the commands `:compile` / `:compiled` — rejected; the substrate vocabulary says translators produce definitions, not compile targets.

---

## 2026-04-22 — Viewing commands prepend language tags

`:list`, `:list raw`, `:list closed`, `:dag`, `:names`, `:lookup`, `:show` all prepend `[arith]` / `[lc]   ` (fixed 7-char width). `:stats` shows `definitions: N (arith M, lc K)` and a `translations: T` line.

**Rationale.** The user asked for it: "Please have the viewing commands display the languages the expression is for." Fixed-width keeps rows aligned. The tag goes on the line, not replacing anything — existing format (hash + names + body) is preserved.

**Alternatives considered.** Color the rows per language — rejected; not every terminal supports color, and the test harness parses plain text. Separate `:list-arith` / `:list-lc` — rejected; the user wants a unified view with language as metadata, not two half-views.

---

## 2026-04-22 — Tests use a deep β-normalizer to compare translator outputs

Translator correctness is a β-normal-form property, not a weak-head property. The lc evaluator is CBV-WHNF (carried from p4), which stops under binders. `(λn. λs. λz. s (n s z)) c_zero` WHNF-reduces to `λs. λz. s (c_zero s z)` — a Lam, hence a value — not to `λs. λz. s z` (= Church one).

So the test harness has a `deep_normalize : Lc_ast.t → Lc_ast.t` that reduces under binders (normal-order, strongly-normalizing inputs only). Tests compare `deep_normalize(translate_output)` against the canonical Church form.

**Rationale.** The prototype's translator is correct; the WHNF evaluator is correct; they disagree on what "equal" means for β-equivalent terms. Adding a normal-order evaluator to the runtime just so tests can compare would be over-engineering — a test-only helper is the right scope.

**Consequence.** `Lc_eval.eval` users still get WHNF in the REPL (`\x. ...` appears unreduced), which is the p4 behavior. A future prototype might add a `:normalize` REPL command if the UX demands it.

**Alternatives considered.** Compare via behavior (apply to test witnesses and compare results) — works but verbose and harder to state. Compare hashes after running the evaluator (breaks because of the WHNF/NF mismatch above). Add a `Lc_deep_eval` module — rejected as speculative infrastructure for a test concern.
