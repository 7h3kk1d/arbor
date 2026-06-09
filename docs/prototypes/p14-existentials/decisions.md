# p14 — Decisions (ADR-lite)

Append-only. Substrate commitments that generalize migrate to `docs/design/`.

---

### 2026-06-05 — Fork p13; add closed existentials (pack/unpack)

**Decision.** p14 = p13's substrate (System-F over abstract types) renamed
`P14_substrate`, plus existentials: an `Exists` type and `Pack`/`Unpack` terms,
reusing p13's `TVar` / `shift_ty` / `subst_ty` machinery. `unpack` is closed
(`unpack [t] x = e in body`); the body's result type may not mention the unpacked
`t` (the avoidance problem is *rejected*, not solved).

**Why.** `∃` is the complement to p13's `∀` and the honest home for the
first-class-module / factory programs the System-F decomposition couldn't write.
The unpacked `t` is just a fresh rigid type variable, so `∃` is a small delta on
p13 rather than new machinery.

---

### 2026-06-05 — Interface = positional Product (no records)

**Decision.** An existential package bundles its operations as a positional
`Product` (`t * ((t->t) * (t->Int))`), not a record. Projection is `fst`/`snd`.

**Why.** Records (label-records, `11`) are a separate addition. Product is what
p13 already used to bundle a functor's ops; reusing it keeps p14 to one new idea
(`∃`). Named-field first-class modules stay future work.

---

### 2026-06-05 — Type-erased; explicit witness; closed unpack

**Decision.** `pack` carries its witness type and target `∃` type explicitly (no
inference). At runtime `pack` forgets the witness (eval its body) and `unpack` is
a `let`-binding. `unpack` is closed-scope; no open-existential / path-dependent
`m.t`.

**Why.** Matches p13's explicit, type-erased style; keeps the checker and
evaluator small. Open existentials (and clever avoidance) are deferred.
