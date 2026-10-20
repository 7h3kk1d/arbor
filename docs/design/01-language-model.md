# Language Model — Options

**Status:** Options doc. No commitment yet. `decisions.md` records the posture: the composition model is deferred, and languages and translators are implemented per-language and per-pair until friction justifies unification.

## What is a "language"?

A language here bundles:

- **Abstract syntax.** The well-formed AST shapes. Concrete syntax is an interface concern and not part of the language's identity.
- **Static semantics.** Type system, scope rules, well-formedness constraints. Absent for untyped languages.
- **Dynamic semantics.** An evaluation or reduction relation.
- **Primitives.** Atomic operations and values grounded outside the language.
- **Identity.** A stable handle for referring to the language from metadata (translations, name resolution) and as a property of each stored definition.

Every definition belongs to exactly one language. The language is a property of the definition, not a component of its hash; see `03-content-addressing.md`.

## Composition options

Four options, each with a distinct shape of friction. None is pre-selected.

### Option A — Fixed ladder

Each named language is a discrete rung, and new features produce a new rung. Translations run up the ladder from subset to superset.

- **Pros.** Simplest mental model. Matches TAPL's chapter-by-chapter presentation. Each language can be reasoned about in isolation.
- **Cons.** A linear order does not reflect orthogonal features: "STLC with products but without sums" needs a new rung or an ad-hoc side branch. Many interesting languages in the TAPL space are not subsets of each other.
- **Canonical example.** The TAPL book, read as a sequence.

### Option B — Feature lattice

Features (lambda, let, refs, sums, products, records, subtyping, ∀, μ, …) are atomic, and a language is a subset of them. Translations run between neighboring lattice points.

- **Pros.** Captures feature orthogonality and makes explicit which combinations have been studied.
- **Cons.** Many feature pairs do not compose cleanly, so the lattice contains many points nobody would materialize, and a framework is needed to rule out nonsensical combinations. The space of potential languages explodes even when only a handful matter.

### Option C — Shared core IR

Every feature elaborates to one rich core. Languages are surface views that compile down, and translating between them is re-elaboration through the core.

- **Pros.** Translation is near-automatic for subsets. One type checker, one evaluator.
- **Cons.** The core must be designed before the feature space is understood, which is the big PLT research problem. A future semantics the core cannot express may force breaking it. Much of the interesting structure ends up in the elaboration instead of the languages.
- **Canonical example.** GHC's Core, Racket's kernel language.

### Option D — Hybrid

A curated set of named languages, for clarity and pedagogy, backed internally by feature flags or a core IR.

- **Pros.** Pragmatic: a tidy lineup for users, flexibility underneath.
- **Cons.** Inherits the tradeoffs of whichever mechanism backs it, and can become the worst of both if mismanaged.

## Language workbench landscape (notes)

Prior art, kept as reference, not recommendation.

- **MPS (JetBrains).** Projectional editor over *concepts* (AST node types) with editor projections, type rules, and generators. Languages extend each other by importing concepts. Editing at the AST level avoids concrete-syntax ambiguity. Relevant to a future structured editor.
- **Racket `#lang` / "languages as libraries."** Surface languages are macros over a shared kernel, mixed at module boundaries. Very productive in practice and close in spirit to Option C.
- **Spoofax / Rascal.** Declarative specs of syntax, name binding, type system, and transformations. Composition is a first-class concern, but semantic interaction is still unsolved in general.
- **PLT Redex / DynSem.** Reduction-semantics frameworks. Rules compose, but new semantic interactions still need manual resolution.
- **Data types à la carte / extensible ML / Wadler's expression problem.** Features as orthogonal algebraic components over an open AST. Clean algebra that does not address semantic interaction.
- **Attribute grammars / aspect-oriented PL design.** Features woven into a base grammar. Historically about compiler construction more than modular semantics.

## The central tension

Syntactic composition is tractable: grammars compose, ASTs union, and concrete syntax can be disambiguated. Semantic composition is hard, especially for type systems. Canonical trouble pairs:

- **Subtyping × parametric polymorphism.** Resolved only by bounded quantification (F-sub).
- **Let-polymorphism × mutable references.** Resolved only by the value restriction.
- **Laziness × side effects.** Resolved only by monadic I/O or similar.
- **Dependent types × almost anything.** Every addition must re-prove soundness.

This is why composition is deferred in favor of hand-written per-pair translators. A workbench-style composition framework would still leave the hard cross-feature semantics to us.

## Current posture

- **Defer choosing among A–D** and live with per-language, per-pair machinery.
- **Revisit when friction is concrete:** translator fragments duplicated across pairs, type-checker logic re-derived across languages, or a language that clearly should exist but is hard to name.
- **Add a commitment criterion to `decisions.md`** once one can be stated.

## TAPL progression as our candidate ladder

The pool of languages we might instantiate, roughly in TAPL order. Listing them in order is not a commitment to Option A.

- Untyped arithmetic expressions (Ch. 3–4)
- Untyped λ-calculus (Ch. 5)
- Simply typed λ-calculus / STLC (Ch. 8–9)
- STLC + simple extensions: unit, ascription, let, pairs, tuples, records, sums, variants, lists (Ch. 11)
- STLC + references (Ch. 13)
- STLC + exceptions (Ch. 14). Borders on effects; may defer.
- STLC + subtyping (Ch. 15)
- F-sub / bounded quantification (Ch. 19). Optional.
- Recursive types (Ch. 20)
- Type reconstruction / HM (Ch. 22)
- System F (Ch. 23)
- Existentials (Ch. 24)
- F-omega (Ch. 29). Optional.
- Dependent types (Ch. 30). Likely out of scope for the current phase.

Which comes first is a roadmap question, deferred to `09-roadmap.md`.

## Open sub-questions (feed to `open-questions.md`)

- What criterion decides graduating from per-language, per-pair machinery to a composition framework?
- Is a language captured as data in the system (a language manifest) or as hand-written host-language modules? This has large consequences for the tech stack.
- Does the choice of first language prejudice the composition model? Starting with untyped λ is neutral; starting with STLC plus features invites Option B thinking.
