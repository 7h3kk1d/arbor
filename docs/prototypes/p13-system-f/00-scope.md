# Phase 13 — System-F over abstract types (functorization)

**Status:** Scope doc for the thirteenth prototype.
**Design context:** `../../design/12-type-abstraction.md` (the `∀` / functor shape; §"The two quantifiers"), `../../design/01-language-model.md` (F-omega region), `../../design/03-content-addressing.md` (hashing types; de Bruijn canonicalization), `../../design/06-architecture.md`.
**Forks:** `p12-abstract-types` (verbatim, renamed `P13_substrate`).
**Prototype design lives here:** `docs/prototypes/p13-system-f/`.
**Implementation code lives at:** `prototypes/p13-system-f/`.

## Thesis

p12 gave *type abstraction* — opaque types with hidden representations, editor-enforced opacity, the unsealing set. It cannot write code that is generic over the carrier: each abstract type (`Counter.t`, `Tally.t`, …) is its own type, and a consumer is tied to one of them. p13 adds the smallest thing that fixes this — **predicative System-F polymorphism** — so you can write a program once and apply it to a counter with *any* internal representation. That is *functorization*, the `∀` shape from `12`.

The driving program (a functor that steps a counter up or down):

```
step : forall t. ((t -> t) * (t -> t)) -> t -> Bool -> t
step = /\t. \ops: (t -> t) * (t -> t). \x: t. \b: Bool.
         if b then (fst ops) x else (snd ops) x

step [Counter.t] (Counter.incr, Counter.decr) Counter.empty true   -- : Counter.t
step [Tally.t]   (Tally.incr,   Tally.decr)   Tally.start    false  -- : Tally.t   (rep = Int * Int)
```

The originally-requested form `(C : { type t; incr; decr }, C.t, b) -> C.t` decomposes, in System-F, into a **type argument** `t` plus a **value bundle** of the ops (a `Product` — the closest p13 has to a record). Applying the functor to a Counter is `step [Counter.t] (Counter.incr, Counter.decr) …` — exactly the standard System-F encoding of an ML functor argument.

**Why this is enough — and where it stops.** Inside `step`, `t` is a bound type variable, so the body is *parametric*: it cannot inspect the representation. Abstraction here comes from **quantification**, separate from (and composing with) p12's **sealing** (which hides the rep behind whatever opaque type you instantiate `t` with). What System-F does **not** give: a value that hides *its own* type (the existential `∃` / first-class module — factories that choose a representation, heterogeneous collections of counters, storing a module in a value). Those, and higher-kinded abstraction (F-omega, generic over a type *constructor*), are explicitly deferred — see Out of scope and `12`.

## What it answers

1. Does explicit, predicative System-F (`/\`, `e [T]`, `forall`) sit cleanly on the content-addressed, de Bruijn substrate — i.e., does type-variable α-equivalence fall out the same way term α-equivalence does?
2. Does it compose with p12's opaque types: instantiate `t := Counter.t` (an opaque type), pass the sealed ops, and have the functor stay parametric (never unseal)?
3. Is type-erased evaluation (`/\`/`[T]` vanish at runtime) the whole runtime story?
4. Does one `step` genuinely apply to counters with *different* representations (Int vs Int×Int)?

## Substrate additions (deltas from p12)

- **Types (`Tnode`)** gain `TVar(int)` — a de Bruijn type variable (counts `Forall` binders outward) — and `Forall(Hash.t)` — `∀`, whose body is a type with one more type var in scope. New encoding tags `0x15` (TVar), `0x16` (Forall). de Bruijn ⇒ α-equivalence of `∀` is automatic and content-addressed, exactly like terms.
- **Terms (`Node`)** gain `TyLam(t)` — type abstraction `Λ` — and `TyApp(t, Hash.t)` — type application. New tags `0x0d`, `0x0e`.
- **Typecheck** gains: `shift_ty` / `subst_ty` (capture-avoiding de Bruijn substitution over content-addressed types); `synth(TyLam e) = Forall(synth e)`; `synth(TyApp f A)` requires `f : Forall(body)` and returns `subst_ty(0, A, body)`; `equal_ty` recurses through `Forall`; `whnf` leaves `TVar`/`Forall` rigid (only open opaques unfold, unchanged). `TVar` is rigid — a functor body cannot unseal it, which *is* parametricity.
- **Eval** type-erases: `TyLam e → e`, `TyApp(f, _) → f`.
- **Surface + parser:** `/\t. e` (type-lambda, token `/\`), `e [T]` (type application, `[`/`]`), `forall t. T` (`∀` type, keyword `forall`). A type variable is written as a bare name; the resolver maps a name bound by an enclosing `forall`/`/\` to a `TVar` (de Bruijn index from the type-var context) and otherwise falls through to the namespace.
- **Pretty** renders `Forall`/`TVar` with generated type-var binder names (parallel to term binders).

No inference: type application is explicit, matching the explicit style of the surface program.

## In scope

- Predicative System-F: `TVar`, `Forall`, `TyLam`, `TyApp`; explicit type application; type-erased eval.
- Composition with p12's opaque types: instantiate a type variable at an opaque type; functor stays parametric.
- Surface syntax + resolver threading a type-var context alongside the term context.
- Test: build `step`, apply it to two counters with different representations (Int, Int×Int); assert the result types and the evaluated values; assert the body never needs to unseal (parametricity).

## Out of scope

- **Existentials / first-class modules (`∃`, `{ type t; … }` as a value).** Factories that choose a representation, heterogeneous collections, storing/returning a module. The honest home for the original `C : { type t; … }` notation; deferred (`12`).
- **Higher-kinded abstraction (F-omega).** Abstracting over a type *constructor* (a generic `map`/`Monad`).
- **Type inference.** Type application is explicit.
- **Polymorphic sealing** — sealing an abstract type *under* a type variable (`/\t. struct …`). Witnesses stay closed (ground) in p13.
- **Bounded/row polymorphism, subtyping.** Inherited-deferred from `12`/`11`.

## Relationship to design docs

- `12` §"The two quantifiers" — p13 is the `∀` (functor) half made real; the `∃` half (first-class modules) is its complement, deferred.
- `01` — the F-omega region; p13 takes the System-F slice (no type operators).
- `03` — types as content-addressed defs; de Bruijn type variables canonicalize `∀` the way de Bruijn term variables canonicalize `λ`.
