# Roadmap

**Status:** Rough phasing. Explicitly not a committed plan — a map, not a march.

## Strategic posture

The substrate design in `docs/design/` is intended to endure across **a series of prototype implementations**. Prototypes are disposable experiments, each with one or a few specific questions to answer. We do not commit to one long-lived implementation that grows feature-by-feature; we build, learn, and often start over.

**Why.** The design space has too many unknowns to commit to a single implementation trajectory. Cheap, focused prototypes surface real problems faster than incremental evolution of a codebase toward a moving target. A prototype that's "wrong" in the right way is useful; a long-lived codebase that gets slowly wrong is expensive to fix.

**Where decisions live.**

- **`docs/design/`** — substrate-level ideas. Endures across prototypes.
- **`docs/prototypes/<prototype-name>/`** — per-prototype decisions: tech stack, storage choices, schema specifics, API shapes, scope. Created when a prototype actually scopes up; doesn't exist yet.

A prototype's decisions stay out of `docs/design/`. If a prototype teaches us something that refines the substrate design, that learning migrates back into `docs/design/` as an update, with the prototype cited as the source.

## Phase 1 — Minimal substrate, one language, no frills

**Language: untyped arithmetic expressions** from TAPL Chapter 3.

```
t ::= true | false | if t then t else t
    | 0 | succ t | pred t | iszero t
```

No binders, no variables, no functions. Values are booleans and natural numbers. The evaluation relation is small; canonicalization is trivial (no α-equivalence to worry about).

**Questions Phase 1 should answer.**

- Does our AST representation work in code?
- Does canonicalization + hashing produce stable, deterministic identifiers?
- Does register / lookup / evaluate compose as a minimum workflow?

**Scope.**

- One language: untyped arithmetic.
- Store + Language + absolutely minimal Interface.
- Full programs registered as single definitions. Cross-definition references are deferred — everything is a self-contained term.

**Out of scope (explicitly).**

- Naming layer. Definitions are referred to by hash, full stop.
- Aspects. No evaluation cache, no type-check (arithmetic is untyped), no translation.
- Translation (one language only).
- Hole-awareness, collaboration, Hazel integration.
- Multi-definition programs with `Ref(hash)` references.

**What "done" looks like.** You can register an arithmetic expression and get a hash; look it up and get the AST; evaluate it and get a value. That's it. You have working code, honest opinions about what chafed, and a clear next-most-interesting question.

**Interface ideas (pick one; all three are simple).**

- **A. Batch runner.** Put expressions in a text file. Tool parses, registers each, evaluates each, prints hashes and values. One pass, no interactivity. Simplest of the three.
- **B. REPL.** Prompt accepts an expression; tool registers it, evaluates it, prints hash + value. State persists in memory for the session. Slightly more "feel" than the batch runner.
- **C. CLI with subcommands.** `register <expr>` → hash; `eval <hash>` → value. State persists across invocations in a local file. Forces the Store to actually persist; the other two can fake it with in-memory state.

My lean: **C** if we want the Store to be meaningfully real from the start; **A** if we want the absolute minimum experiment. **B** is a middle ground. Your call.

## Phase 2 — Naming + cross-definition references

Same language as Phase 1 (untyped arithmetic). Add a naming layer and cross-definition references.

**Questions.**

- Does the namespace (name → hash) feel right in practice?
- Does the "renderer substitutes names for referenced hashes" affordance actually improve readability?
- Does the substrate's "no silent breakage" property feel natural when names rebind?

**Scope.**

- Add `Ref(hash)` as a first-class AST construct inside arithmetic expressions. A compound expression may contain references to other stored definitions.
- Add a minimal namespace: bind, rebind, unbind, lookup, reverse-lookup. One global namespace.
- Add a **renderer** that walks an expression and, whenever it encounters a `Ref(hash)`, consults the namespace: if that hash has a name, print the name; otherwise fall back to printing the hash (or inlining the referenced subterm — a rendering choice worth deciding during the prototype).
- Interface extends Phase 1's with bind/show/render operations.

**Out of scope.**

- Aspects (no caching of anything).
- Translation, multiple languages.
- Anything Hazel, hole, or collaborative.

**What "done" looks like.** You register `succ 0`, bind it to the name `one`. You register `succ (Ref one_hash)` — call it `two` — and get a new hash whose body references the hash of `one`. When you render `two`, you see `succ one`, not `succ (succ 0)` and not `succ <hash-soup>`. Rebind `one` to some other hash and observe that `two`'s body is unchanged (it still references the old hash of `one`) — the "no silent breakage" property is now visible in a real prototype.

## Phase 3 — Untyped λ-calculus with naming and de Bruijn indices

New language. Carries the naming layer from Phase 2 but starts fresh as its own prototype — arithmetic is not required.

**Questions.**

