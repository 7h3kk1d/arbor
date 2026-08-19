{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part III — typing (arbor-stlc §sec:typing)
--
-- The judgment, with T-Ref (rem:tref: "p6 has no such rule — it inlines a
-- referenced definition's whole body at resolution time, so no reference
-- survives to the typing judgment"). Deliberately pure of the Θ aspect: it
-- re-derives a target's typing rather than reading a cache, mirroring how ⇓
-- never reads E.
------------------------------------------------------------------------

open import Arbor.Stlc.Params using (Params)

module Arbor.Stlc.Typing (P : Params) where

open import Arbor.Stlc.Store P public

open import Data.List.Base using (List; []; _∷_; length)
open import Data.Nat.Base using (ℕ; zero; suc; _<_; s≤s; z≤n)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; trans; cong; subst)

------------------------------------------------------------------------
-- Γ(i) = T

data Lookup : List Term → ℕ → Term → Set where
  here  : ∀ {Γ T}     → Lookup (T ∷ Γ) 0 T
  there : ∀ {Γ i T U} → Lookup Γ i T → Lookup (U ∷ Γ) (suc i) T

lookup-< : ∀ {Γ i T} → Lookup Γ i T → i < length Γ
lookup-< here      = s≤s z≤n
lookup-< (there l) = s≤s (lookup-< l)

lookup-func : ∀ {Γ i T U} → Lookup Γ i T → Lookup Γ i U → T ≡ U
lookup-func here      here      = refl
lookup-func (there l) (there k) = lookup-func l k

------------------------------------------------------------------------
-- The typing judgment (def:typing)
--
-- One departure from the paper, forced by the unsorted encoding: T-Abs carries
-- IsTy for its annotation. In the paper types are a separate syntactic
-- category, so `LamT T t` has T a type by construction; here Term is one
-- datatype and the sort has to be said. The paper's "the annotation is taken as
-- given, never checked against anything" still holds — IsTy is a sort
-- condition, not a check against the body.

infix 4 _⨾_⊢_∶_
data _⨾_⊢_∶_ (σ : Store) : List Term → Term → Term → Set where
  t-true  : ∀ {Γ} → σ ⨾ Γ ⊢ true ∶ TBool
  t-false : ∀ {Γ} → σ ⨾ Γ ⊢ false ∶ TBool
  t-var   : ∀ {Γ i T} → Lookup Γ i T → σ ⨾ Γ ⊢ var i ∶ T
  t-abs   : ∀ {Γ T t U} → IsTy T → σ ⨾ (T ∷ Γ) ⊢ t ∶ U →
            σ ⨾ Γ ⊢ lamT T t ∶ TArr T U
  t-app   : ∀ {Γ t u T U} → σ ⨾ Γ ⊢ t ∶ TArr T U → σ ⨾ Γ ⊢ u ∶ T →
            σ ⨾ Γ ⊢ app t u ∶ U
  t-if    : ∀ {Γ c a b T} → σ ⨾ Γ ⊢ c ∶ TBool → σ ⨾ Γ ⊢ a ∶ T → σ ⨾ Γ ⊢ b ∶ T →
            σ ⨾ Γ ⊢ iff c a b ∶ T
  -- T-Ref: types the reference THROUGH the store, the same idealization step
  -- arbor-core took when it reintroduced the reference node.
  t-ref   : ∀ {Γ h t T} → σ ⊢ h ⇝ t → σ ⨾ [] ⊢ t ∶ T → σ ⨾ Γ ⊢ ref h ∶ T

------------------------------------------------------------------------
-- lem:ty-scope — typing implies scoping
--
-- "In particular a T-Ref target is closed with no separate premise."

ty-scope : ∀ {σ Γ t T} → σ ⨾ Γ ⊢ t ∶ T → length Γ ⊢ t
ty-scope t-true          = s-true
ty-scope t-false         = s-false
ty-scope (t-var l)       = s-var (lookup-< l)
ty-scope (t-abs isT d)   = s-lam (ty-scoped _ isT) (ty-scope d)
ty-scope (t-app d₁ d₂)   = s-app (ty-scope d₁) (ty-scope d₂)
ty-scope (t-if d₁ d₂ d₃) = s-if (ty-scope d₁) (ty-scope d₂) (ty-scope d₃)
ty-scope (t-ref _ _)     = s-ref

------------------------------------------------------------------------
-- lem:ty-unique — the rules are syntax-directed
--
-- "Induction on the derivation, inversion at each step (T-Ref included: both
-- derivations continue at the same reconstruction)."

