# Decisions — p6-stlc

Prototype-specific decision log. ADR-lite. Append-only; reversals get new entries.

---

## 2026-04-23 — Fresh prototype, not an in-place edit of p5

A new prototype at `prototypes/p6-stlc/` starting from a copy of p5's tree, with arith stripped and stlc added as a sibling of lc. p5's scope doc frames it as "untyped arith + untyped lc," and its translator is total; editing p5 in place would blur what was multi-language-without-types versus multi-language-with-types. Disposable-prototype discipline (`CLAUDE.md` §Conventions).

**Alternatives considered.** Modify p5 in place (cheaper diff; rejected for the reason above). Carry arith into p6 (rejected — p5 already made the multi-language point; a third language here would dilute focus on the new substrate exercises without adding evidence).

---

## 2026-04-23 — `Definition.t = Lc | Stlc`; drop arith

p5's sum was `Arith | Lc`; p6's is `Lc | Stlc`. Arith is gone.

**Rationale.** p5's `Arith | Lc` sum and its language-guard machinery already demonstrated multi-language storage. The interesting new story in p6 is typed↔untyped, not three-way coexistence. Keeping arith would triple the test surface for no additional evidence.

**Alternatives considered.** `Arith | Lc | Stlc` — rejected. The scope doc for p5 would have been dishonest about what "Phase 4 answered"; better to let p5 stand as the evidence and make p6 a focused next step.

---

## 2026-04-23 — Language tag `'S'` for stlc; `'L'` for lc unchanged

Stlc nodes are encoded with a leading `'S'` byte (0x53), mirroring p5's `'L'` and `'A'` convention. Ty-only hashes (used only to key cache entries for `Lc_to_stlc_check`) use a leading `'T'` so they can't collide with definition hashes.

**Rationale.** Consistency with p5. Cheap visual invariant: grep the raw bytes of any p6 hash and see which language it belongs to.

---

## 2026-04-23 — Types are `Ty.t = Bool | Arrow(Ty.t, Ty.t)`, exactly TAPL Ch. 9

No type variables, no polymorphism. Every lambda surface-syntactically annotated `\x:T. body`.

**Rationale.** User preference for a literal reading of TAPL Ch. 8+9. The alternative (principal-type inference via type variables) would have been more expressive for Lc→Stlc translation but drifts from the chapter and adds α-equivalence-on-types complexity we don't need for this prototype's questions.

**Consequence.** `\x. x` in untyped lc has no principal type in p6's stlc — it has many possible types. The Lc→Stlc translator therefore takes a user-supplied expected type (see below). This forces the design to grapple with "translator with extra inputs," which is itself a valuable substrate exercise.

**Alternatives considered.** Include type variables (`TyVar(int)`) so that `\x. x` infers a canonical `'0 → '0` — rejected for the reason above. Multiple base types (`Nat`, `Unit`) — rejected as noise; Bool alone is enough to exercise the structure and to write interesting programs via `true`/`false`/`if`.

---

## 2026-04-23 — Store invariant: every stored stlc definition is well-typed

Type-checking happens in `Resolver.resolve_stlc` between surface→de-Bruijn and `Store.ingest_stlc`. An ill-typed input fails at resolution with `Resolver.Type_error(msg)`; no ill-typed stlc term ever enters the Store.

**Rationale.** Parallels p5's Store-level cross-language-reference check (`store.re:32–42`). "Typed values at substrate API boundaries" (feedback memory) extended to per-language validity. Makes the `stlc:type-check:v1` aspect trivially sound: its cached value is always the synthesized type of something the typechecker would already have accepted.

**Alternatives considered.** Allow ill-typed stlc in the Store and represent type errors as a negative aspect value (e.g. `Type_error(string)`) — rejected. Once you allow "maybe-typed" definitions, every downstream consumer (evaluator, translator, pretty-printer) has to branch on well-typedness, contradicting the simplicity the strong invariant buys. If a future prototype wants to store programs-with-errors (Hazel-style holes), it will revisit this.

---

## 2026-04-23 — Aspect-value additions: `Type_of(Ty.t)` and `Translation_untypable(string)`

`Attachment.aspect_value` gains two constructors in p6. The existing `Eval_*` and `Translation_target(Hash.t)` are unchanged.

