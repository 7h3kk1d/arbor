{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part III — the namespace (arbor-core §sec:naming)
--
-- The namespace (def:ns), resolution (def:resolve), elaboration (def:elab),
-- and printing (def:print).
--
-- Resolution is the substrate primitive: exact lookup, two outcomes. p9/p11
-- do implement dotted-suffix resolution with an Ambiguous error, but at the
-- *editing* layer; decisions.md 2026-07-23 records that divergence and keeps
-- this judgment two-valued.
--
-- Elaboration is a partial function (the two identifier rules are disjoint:
-- el-bound takes the LEAST index, el-free requires x ∉ Γ). Printing is
-- deliberately a *relation* — p-lam picks any binder name, p-ref any alias —
-- whose leaf side conditions silently reject capturing choices.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Naming (P : Params) where

open import Arbor.Core.Eval P public

open import Arbor.Core.Surface Name public
import Arbor.Prelude

-- A second instantiation of the map module: the namespace and the binding
-- history are keyed by Name, the store and the aspects by Hash.
module MN = Arbor.Prelude.Map _≟N_

open import Data.List.Base using (List; []; _∷_; length; _++_)
open import Data.List.Membership.Propositional using (_∈_; _∉_)
open import Data.List.Membership.Propositional.Properties using (∈-++⁻)
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Nat.Base using (ℕ; zero; suc; _<_; s≤s; z≤n)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (inj₁; inj₂)
open import Data.Empty using (⊥-elim)
open import Relation.Binary.PropositionalEquality
  using (_≡_; _≢_; refl; sym; trans; cong; cong₂)
open import Arbor.Prelude using (just-inj)

------------------------------------------------------------------------
-- The namespace (def:ns)
--
-- "Functional in the name (a name resolves to at most one hash) but not
-- injective (aliases — several names for one hash — are free)."

Namespace : Set
Namespace = MN.PMap Hash

-- Resolution (def:resolve).
Resolves : Namespace → Name → Hash → Set
Resolves ν x h = ν x ≡ just h

bindName : Namespace → Name → Hash → Namespace
bindName ν x h = MN._[_↦_] ν x h

unbindName : Namespace → Name → Namespace
unbindName ν x = MN._∖_ ν x

------------------------------------------------------------------------
-- "the least index i with Γ(i) = x"
--
-- This is what makes shadowing unambiguous (λx.λx. x elaborates only to
-- lam (lam (var 0))) and the two identifier rules disjoint.

data FirstIdx (x : Name) : List Name → ℕ → Set where
  fst-here  : ∀ {Γ} → FirstIdx x (x ∷ Γ) 0
  fst-there : ∀ {y Γ i} → y ≢ x → FirstIdx x Γ i → FirstIdx x (y ∷ Γ) (suc i)

firstIdx-< : ∀ {x Γ i} → FirstIdx x Γ i → i < length Γ
firstIdx-< fst-here        = s≤s z≤n
firstIdx-< (fst-there _ f) = s≤s (firstIdx-< f)

-- The least index is unique, and it witnesses membership. Together these make
-- el-bound and el-free disjoint, which is what def:elab means by "elaboration
-- is a partial function".
firstIdx-func : ∀ {x Γ i j} → FirstIdx x Γ i → FirstIdx x Γ j → i ≡ j
firstIdx-func fst-here          fst-here          = refl
firstIdx-func fst-here          (fst-there ¬p _)  = ⊥-elim (¬p refl)
firstIdx-func (fst-there ¬p _)  fst-here          = ⊥-elim (¬p refl)
firstIdx-func (fst-there _ f)   (fst-there _ g)   = cong suc (firstIdx-func f g)

firstIdx-∈ : ∀ {x Γ i} → FirstIdx x Γ i → x ∈ Γ
firstIdx-∈ fst-here        = here refl
firstIdx-∈ (fst-there _ f) = there (firstIdx-∈ f)

------------------------------------------------------------------------
-- Elaboration: surface → core (def:elab)

data Elab (ν : Namespace) : List Name → Surface → Term → Set where
  el-bound : ∀ {Γ x i} → FirstIdx x Γ i →
             Elab ν Γ (svar x) (var i)
  el-free  : ∀ {Γ x h} → x ∉ Γ → Resolves ν x h →
             Elab ν Γ (svar x) (ref h)
  el-lam   : ∀ {Γ x s t} → Elab ν (x ∷ Γ) s t →
             Elab ν Γ (slam x s) (lam t)
  el-app   : ∀ {Γ s₁ s₂ t₁ t₂} → Elab ν Γ s₁ t₁ → Elab ν Γ s₂ t₂ →
             Elab ν Γ (sapp s₁ s₂) (app t₁ t₂)

------------------------------------------------------------------------
-- Printing: core → surface (def:print)

data Print (ν : Namespace) : List Name → Term → Surface → Set where
  p-var : ∀ {Δ x i} → FirstIdx x Δ i →
          Print ν Δ (var i) (svar x)
  p-ref : ∀ {Δ x h} → x ∉ Δ → Resolves ν x h →
          Print ν Δ (ref h) (svar x)
  p-lam : ∀ {Δ x t s} → Print ν (x ∷ Δ) t s →
          Print ν Δ (lam t) (slam x s)
  p-app : ∀ {Δ t₁ t₂ s₁ s₂} → Print ν Δ t₁ s₁ → Print ν Δ t₂ s₂ →
          Print ν Δ (app t₁ t₂) (sapp s₁ s₂)

------------------------------------------------------------------------
-- Printing inverts to elaboration (prop:print-elab)
--
-- "Whatever rendering the printer chooses, re-resolving it recovers exactly
-- the stored term." Each P rule's side condition is precisely the matching El
-- rule's premise, so the induction is a rule-for-rule rename.

print-elab : ∀ {ν Δ t s} → Print ν Δ t s → Elab ν Δ s t
print-elab (p-var f)     = el-bound f
print-elab (p-ref nx r)  = el-free nx r
print-elab (p-lam p)     = el-lam (print-elab p)
print-elab (p-app p₁ p₂) = el-app (print-elab p₁) (print-elab p₂)

------------------------------------------------------------------------
-- Elaboration discharges the ingest premises (lem:elab-premises)
--
-- "A top-level elaboration produces a closed term all of whose references
-- lie in ran(N) — under a coherent configuration these are exactly the
-- premises of the Ingest transition."

elab-scoped : ∀ {ν Γ s t} → Elab ν Γ s t → length Γ ⊢ t
elab-scoped (el-bound f)     = s-var (firstIdx-< f)
elab-scoped (el-free _ _)    = s-ref
elab-scoped (el-lam d)       = s-lam (elab-scoped d)
elab-scoped (el-app d₁ d₂)   = s-app (elab-scoped d₁) (elab-scoped d₂)

elab-refs : ∀ {ν Γ s t} → Elab ν Γ s t →
            ∀ {r} → r ∈ refsT t → MN._∈ran_ r ν
elab-refs (el-bound _) ()
elab-refs (el-free {x = x} _ res) (here refl) = x , res
elab-refs (el-lam d)   mem = elab-refs d mem
elab-refs {t = app t₁ t₂} (el-app d₁ d₂) mem with ∈-++⁻ (refsT t₁) mem
... | inj₁ m₁ = elab-refs d₁ m₁
... | inj₂ m₂ = elab-refs d₂ m₂

------------------------------------------------------------------------
-- Elaboration is a partial function (def:elab)
--
-- "The least-index premise makes shadowing unambiguous, and the two
-- identifier rules are disjoint, so elaboration is a partial *function*."

elab-func : ∀ {ν Γ s t t′} → Elab ν Γ s t → Elab ν Γ s t′ → t ≡ t′
elab-func (el-bound f)   (el-bound g)    = cong var (firstIdx-func f g)
elab-func (el-bound f)   (el-free nx _)  = ⊥-elim (nx (firstIdx-∈ f))
elab-func (el-free nx _) (el-bound g)    = ⊥-elim (nx (firstIdx-∈ g))
elab-func (el-free _ r₁) (el-free _ r₂)  = cong ref (just-inj (trans (sym r₁) r₂))
elab-func (el-lam d₁)    (el-lam d₂)     = cong lam (elab-func d₁ d₂)
elab-func (el-app a₁ b₁) (el-app a₂ b₂)  =
  cong₂ app (elab-func a₁ a₂) (elab-func b₁ b₂)

------------------------------------------------------------------------
-- Elaboration and printing are inverse rule-for-rule (prop:elab-print)
--
-- print-elab's converse. Together they say the two judgments are the same
-- relation read in opposite directions, which makes the *existence* half of
-- prop:elab-print immediate: the printer can always return the very surface
-- term the elaboration came from.
--
-- FINDING (2026-08-05): the paper guards prop:elab-print with "every reference
-- in t has a name outside Γ". That premise is redundant — an Elab derivation
-- already witnesses it at every el-free leaf, which is exactly what p-ref
-- needs. Worth noting because with Name abstract there is no fresh-name supply,
-- so a proof that had to *choose* binder names (as the paper's sketch does:
-- "choosing binder names away from the finitely many aliases the subterm's
-- references need") would not go through at all. Reusing s avoids the issue.

elab-print : ∀ {ν Γ s t} → Elab ν Γ s t → Print ν Γ t s
elab-print (el-bound f)    = p-var f
elab-print (el-free nx r)  = p-ref nx r
elab-print (el-lam d)      = p-lam (elab-print d)
elab-print (el-app d₁ d₂)  = p-app (elab-print d₁) (elab-print d₂)
