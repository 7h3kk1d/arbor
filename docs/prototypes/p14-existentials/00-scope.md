# Phase 14 — Existentials / first-class modules

**Status:** Scope doc for the fourteenth prototype.
**Design context:** `../../design/12-type-abstraction.md` (the `∃` / existential-package shape; §"The two quantifiers"), `../../design/01-language-model.md` (existentials = TAPL Ch. 24).
**Forks:** `p13-system-f` (verbatim, renamed `P14_substrate`).
**Prototype design lives here:** `docs/prototypes/p14-existentials/`.
**Implementation code lives at:** `prototypes/p14-existentials/`.

## Thesis

p13 added `∀` (functors): a *consumer* generic over the carrier, with the type chosen at the call site. p14 adds the complement, `∃` (existentials): a *value that hides its own representation*, with the type chosen by the *producer* and unknown to the consumer until `unpack`. This is the `12` existential-package shape, and the first-class-module value the System-F decomposition couldn't express — factories that pick a representation, heterogeneous collections, modules returned/stored as values.

The driving program is the factory `∀`-System-F couldn't write — `mkCounter` returns a counter implementation whose representation it chooses, hidden behind `∃`:

```
mkCounter : Bool -> exists t. t * ((t -> t) * (t -> Int))
mkCounter = \fast: Bool.
  if fast then pack [Int]      (0,     (\x:Int. x+1, \x:Int. x))                          as exists t. t * ((t -> t) * (t -> Int))
          else pack [Int*Int]  ((0,0), (\p:Int*Int. (fst p+1, snd p), \p:Int*Int. fst p)) as exists t. t * ((t -> t) * (t -> Int))

unpack [t] c = mkCounter true in
  snd (snd c) ((fst (snd c)) ((fst (snd c)) (fst c)))   -- get (incr (incr empty)) : Int
```

The two `if` branches pack *different* witnesses (`Int`, `Int*Int`) yet unify at the single `∃` type; the consumer `unpack`s and uses the ops without learning the representation. An interface is bundled as a positional `Product` (no records yet, as in p13): `t * ((t->t) * (t->Int))` = `(empty, (incr, get))`.

**Abstraction source.** Inside `unpack`, `t` is a fresh rigid type variable — abstract by *being a variable*, exactly like p13's `∀`-bound vars and reusing the same `TVar`/shift/subst machinery. So `∃` is a small delta on p13, not a new mechanism.

## What it answers

1. Does `∃`/pack/unpack sit on p13's type-variable substrate with just an `Exists` type plus two term forms?
2. Can two `pack`s of different witnesses unify at one `∃` type (the `if`-factory)?
3. Does `unpack` keep the witness abstract (the body cannot inspect it), and is the **avoidance problem** handled — the body's result type may not mention the unpacked `t`?
4. Type-erased eval: pack forgets the witness; unpack is a `let`-binding at runtime.

## Substrate additions (deltas from p13)

- **Types (`Tnode`)** gain `Exists(Hash.t)` — `∃`, body has one more type var in scope (de Bruijn, like `Forall`). New tag `0x17`.
- **Terms (`Node`)** gain `Pack{ witness: Hash.t; body: t; ty: Hash.t }` (the hidden witness, the packed value, the target `∃` type) and `Unpack(t, t)` (scrutinee; body, which binds a fresh type var and a value var). Tags `0x0f`, `0x10`.
- **Typecheck.**
  - `synth(Pack{witness, body, ty})`: `ty` must be `Exists(eb)`; check `body : subst_ty(0, witness, eb)`; result `ty` (witness forgotten).
  - `synth(Unpack(scrut, body))`: `scrut : Exists(eb)`; enter a fresh type var (shift the term ctx by 1), push `x : eb`; synth `body : r`; **avoidance check** — `TVar(0)` must not occur free in `r` (else the witness escapes); result is `r` *strengthened* (free vars ≥ 1 shifted down).
  - `equal_ty` recurses through `Exists`; `shift_ty`/`subst_ty`/`normalize_type` handle `Exists` (cutoff +1 under the binder); new `occurs`/`strengthen` helpers.
- **Eval** type-erases: `Pack{body} → body`; `Unpack(scrut, body) → let v = scrut in body` (bind the value; the type vanishes).
- **Surface + parser:** `exists t. T`; `pack [W] e as E`; `unpack [t] x = e in body`. Keywords `pack`/`unpack`/`as`/`exists`; the resolver threads the type-var context (unpack pushes `t` into `tvs` and `x` into the term ctx for the body).
- **Pretty** renders `exists`, `pack`, `unpack`.

## In scope

- `∃` type; `pack`/`unpack`; the `if`-factory unifying two witnesses; unpack-and-use; the avoidance check.
- Test: build `mkCounter` (Int vs Int×Int branches), unpack both, compute an Int; assert the result and that an escaping result type is rejected.

## Out of scope

- **Records with named fields.** An interface is a positional `Product`; `{ empty; incr; get }`-as-a-value with projection-by-name is future work.
- **Open existentials / path-dependent `m.t`** (Montagu–Rémy and the avoidance problem in general). Closed `unpack … in …` only; a result type that mentions the witness is rejected, not avoided cleverly.
- **Higher-kinded** abstraction (F-omega).
- **Type inference** — pack carries its witness and target type explicitly.

## Relationship to design docs

- `12` §"The two quantifiers" — p14 is the `∃` half; p13 was the `∀` half. The shared interface (here a `Product`) is the bridge `11`'s label-record will eventually replace.
- `01` — existentials are the TAPL Ch. 24 region.