- `Type_of(Ty.t)` — inline structured value (not hashed). Used by the `stlc:type-check:v1` aspect.
- `Translation_untypable(string)` — refusal marker with a human-readable cause. Used by `lc-to-stlc:check:v1[ty=…]` and reserved for any future partial translator.

**Rationale for inline Ty.** p6's types are small (most are ≤ 3 nodes), and inlining keeps the Attachment API unchanged. Hashing types would introduce a separate hash space and a second content-addressed registry to manage; unjustified at this scope.

**Rationale for `Translation_untypable`.** `docs/design/05-translation.md` acknowledges that translators may fail but doesn't prescribe how failures are represented. p6's answer: cache the refusal as a distinct aspect-value variant under the same (aspect, procedure) key a success would use. This way, `Attachment.peek` short-circuits both success and failure, and procedure-id bumps (to a `v2`) invalidate stale failures automatically.

**Alternatives considered.** Omit the aspect entry entirely on failure (no distinction between "never tried" and "tried and refused"; pay the re-inference cost on every replay) — rejected. A richer structured value (`Translation_result(result(Hash.t, string))`) — equivalent to the current design but requires unwrapping everywhere; the two-variants-at-the-same-level version is a small ergonomic win and leaves room for further failure-kind variants later.

---

## 2026-04-23 — Lc→Stlc cache keying via procedure-identity suffix

The `lc-to-stlc:check` translator takes an extra input — the expected type — beyond the source hash. The aspect store's key is `(target, aspect, procedure)` — a fixed triple. p6 encodes the expected type into the procedure identity:

```
procedure_id = "lc-to-stlc:check:v1[ty=" ++ Hash.hex_prefix(Ty.hash(Ty.canonicalize(ty)), 8) ++ "]"
```

Different expected types produce different procedure ids, giving distinct cache entries under the same aspect.

**Rationale.** Zero changes to Attachment's key shape. Works today. Procedure identity is already "whatever string uniquely identifies the computation"; folding the extra input in is a natural extension.

**Consequence / cost.** `Attachment.entries_for(procedure=…)` gives one procedure's entries, so listing all `lc→stlc` translations requires a prefix match on procedure ids. `Lc_to_stlc_check.all_entries` folds the store directly to implement this — slightly less tidy than `entries_for` but a self-contained shim.

**Alternatives considered.** Extend `Attachment.key` with an optional `extras: list(Hash.t)` field — rejected as premature; one partial-input translator doesn't justify an API shape change that every other aspect would need to route `None` through. Revisit if a second translator-with-inputs arrives in a later prototype.

---

## 2026-04-23 — Lc→Stlc algorithm: constraint-based check-mode with fresh metavariables

`Lc_to_stlc_check.check_at_type` walks the untyped term generating a fresh type-metavariable for every binder. At each `App(f, a)` it introduces a fresh metavariable α for the argument's type, checks `f` at `Arrow(α, expected)` and `a` at `α`. Unification uses standard union-find with an occurs check. A second pass ("zonk") concretizes each binder's metavariable; any unresolved metavariable at the end reports ambiguity and refuses the translation.

**Rationale.** Pure bidirectional check-mode gets stuck on `App` without synthesis on the function; the usual HM-style dual pass handles it. Since p6 has no type variables, every metavariable must ground to `Bool` or an `Arrow` built from it — the "ambiguous" failure only fires when the expected type is genuinely too weak to determine every binder (which is rare for the target shapes users will type).

**Error message format.** We reuse the metavariable ids in human-readable form (`'0`, `'1`, …) so the user can read unifier traces. They do not leak into the stored type — only into the `Translation_untypable(string)` cause field.

---

## 2026-04-23 — REPL `::` syntax for Lc→Stlc target type

`:translate <name|#pfx>` on a stlc source runs erase+Church. On a lc source, the command requires `::` followed by a type expression: `:translate foo :: Bool -> Bool`. Parsed by `Stlc_parser.main_ty`, reusing the stlc type grammar.

**Rationale.** `::` is the standard type-ascription token in most ML dialects. Reusing the stlc type parser means the grammar for "types at the REPL" and "types inside stlc terms" is the same by construction.

**Alternatives considered.** A separate `:translate-at <type> <name>` command — rejected for cluttering the command set. Making the target type a prompt-level default (`:lang stlc; :target-type Bool -> Bool`) — rejected because it would hide a significant input in state.
