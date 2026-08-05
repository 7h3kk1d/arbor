{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Finite partial functions, the shape every store in the paper takes
--
-- The paper writes A ⇀ B for "finite partial function" (arbor-core §"Scope
-- and posture", the notation paragraph). We represent one as a total
-- function into Maybe, keyed by a type with decidable equality.
--
-- decisions.md 2026-08-05 (store representation): a bare function makes
-- inclusion — which is what monotonicity (thm:mono) and evaluation
-- stability (thm:stability) are stated in terms of — literally
-- "everything the smaller map says, the bigger map says too", with no
-- lookup-vs-membership bookkeeping. Finiteness is not needed until the
-- migration rewrite has to enumerate dom(Σ) (M3); a `support` field joins
-- the record then, not before.
------------------------------------------------------------------------

module Arbor.Prelude where

open import Data.Empty using (⊥; ⊥-elim)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Relation.Binary.Definitions using (DecidableEquality)
open import Relation.Binary.PropositionalEquality using (_≡_; _≢_; refl; sym; trans; cong)
open import Relation.Nullary using (¬_)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

-- Kept outside the parameterized module below: a level metavariable that
-- depends on a module parameter does not get solved.
nothing≢just : ∀ {V : Set} {x : V} → nothing ≡ just x → ⊥
nothing≢just ()

just-inj : ∀ {V : Set} {x y : V} → just x ≡ just y → x ≡ y
just-inj refl = refl

module Map {K : Set} (_≟_ : DecidableEquality K) where

  PMap : Set → Set
  PMap V = K → Maybe V

  -- The empty map.
  ∅ : ∀ {V} → PMap V
  ∅ _ = nothing

  -- The paper's f[a ↦ b].
  infixl 6 _[_↦_]
  _[_↦_] : ∀ {V} → PMap V → K → V → PMap V
  (m [ k ↦ v ]) k′ with k′ ≟ k
  ... | yes _ = just v
  ... | no  _ = m k′

  -- The paper's f ∖ a.
  infixl 6 _∖_
  _∖_ : ∀ {V} → PMap V → K → PMap V
  (m ∖ k) k′ with k′ ≟ k
  ... | yes _ = nothing
  ... | no  _ = m k′

  -- dom and ran.
  _∈dom_ : ∀ {V} → K → PMap V → Set
  k ∈dom m = ∃[ v ] (m k ≡ just v)

  _∈ran_ : ∀ {V} → V → PMap V → Set
  v ∈ran m = ∃[ k ] (m k ≡ just v)

  -- Inclusion of partial functions: the ⊆ of thm:mono and thm:stability.
  infix 4 _⊑_
  _⊑_ : ∀ {V} → PMap V → PMap V → Set
  m ⊑ m′ = ∀ k v → m k ≡ just v → m′ k ≡ just v

  ⊑-refl : ∀ {V} {m : PMap V} → m ⊑ m
  ⊑-refl _ _ eq = eq

  ⊑-trans : ∀ {V} {m₁ m₂ m₃ : PMap V} → m₁ ⊑ m₂ → m₂ ⊑ m₃ → m₁ ⊑ m₃
  ⊑-trans p q k v eq = q k v (p k v eq)

  ------------------------------------------------------------------------
  -- Lookup after update

  lookup-hit : ∀ {V} (m : PMap V) k v → (m [ k ↦ v ]) k ≡ just v
  lookup-hit m k v with k ≟ k
  ... | yes _  = refl
  ... | no ¬p  = ⊥-elim (¬p refl)

  lookup-miss : ∀ {V} (m : PMap V) {k k′} v → k′ ≢ k → (m [ k ↦ v ]) k′ ≡ m k′
  lookup-miss m {k} {k′} v ¬p with k′ ≟ k
  ... | yes p = ⊥-elim (¬p p)
  ... | no  _ = refl

  remove-hit : ∀ {V} (m : PMap V) k → (m ∖ k) k ≡ nothing
  remove-hit m k with k ≟ k
  ... | yes _ = refl
  ... | no ¬p = ⊥-elim (¬p refl)

  remove-miss : ∀ {V} (m : PMap V) {k k′} → k′ ≢ k → (m ∖ k) k′ ≡ m k′
  remove-miss m {k} {k′} ¬p with k′ ≟ k
  ... | yes p = ⊥-elim (¬p p)
  ... | no  _ = refl

  ------------------------------------------------------------------------
  -- Growth
  --
  -- An update grows the map exactly when it does not disturb what is
  -- already there: either the key is fresh, or it already holds the very
  -- value being written. In the store this side condition is discharged by
  -- injectivity of the hash (arbor-core thm:mono: "each key is ⌈n⌉ for the
  -- very node n stored, so any collision on a key is with an identical
  -- node").

  Undisturbed : ∀ {V} → PMap V → K → V → Set
  Undisturbed m k v = (m k ≡ nothing) ⊎ (m k ≡ just v)

  -- Taking the decision as an argument rather than `with`-abstracting it keeps
  -- the goal unabstracted, so lookup-hit/lookup-miss apply directly.
  ⊑-update : ∀ {V} {m : PMap V} {k v} → Undisturbed m k v → m ⊑ (m [ k ↦ v ])
  ⊑-update {V} {m} {k} {v} (inj₁ absent) k′ v′ eq = go k′ (k′ ≟ k) eq
    where
    go : ∀ k″ → Dec (k″ ≡ k) → m k″ ≡ just v′ → (m [ k ↦ v ]) k″ ≡ just v′
    go k″ (no ¬p)    e = trans (lookup-miss m {k} {k″} v ¬p) e
    go _  (yes refl) e = ⊥-elim (nothing≢just (trans (sym absent) e))
  ⊑-update {V} {m} {k} {v} (inj₂ present) k′ v′ eq = go k′ (k′ ≟ k) eq
    where
    go : ∀ k″ → Dec (k″ ≡ k) → m k″ ≡ just v′ → (m [ k ↦ v ]) k″ ≡ just v′
    go k″ (no ¬p)    e = trans (lookup-miss m {k} {k″} v ¬p) e
    go _  (yes refl) e = trans (lookup-hit m k v) (trans (sym present) e)

  -- An update never removes a key.
  update-∈dom : ∀ {V} (m : PMap V) k v → k ∈dom (m [ k ↦ v ])
  update-∈dom m k v = v , lookup-hit m k v

  ------------------------------------------------------------------------
  -- Reading a lookup back: it came either from the update or from the map

  update-split : ∀ {V} (m : PMap V) k v {k′ v′} → (m [ k ↦ v ]) k′ ≡ just v′ →
                 ((k′ ≡ k) × (v′ ≡ v)) ⊎ (m k′ ≡ just v′)
  update-split m k v {k′} {v′} eq = go k′ (k′ ≟ k) eq
    where
    go : ∀ k″ → Dec (k″ ≡ k) → (m [ k ↦ v ]) k″ ≡ just v′ →
         ((k″ ≡ k) × (v′ ≡ v)) ⊎ (m k″ ≡ just v′)
    go _  (yes refl) e = inj₁ (refl , just-inj (trans (sym e) (lookup-hit m k v)))
    go k″ (no ¬p)    e = inj₂ (trans (sym (lookup-miss m {k} {k″} v ¬p)) e)

  remove-split : ∀ {V} (m : PMap V) k {k′ v′} → (m ∖ k) k′ ≡ just v′ →
                 (k′ ≢ k) × (m k′ ≡ just v′)
  remove-split m k {k′} {v′} eq = go k′ (k′ ≟ k) eq
    where
    go : ∀ k″ → Dec (k″ ≡ k) → (m ∖ k) k″ ≡ just v′ →
         (k″ ≢ k) × (m k″ ≡ just v′)
    go _  (yes refl) e = ⊥-elim (nothing≢just (trans (sym (remove-hit m k)) e))
    go k″ (no ¬p)    e = ¬p , trans (sym (remove-miss m {k} {k″} ¬p)) e
