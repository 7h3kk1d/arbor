{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part V — type safety (arbor-stlc lem:subst, thm:pres, thm:local)
--
-- The substitution lemma is proved in the form preservation uses: thm:pres is
-- stated at the EMPTY context, so the value being substituted is closed
-- (lem:ty-scope), and a closed term is unmoved by the shifting that [_↦_]
-- performs as it descends under binders. That collapses the usual TAPL 9.3.8
-- shift-and-weaken machinery into one induction over the context prefix.
--
-- FINDING: the paper states lem:subst at a general Γ, with `Σ;Γ ⊢ v : T₁`.
-- Nothing in arbor-stlc uses that generality — thm:pres is the lemma's only
-- consumer and instantiates Γ to ∅, where `Σ;∅ ⊢ v : T₁` already forces v
-- closed. The general form is true but unused, and proving it would need the
-- shifting apparatus this proof avoids.
------------------------------------------------------------------------

open import Arbor.Stlc.Params using (Params)

module Arbor.Stlc.Safety (P : Params) where

open import Arbor.Stlc.Eval P public

open import Data.List.Base using (List; []; _∷_; _++_; length)
open import Data.Nat.Base using (ℕ; zero; suc; _≤_; _<_; z≤n; s≤s)
open import Data.Nat.Properties using (≤-refl; ≤-trans; ≤-pred; <-irrefl; ≰⇒>; <⇒≢)
  renaming (_≟_ to _≟ℕ_)
open import Data.Empty using (⊥-elim)
open import Data.Product using (_×_; _,_; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Relation.Binary.PropositionalEquality
  using (_≡_; refl; sym; trans; cong; subst)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

------------------------------------------------------------------------
-- Context lemmas

lookup-++ˡ : ∀ {Γ₁ Γ₂ i T} → Lookup Γ₁ i T → Lookup (Γ₁ ++ Γ₂) i T
lookup-++ˡ here      = here
lookup-++ˡ (there l) = there (lookup-++ˡ l)

-- A lookup in Γ₁ ++ (T ∷ []) is either inside Γ₁ or is the substituted
-- variable itself — there is nothing past it.
lookup-split : ∀ Γ₁ {T i U} → Lookup (Γ₁ ++ (T ∷ [])) i U →
               (Lookup Γ₁ i U) ⊎ ((i ≡ length Γ₁) × (U ≡ T))
lookup-split []        here      = inj₂ (refl , refl)
lookup-split []        (there ())
lookup-split (A ∷ Γ₁)  here      = inj₁ here
lookup-split (A ∷ Γ₁)  (there l) with lookup-split Γ₁ l
... | inj₁ l′         = inj₁ (there l′)
... | inj₂ (e₁ , e₂)  = inj₂ (cong suc e₁ , e₂)

-- Appending to the right of a context disturbs no index already in scope.
weaken : ∀ {σ Γ₁} Γ₂ {t T} → σ ⨾ Γ₁ ⊢ t ∶ T → σ ⨾ (Γ₁ ++ Γ₂) ⊢ t ∶ T
weaken Γ₂ t-true          = t-true
weaken Γ₂ t-false         = t-false
weaken Γ₂ (t-var l)       = t-var (lookup-++ˡ l)
weaken Γ₂ (t-abs isT d)   = t-abs isT (weaken Γ₂ d)
weaken Γ₂ (t-app d₁ d₂)   = t-app (weaken Γ₂ d₁) (weaken Γ₂ d₂)
weaken Γ₂ (t-if d₁ d₂ d₃) = t-if (weaken Γ₂ d₁) (weaken Γ₂ d₂) (weaken Γ₂ d₃)
weaken Γ₂ (t-ref r d)     = t-ref r d

------------------------------------------------------------------------
-- lem:subst — substitution
--
-- Generalized over the context PREFIX, which is what makes the induction go
-- under binders. The substituted variable is always the last in the context,
-- so its tail stays empty and v stays closed throughout.

subst-typed :
  ∀ {σ} Γ₁ {T v t U} → Closed v →
  σ ⨾ (Γ₁ ++ (T ∷ [])) ⊢ t ∶ U →
  σ ⨾ [] ⊢ v ∶ T →
  σ ⨾ Γ₁ ⊢ ↓ (length Γ₁) ([ length Γ₁ ↦ v ] t) ∶ U

subst-typed Γ₁ cv t-true  dv = t-true
subst-typed Γ₁ cv t-false dv = t-false

subst-typed {σ} Γ₁ {T} {v} {U = U} cv (t-var {i = i} l) dv
  with lookup-split Γ₁ l
-- the substituted variable itself: v, which is closed, so ↓ leaves it alone
... | inj₂ (refl , refl)
      rewrite subst-var-hit (length Γ₁) v
            | ↓-id {t = v} {n = 0} {c = length Γ₁} z≤n cv = weaken Γ₁ dv
-- any other variable is below the cut, so both operations pass it through
... | inj₁ l′
      rewrite subst-var-miss {i} {length Γ₁} v (<⇒≢ (lookup-< l′))
            | ↓-var-lo (lookup-< l′) = t-var l′

subst-typed {σ} Γ₁ {T} {v} cv (t-abs {T = A} isA d) dv
  rewrite ty-subst {s = v} (length Γ₁) isA
        | ty-↓ (length Γ₁) isA
        | ↑-id {t = v} {n = 0} {c = 0} 1 z≤n cv
  = t-abs isA (subst-typed (A ∷ Γ₁) cv d dv)

subst-typed Γ₁ cv (t-app d₁ d₂)   dv =
  t-app (subst-typed Γ₁ cv d₁ dv) (subst-typed Γ₁ cv d₂ dv)
subst-typed Γ₁ cv (t-if d₁ d₂ d₃) dv =
  t-if (subst-typed Γ₁ cv d₁ dv) (subst-typed Γ₁ cv d₂ dv) (subst-typed Γ₁ cv d₃ dv)
subst-typed Γ₁ cv (t-ref r d)     dv = t-ref r d

-- The paper's statement, at the context preservation uses.
lem-subst : ∀ {σ T t U v} →
            σ ⨾ (T ∷ []) ⊢ t ∶ U → σ ⨾ [] ⊢ v ∶ T →
            σ ⨾ [] ⊢ beta t v ∶ U
lem-subst {σ} {T} {t} {U} {v} d dv
  rewrite ↑-id {t = v} {n = 0} {c = 0} 1 z≤n (ty-scope dv) =
    subst-typed [] (ty-scope dv) d dv

------------------------------------------------------------------------
-- thm:pres — preservation
--
-- "Induction on the evaluation derivation. E-App: inversion + lem:subst.
-- E-Ref: inversion of T-Ref IS the typing of the unfolded target — no appeal
-- to any aspect or store invariant. E-IfTrue/False: inversion of T-If."
--
-- The E-Ref line is the one worth pausing on. Nothing here consults Θ, and
-- nothing here assumes anything about the store beyond what the derivation
-- itself carries: T-Ref's premise is the target's typing, so unfolding a
-- reference hands back exactly the derivation needed. That is what makes
-- thm:local below hold with no global hypothesis.

thm-pres : ∀ {σ t T v} → σ ⨾ [] ⊢ t ∶ T → σ ⊢ t ⇓ v → σ ⨾ [] ⊢ v ∶ T
thm-pres d (e-val _) = d
thm-pres (t-ref r′ d) (e-ref r e) with ⇝-func r′ r
... | refl = thm-pres d e
thm-pres (t-if d₁ d₂ d₃) (e-if-true  g e) = thm-pres d₂ e
thm-pres (t-if d₁ d₂ d₃) (e-if-false g e) = thm-pres d₃ e
thm-pres (t-app d₁ d₂)   (e-app f a b) with thm-pres d₁ f
... | t-abs isA body = thm-pres (lem-subst body (thm-pres d₂ a)) b

------------------------------------------------------------------------
-- thm:local — local soundness, well-typed islands
--
-- "Suppose wt_Σ(h). Then evaluating ref h ... if it converges the value has
-- type typeof(Σ,h) — INDEPENDENTLY of the typing status of every other entry
-- and every name. The store may be full of ill-typed closed debris and
-- broken(Σ,N) may be non-empty; none of it is reachable from h."
--
-- The independence is not a side condition discharged below; it is visible in
-- the statement, which quantifies over nothing but h's own derivation. "A
-- well-typed term carries its support with it" is exactly T-Ref: the
-- derivation contains the typing of everything in h's reference closure.

thm-local : ∀ {σ h T v} → TypeOf σ h T → σ ⊢ ref h ⇓ v → σ ⨾ [] ⊢ v ∶ T
thm-local (t , r , d) (e-ref r′ e) with ⇝-func r r′
... | refl = thm-pres d e

-- And it survives store growth, by lem:ty-stable and ⇓-mono — so the island
-- stays sound in every future store, not merely the one it was checked in.
thm-local-mono : ∀ {σ σ′ h T v} → σ ⊑ σ′ →
                 TypeOf σ h T → σ′ ⊢ ref h ⇓ v → σ′ ⨾ [] ⊢ v ∶ T
thm-local-mono sub tp e = thm-local (ty-stable sub tp) e

-- Every evaluation ends in a value — which needs no typing at all, only the
-- shape of the rules. Worth separating from thm:pres for that reason: the
-- typed content of preservation is the TYPE of the value, not its being one.
⇓-value : ∀ {σ t v} → σ ⊢ t ⇓ v → Value v
⇓-value (e-val vv)       = vv
⇓-value (e-ref _ e)      = ⇓-value e
⇓-value (e-app _ _ b)    = ⇓-value b
⇓-value (e-if-true _ e)  = ⇓-value e
⇓-value (e-if-false _ e) = ⇓-value e

-- So a well-typed closed term evaluates to a canonical form of its type
-- (thm:pres + lem:canon).
pres-canon-bool : ∀ {σ t v} → σ ⨾ [] ⊢ t ∶ TBool → σ ⊢ t ⇓ v → IsBoolLit v
pres-canon-bool d e = canon-bool (⇓-value e) (thm-pres d e)

pres-canon-arr : ∀ {σ t v T U} → σ ⨾ [] ⊢ t ∶ TArr T U → σ ⊢ t ⇓ v →
                 ∃-syntax (λ t₀ → v ≡ lamT T t₀)
pres-canon-arr d e = canon-arr (⇓-value e) (thm-pres d e)