ty-unique : ∀ {σ Γ t T U} → σ ⨾ Γ ⊢ t ∶ T → σ ⨾ Γ ⊢ t ∶ U → T ≡ U
ty-unique t-true        t-true        = refl
ty-unique t-false       t-false       = refl
ty-unique (t-var l)     (t-var k)     = lookup-func l k
ty-unique (t-abs _ d)   (t-abs _ e)   = cong (TArr _) (ty-unique d e)
ty-unique (t-app d₁ _)  (t-app e₁ _)  = arr-cod (ty-unique d₁ e₁)
  where
  arr-cod : ∀ {A B C D} → TArr A B ≡ TArr C D → B ≡ D
  arr-cod refl = refl
ty-unique (t-if _ d _)  (t-if _ e _)  = ty-unique d e
ty-unique (t-ref r₁ d)  (t-ref r₂ e)  with ⇝-func r₁ r₂
... | refl = ty-unique d e

------------------------------------------------------------------------
-- def:typeof — typeof and well-typedness

TypeOf : Store → Hash → Term → Set
TypeOf σ h T = ∃[ t ] ((σ ⊢ h ⇝ t) × (σ ⨾ [] ⊢ t ∶ T))

wt : Store → Hash → Set
wt σ h = ∃[ T ] TypeOf σ h T

-- typeof is a partial FUNCTION (lem:ty-unique at the top level).
typeof-func : ∀ {σ h T U} → TypeOf σ h T → TypeOf σ h U → T ≡ U
typeof-func (t , r , d) (u , r′ , e) with ⇝-func r r′
... | refl = ty-unique d e

-- "wt implies closed_Σ(h) — the typed analogue refines the untyped notion."
wt→ClosedIn : ∀ {σ h} → wt σ h → ClosedIn σ h
wt→ClosedIn (T , t , r , d) = t , r , ty-scope d

------------------------------------------------------------------------
-- lem:ty-stable — typing is absolute for stored hashes
--
-- "A derivation reads only entries reachable from t's references, which are
-- identical in both stores by immutability; replay in either direction — the
-- same argument as thm:stability. So the type of a stored term is a fact about
-- the hash, fixed at ingest time forever."
--
-- Note this is the typed twin of arbor-core's ⇝-mono/⇓-mono, and like them it
-- needs only Σ ⊆ Σ′ — no well-formedness premise.

ty-mono : ∀ {σ σ′ Γ t T} → σ ⊑ σ′ → σ ⨾ Γ ⊢ t ∶ T → σ′ ⨾ Γ ⊢ t ∶ T
ty-mono sub t-true          = t-true
ty-mono sub t-false         = t-false
ty-mono sub (t-var l)       = t-var l
ty-mono sub (t-abs isT d)   = t-abs isT (ty-mono sub d)
ty-mono sub (t-app d₁ d₂)   = t-app (ty-mono sub d₁) (ty-mono sub d₂)
ty-mono sub (t-if d₁ d₂ d₃) = t-if (ty-mono sub d₁) (ty-mono sub d₂) (ty-mono sub d₃)
ty-mono sub (t-ref r d)     = t-ref (⇝-mono sub r) (ty-mono sub d)

ty-stable : ∀ {σ σ′ h T} → σ ⊑ σ′ → TypeOf σ h T → TypeOf σ′ h T
ty-stable sub (t , r , d) = t , ⇝-mono sub r , ty-mono sub d

wt-stable : ∀ {σ σ′ h} → σ ⊑ σ′ → wt σ h → wt σ′ h
wt-stable sub (T , tp) = T , ty-stable sub tp
