# p13 — Decisions (ADR-lite)

Append-only. Substrate commitments that generalize migrate to `docs/design/`.

---

### 2026-06-05 — Fork p12 verbatim; add predicative System-F

**Decision.** p13 = p12's substrate (opaque types, seals, editing-context
opacity, evaluator, tests aspect, REPL, web) renamed to `P13_substrate`, plus
predicative System-F: `TVar` / `Forall` types, `TyLam` / `TyApp` terms, explicit
type application, type-erased evaluation.

**Why.** Functorization (code generic over the carrier) is the next step the user
wants, and it genuinely requires abstracting over a type — which p12 has none of.
System-F is the minimal, standard mechanism. It composes with p12's opaque types:
instantiate a type variable at an opaque type; the functor body stays parametric.

---

### 2026-06-05 — Functor argument = type arg + ops bundle (no first-class modules)

**Decision.** The requested `(C : { type t; incr; decr }, C.t, b) -> C.t`
is realized as `forall t. ((t->t)*(t->t)) -> t -> Bool -> t`: a type argument plus
an ops `Product`. No first-class module value, no path-dependent `C.t`.

**Why.** That decomposition is the standard System-F encoding of an ML functor
argument and is far smaller than first-class modules / `∃` / path-dependent types
(the hardest corner of `12`). The bundled-module-as-a-value form is the `∃`
complement and stays deferred.

---

### 2026-06-05 — Explicit, predicative, type-erased

**Decision.** No type inference (type application is written `e [T]`).
Predicative (no `∀` instantiation at a `∀` type required for the driving program).
Type abstraction/application erase at runtime (`TyLam e → e`, `TyApp(f,_) → f`).

**Why.** Matches the explicit surface style; keeps the checker and evaluator
small. de Bruijn type variables make `∀` α-equivalence fall out of content
addressing, exactly as for `λ`.