- Does canonicalization via de Bruijn indices produce stable hashes across α-renamings?
- Does the naming layer (which names *definitions*) coexist cleanly with bound variables inside expressions (which are *not* named — they're de Bruijn indices)? These are different things operating at different layers and we want to confirm that in code.
- What does the evaluation story look like for untyped LC (β-reduction strategy, normal-form detection, non-termination)?

**Scope.**

- Language: untyped λ-calculus (TAPL Ch. 5). Syntax with names in the surface form, de Bruijn indices internally.
- Canonical form: de Bruijn. The hash is computed on the de Bruijn form, so `λx. x` and `λy. y` share a hash.
- Renderer: converts de Bruijn back to named bound variables for display (picking fresh, readable names). Respects definition-level names for cross-definition `Ref`s.
- Naming layer and cross-definition references carry over from Phase 2.

**Out of scope.**

- Types.
- Aspects.
- Interaction with any other language (arithmetic from earlier phases does not appear here).

**What "done" looks like.** `λx. x` and `λy. y` produce the same hash. You can bind the identity function to a name `id`, write `id 0` (where `0` is itself a stored LC term — maybe a Church numeral if we lean into it, or just an explicit λ), and see it render as `id 0`. β-reduction produces normal forms for terminating terms. The difference between "definition-level names" and "bound-variable-inside-a-term" is concrete and legible.

## Phase 4 — Two languages + translation: untyped arithmetic ↔ untyped λ-calculus

First prototype with two languages coexisting. First prototype with aspects, minimally — specifically the translation aspect for caching.

**Questions.**

- Does "no cross-language references; reuse via translation" survive first contact with two real languages?
- How painful is writing the hand-written translator? (Natural candidate: arithmetic → LC via Church encoding — `true`/`false` as Church booleans, `0`/`succ`/`pred` as Church numerals, `iszero` derivable.)
- Does the eager-transitive-closure model work as described in `05-translation.md`?
- Does the translation-aspect caching story feel right?

**Scope.**

- Two languages in one Store: untyped arithmetic and untyped λ-calculus (from Phase 3).
- At least one translator: untyped arithmetic → untyped λ-calculus.
- The aspect store, introduced **minimally** — just enough to record translation correspondences. Other aspect categories (evaluation caching, type-check) are still out of scope.
- Naming layer handles names pointing into either language's portion of the store.

**Out of scope.**

- Types.
- Reverse-direction translator (LC → arithmetic) unless it's interesting; Church-encoded LC can't obviously be recovered as arithmetic.
- Multiple translators per pair.
- Hazel, holes, collaboration.

**What "done" looks like.** You register an arithmetic expression, translate it to LC, and the resulting LC definition is stored with a translation-aspect record pointing back to the arithmetic source. Re-translating the same arithmetic source reuses the cached LC result via hash lookup. Translating a compound arithmetic expression eagerly translates its sub-definition closure. The `Ref(hash)` constraint is visibly enforced: you cannot write an arithmetic expression that directly references an LC definition, and the substrate rejects any attempt to do so.

## Beyond Phase 4 — speculative

Deliberately vague; real choices are made by what Phases 1–4 reveal:

- Type systems: STLC and its extensions; type-check as a derived aspect.
- Evaluation caching as a derived aspect; procedure identity in practice.
- Integrate a hole-aware language (likely adapting Hazel's core calculus).
- Multi-modal interface experiments (structured editor, notebook, conversational).
- Collaborative editing via a Grove-style edit state.
- Proof-assisted or LLM-assisted translators.
- AI-assisted hole filling.
- Engagement with the propl24 computational-commons vision.
- Whatever a prior prototype made obvious that we haven't thought of yet.

## Non-goals (this roadmap specifically)

- Committing to a linear phasing, an exact sequence, or a timeline.
- Pinning a tech stack here; that's a per-prototype decision.
- Pretending any prototype will survive. Each is a candidate; each is expendable.

## Open sub-questions

Tracked in `open-questions.md` under "Roadmap."

- **Phase 1 interface choice.** Batch runner, REPL, or stateful CLI.
- **Phase 2 rendering behavior on referenced-but-unnamed hashes.** Fall back to hash, or inline the referenced subterm? Both are defensible.
- **Phase 3: keep or discard Phase 1/2's arithmetic code?** Spirit of disposable prototypes says discard; practically we may want to keep lessons (not code). Worth revisiting when we get there.
- **Phase 4 reverse translator.** Is LC → arithmetic interesting (probably not — not a useful retract) or do we only do the forward direction?
- **Throwaway discipline in practice.** Are we actually willing to discard prototypes, or will we slip into evolving one? Worth noticing if it happens.
- **When do learnings migrate back to `docs/design/`?** Implicit criterion: when a prototype reveals something that changes the substrate design, update the design docs with the prototype as source. Worth making this habit explicit once prototypes exist.
