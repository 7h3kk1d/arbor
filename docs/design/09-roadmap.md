# Roadmap

**Status:** Rough phasing, not a committed plan. Phases 1–4 were built as p1 (arithmetic), p3 (naming), p4 (λ-calculus) and p5 (multi-language), with p2 (structural sharing) between the first two; the phase text below is the original scoping.

## Strategic posture

The substrate design in `docs/design/` is meant to endure across a series of prototype implementations. Each prototype is a disposable experiment that answers one or a few specific questions. We do not grow one long-lived implementation feature by feature; we build, learn, and often start over.

The design space has too many unknowns to commit to a single implementation trajectory. Cheap, focused prototypes surface real problems faster than evolving one codebase toward a moving target. A prototype that is wrong in a useful way is cheap; a long-lived codebase that drifts wrong is expensive to fix.

Decisions live in two places:

- **`docs/design/`** — substrate-level ideas, enduring across prototypes.
- **`docs/prototypes/<prototype-name>/`** — per-prototype decisions: tech stack, storage choices, schema specifics, API shapes, scope. Created when a prototype is scoped.

A prototype's decisions stay out of `docs/design/`. When a prototype teaches something that refines the substrate design, the learning migrates back into `docs/design/` as an update citing the prototype.

## Phase 1 — Minimal substrate, one language, no frills

**Language:** untyped arithmetic expressions from TAPL Chapter 3.

```
t ::= true | false | if t then t else t
    | 0 | succ t | pred t | iszero t
```

No binders, variables or functions. Values are booleans and natural numbers. The evaluation relation is small, and canonicalization is trivial because there is no α-equivalence.

**Questions.**

- Does our AST representation work in code?
- Does canonicalization plus hashing produce stable, deterministic identifiers?
- Does register / lookup / evaluate compose as a minimum workflow?

**Scope.** One language, untyped arithmetic. Store, Language and a minimal Interface. Full programs are registered as single self-contained definitions; cross-definition references are deferred.

**Out of scope.**

- Naming layer. Definitions are referred to by hash.
- Aspects: no evaluation cache, no type-check (arithmetic is untyped), no translation.
- Translation (one language only).
- Hole-awareness, collaboration, Hazel integration.
- Multi-definition programs with `Ref(hash)` references.

**Done when** you can register an arithmetic expression and get a hash, look it up and get the AST, and evaluate it and get a value, along with notes on what chafed and a clear next question.

**Interface options**, all simple:

- **A. Batch runner.** Expressions in a text file; the tool parses, registers and evaluates each and prints hashes and values. No interactivity. The simplest.
- **B. REPL.** A prompt accepts an expression, registers and evaluates it, and prints hash and value. State lives in memory for the session.
- **C. CLI with subcommands.** `register <expr>` → hash; `eval <hash>` → value. State persists across invocations in a local file, which forces the Store to actually persist; A and B can fake it in memory.

The original lean was C if the Store should be real from the start, A for the absolute minimum, B as a middle ground.

## Phase 2 — Naming + cross-definition references

Same language as Phase 1, plus a naming layer and cross-definition references.

**Questions.**

- Does the namespace (name → hash) feel right in practice?
- Does rendering names in place of referenced hashes improve readability?
- Does "no silent breakage" feel natural when names rebind?

**Scope.**

- `Ref(hash)` as a first-class AST construct, so a compound expression may reference other stored definitions.
- A minimal namespace: bind, rebind, unbind, lookup, reverse-lookup. One global namespace.
- A renderer that, at each `Ref(hash)`, prints the hash's name if it has one, and otherwise prints the hash or inlines the referenced subterm. Which fallback is a choice for the prototype.
- Interface extends Phase 1's with bind, show and render.

**Out of scope.** Aspects (no caching), translation, multiple languages, Hazel, holes, collaboration.

**Done when** you register `succ 0` and bind it to `one`, then register `succ (Ref one_hash)` as `two` and get a new hash whose body references `one`'s hash. Rendering `two` shows `succ one`, not `succ (succ 0)` or a hash. Rebinding `one` leaves `two`'s body unchanged, since it still references the old hash, which makes "no silent breakage" visible.

