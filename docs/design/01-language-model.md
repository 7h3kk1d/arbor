# Language Model — Options

**Status:** Options doc. No commitment yet. See `decisions.md` for the posture: composition model deferred, step in slowly, implement per-language and per-pair until friction justifies unification.

## What is a "language"?

A language in this system bundles:

- **Abstract syntax.** The set of well-formed AST shapes. Concrete syntax is an interface concern and not part of the core language identity.
- **Static semantics.** Type system, scope rules, well-formedness constraints. Fully absent for untyped languages.
- **Dynamic semantics.** Evaluation or reduction relation.
- **Primitives.** The set of atomic operations and values grounded outside the language itself.
- **Identity.** A stable handle we use to refer to the language from metadata (translations, name resolution, and as a property on each stored definition).

Every definition belongs to exactly one language. The language is a property of the definition, not a component of its hash; see `03-content-addressing.md`.

## Composition options

Four options, each leaves a distinct shape of friction when we push on it. None is pre-selected.

### Option A — Fixed ladder

Each named language is a discrete rung. New features produce a new rung. Translations run up the ladder from subset to superset.

- **Pros.** Simplest mental model. Matches TAPL chapter-by-chapter presentation. Every language stands alone and can be reasoned about in isolation.
- **Cons.** Linear order doesn't reflect orthogonal features. Adding "STLC with products but without sums" needs a new rung or an ad-hoc side branch. Many interesting languages in the TAPL space are *not* subsets of each other.
- **Canonical example.** The TAPL book itself, read as a sequence.

### Option B — Feature lattice

Features (lambda, let, refs, sums, products, records, subtyping, ∀, μ, …) are atomic and a language is a subset. Translations run between neighboring lattice points.

- **Pros.** Captures feature orthogonality. Makes explicit which combinations we've actually studied.
- **Cons.** Many feature pairs don't compose cleanly; the lattice includes many points we'd never want to materialize. Needs a framework to prevent nonsensical combinations. Combinatorial explosion of potential languages even when we only care about a handful.

### Option C — Shared core IR

Every feature elaborates to a single rich core. "Languages" are surface-level views that compile down. Translation between languages is re-elaboration through the core.

- **Pros.** Translation is near-automatic for subsets. One type checker, one evaluator. Lots of leverage.
- **Cons.** Requires designing the core before understanding the feature space — the big PLT research problem. If the core doesn't support some future semantics, adding it may require breaking the core. Much of the interesting structure lives in the elaboration rather than the languages themselves.
- **Canonical example.** GHC's Core, Racket's kernel language.

### Option D — Hybrid

A curated set of named languages (for clarity and pedagogy), internally backed by either feature flags or a core IR.

- **Pros.** Pragmatic. Lets us present a tidy lineup to users while staying flexible underneath.
- **Cons.** Inherits the tradeoffs of whichever underlying mechanism we choose. Can become worst-of-both if mismanaged.

## Language workbench landscape (notes)

Prior art, kept as reference rather than as a recommendation.

- **MPS (JetBrains).** Projectional editor over *concepts* (AST node kinds) with editor projections, type rules, generators. Languages extend each other by importing concepts. Avoids concrete-syntax ambiguity because editing is AST-level. Relevant to our structured-editor future.
- **Racket `#lang` / "languages as libraries."** Surface languages are macros over a shared kernel. Mixing happens at module boundaries. Practically very productive — close in spirit to Option C.
- **Spoofax / Rascal.** Declarative specs of syntax, name binding, type system, transformations. Language composition is a first-class concern but semantic interaction is still not solved in general.
- **PLT Redex / DynSem.** Reduction-semantics frameworks. Rules can be composed, but new semantic interactions still require manual resolution.
- **Data types à la carte / extensible ML / Wadler's expression problem.** Features as orthogonal algebraic components over an open AST. Clean algebra; doesn't address semantic interaction.
- **Attribute grammars / aspect-oriented PL design.** Features as aspects woven into a base grammar. Historically more about compiler construction than modular semantics.

## The central tension

Syntactic composition is tractable — grammars compose, ASTs union, concrete syntax can be disambiguated. **Semantic composition is hard**, especially for type systems. Canonical trouble pairs:

- **Subtyping × parametric polymorphism.** Resolved only by moving to bounded quantification (F-sub).
- **Let-polymorphism × mutable references.** Resolved only by the value restriction.
- **Laziness × side effects.** Resolved only by monadic I/O or similar.
- **Dependent types × almost anything.** Every addition has to re-prove soundness.

This tension is the core reason we're deferring composition and leaning on hand-written per-pair translators: even if we adopted a workbench-style composition framework, the hard problems of cross-feature semantics would still fall on us.

## Current posture

- **Defer choosing among A–D.** Live with per-language, per-pair machinery.
- **Revisit the composition question when friction is concrete:** we're duplicating translator fragments across pairs, or re-deriving the same type-checker logic across languages, or struggling to name a language that "clearly should exist."
- **A commitment criterion will be added to `decisions.md` when we can state one.**

## TAPL progression as our candidate ladder

Working list of languages we might instantiate, roughly in TAPL order. Not a commitment to Option A; just the pool we'll draw from.

- Untyped arithmetic expressions (Ch. 3–4)
- Untyped λ-calculus (Ch. 5)
- Simply typed λ-calculus / STLC (Ch. 8–9)
- STLC + simple extensions: unit, ascription, let, pairs, tuples, records, sums, variants, lists (Ch. 11)
- STLC + references (Ch. 13)
- STLC + exceptions (Ch. 14) — borders on effects; may defer
- STLC + subtyping (Ch. 15)
- F-sub / bounded quantification (Ch. 19) — optional
- Recursive types (Ch. 20)
- Type reconstruction / HM (Ch. 22)
- System F (Ch. 23)
- Existentials (Ch. 24)
- F-omega (Ch. 29) — optional
- Dependent types (Ch. 30) — likely out of scope for current phase

Which of these is the **first** language is a roadmap question, deferred to `09-roadmap.md`.

## Open sub-questions (feed to `open-questions.md`)

- What's our decision criterion for graduating from "per-language per-pair" to a composition framework?
- Is a "language" itself captured as data in the system (a language manifest) or as hand-written host-language modules? Big practical consequence for the tech stack.
- Does the choice of first language prejudice the eventual composition model? (E.g., starting with untyped λ is neutral; starting with STLC + features invites Option B thinking.)
