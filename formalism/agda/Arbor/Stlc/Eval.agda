{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part V — evaluation (arbor-stlc def:value)
--
-- arbor-core's def:eval with E-Lam generalized to values and two rules for the
-- conditional; E-Ref and E-App unchanged. As there, it reads Σ only to unfold
-- references and takes no namespace argument.
------------------------------------------------------------------------

open import Arbor.Stlc.Params using (Params)

module Arbor.Stlc.Eval (P : Params) where

open import Arbor.Stlc.Typing P public

open import Data.Product using (_×_; _,_; ∃-syntax)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; cong₂)

------------------------------------------------------------------------
-- Values (def:value): "v ::= LamT T t | true | false"

data Value : Term → Set where
  v-lam   : ∀ {T t} → Value (lamT T t)
  v-true  : Value true
  v-false : Value false

------------------------------------------------------------------------
-- The evaluation relation

infix 4 _⊢_⇓_
data _⊢_⇓_ (σ : Store) : Term → Term → Set where
  e-val     : ∀ {v} → Value v → σ ⊢ v ⇓ v
  e-ref     : ∀ {h t v} → σ ⊢ h ⇝ t → σ ⊢ t ⇓ v → σ ⊢ ref h ⇓ v
  e-app     : ∀ {t u T t₀ vu v} →
              σ ⊢ t ⇓ lamT T t₀ → σ ⊢ u ⇓ vu → σ ⊢ beta t₀ vu ⇓ v →
              σ ⊢ app t u ⇓ v
  e-if-true : ∀ {c a b v} → σ ⊢ c ⇓ true  → σ ⊢ a ⇓ v → σ ⊢ iff c a b ⇓ v
  e-if-false : ∀ {c a b v} → σ ⊢ c ⇓ false → σ ⊢ b ⇓ v → σ ⊢ iff c a b ⇓ v

-- Determinism carries over: "the guard's value selects at most one rule".
⇓-det : ∀ {σ t v₁ v₂} → σ ⊢ t ⇓ v₁ → σ ⊢ t ⇓ v₂ → v₁ ≡ v₂
⇓-det (e-val _) (e-val _) = refl
⇓-det (e-ref r₁ d₁) (e-ref r₂ d₂) with ⇝-func r₁ r₂
... | refl = ⇓-det d₁ d₂
⇓-det (e-app f₁ a₁ b₁) (e-app f₂ a₂ b₂) with ⇓-det f₁ f₂ | ⇓-det a₁ a₂
... | refl | refl = ⇓-det b₁ b₂
⇓-det (e-if-true  g₁ d₁) (e-if-true  g₂ d₂) = ⇓-det d₁ d₂
⇓-det (e-if-false g₁ d₁) (e-if-false g₂ d₂) = ⇓-det d₁ d₂
⇓-det (e-if-true  g₁ _)  (e-if-false g₂ _)  with ⇓-det g₁ g₂
... | ()
⇓-det (e-if-false g₁ _)  (e-if-true  g₂ _)  with ⇓-det g₁ g₂
... | ()

-- Stability under store growth, exactly as in arbor-core.
⇓-mono : ∀ {σ σ′ t v} → σ ⊑ σ′ → σ ⊢ t ⇓ v → σ′ ⊢ t ⇓ v
⇓-mono sub (e-val vv)        = e-val vv
⇓-mono sub (e-ref r d)       = e-ref (⇝-mono sub r) (⇓-mono sub d)
⇓-mono sub (e-app f a b)     = e-app (⇓-mono sub f) (⇓-mono sub a) (⇓-mono sub b)
⇓-mono sub (e-if-true g d)   = e-if-true (⇓-mono sub g) (⇓-mono sub d)
⇓-mono sub (e-if-false g d)  = e-if-false (⇓-mono sub g) (⇓-mono sub d)

------------------------------------------------------------------------
-- lem:canon — canonical forms
--
-- "A closed value of type Bool is true or false; of type T ⇒ U is some
-- LamT T t."

data IsBoolLit : Term → Set where
  is-true  : IsBoolLit true
  is-false : IsBoolLit false

canon-bool : ∀ {σ Γ v} → Value v → σ ⨾ Γ ⊢ v ∶ TBool → IsBoolLit v
canon-bool v-true  _ = is-true
canon-bool v-false _ = is-false

canon-arr : ∀ {σ Γ v T U} → Value v → σ ⨾ Γ ⊢ v ∶ TArr T U →
            ∃-syntax (λ t₀ → v ≡ lamT T t₀)
canon-arr (v-lam {t = t₀}) (t-abs _ _) = t₀ , refl