## Phase 3 — Untyped λ-calculus with naming and de Bruijn indices

A new language. It carries the naming layer from Phase 2 but is its own prototype; arithmetic is not required.

**Questions.**

- Does de Bruijn canonicalization produce stable hashes across α-renamings?
- Does the naming layer, which names definitions, coexist cleanly with bound variables, which are de Bruijn indices and unnamed? The two operate at different layers, and we want to confirm that in code.
- What is the evaluation story for untyped LC: β-reduction strategy, normal-form detection, non-termination?

**Scope.**

- Untyped λ-calculus (TAPL Ch. 5), with names in the surface syntax and de Bruijn indices internally.
- The hash is computed on the de Bruijn form, so `λx. x` and `λy. y` share a hash.
- The renderer converts de Bruijn back to named bound variables, picking fresh, readable names, and uses definition-level names for cross-definition `Ref`s.
- Naming layer and cross-definition references carry over from Phase 2.

**Out of scope.** Types, aspects, and interaction with any other language.

**Done when** `λx. x` and `λy. y` produce the same hash; the identity function bound to `id` and applied as `id 0` renders as `id 0`, where `0` is itself a stored LC term (a Church numeral or an explicit λ); β-reduction reaches normal forms for terminating terms; and the difference between definition-level names and bound variables is concrete.

## Phase 4 — Two languages + translation: untyped arithmetic ↔ untyped λ-calculus

The first prototype with two languages, and the first with aspects, used minimally to cache translations.

**Questions.**

- Does "no cross-language references; reuse via translation" survive two real languages?
- How painful is a hand-written translator? The natural candidate is arithmetic → LC by Church encoding: `true`/`false` as Church booleans, `0`/`succ`/`pred` as Church numerals, `iszero` derived.
- Does the eager-transitive-closure model in `05-translation.md` work?
- Does caching translations as an aspect feel right?

**Scope.**

- Untyped arithmetic and untyped λ-calculus (from Phase 3) in one Store.
- At least one translator, arithmetic → λ-calculus.
- The aspect store, introduced only far enough to record translation correspondences. Evaluation caching and type-check aspects stay out of scope.
- Names may point into either language's portion of the store.

**Out of scope.**

- Types.
- A reverse translator (LC → arithmetic), unless it proves interesting; Church-encoded LC has no obvious recovery as arithmetic.
- Multiple translators per pair.
- Hazel, holes, collaboration.

**Done when** translating a registered arithmetic expression stores an LC definition with a translation-aspect record pointing back to the source; re-translating reuses the cached result by hash lookup; translating a compound expression eagerly translates its sub-definition closure; and the substrate rejects any arithmetic expression that directly references an LC definition.

## Beyond Phase 4 — speculative

Real choices depend on what Phases 1–4 reveal. Candidates:

- Type systems: STLC and its extensions; type-check as a derived aspect.
- Evaluation caching as a derived aspect; procedure identity in practice.
- A hole-aware language, likely adapting Hazel's core calculus.
- Multi-modal interface experiments (structured editor, notebook, conversational).
- Collaborative editing via a Grove-style edit state.
- Proof-assisted or LLM-assisted translators.
- AI-assisted hole filling.
- Engagement with the propl24 computational-commons vision.
- Whatever an earlier prototype makes obvious.

## Non-goals

- A linear phasing, exact sequence or timeline.
- A tech stack; that is a per-prototype decision.
- Any expectation that a prototype survives.

## Open sub-questions

Tracked in `open-questions.md` under "Roadmap."

- **Phase 1 interface choice.** Batch runner, REPL, or stateful CLI.
- **Phase 2 rendering of referenced-but-unnamed hashes.** Fall back to the hash, or inline the subterm? Both are defensible.
- **Phase 3: keep or discard the Phase 1/2 arithmetic code?** Disposability says discard and keep the lessons. Revisit at the time.
- **Phase 4 reverse translator.** LC → arithmetic is probably not a useful retract; is the forward direction enough?
- **Throwaway discipline in practice.** Will we actually discard prototypes, or slip into evolving one?
- **When do learnings migrate back to `docs/design/`?** The implicit criterion is when a prototype changes the substrate design, cited as source. Make the habit explicit once prototypes exist.
