{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Ingest preserves well-formedness, over any signature
--
-- Arbor.Core.Preservation's ingest-* family, generically. Each concrete
-- four-case induction becomes one shape case plus a fold over the structural
-- children — which is why these are shorter here than there.
------------------------------------------------------------------------

open import Arbor.Sig using (Sig)
open import Arbor.Hash using (HashModel)
import Arbor.Generic.Syntax as GS

module Arbor.Generic.Preservation
  (Σg : Sig) (HM : HashModel (GS.Node Σg)) where

open import Arbor.Generic.Store Σg HM public

open import Data.List.Base using (List; []; _++_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Membership.Propositional.Properties using (∈-++⁻; ∈-++⁺ˡ; ∈-++⁺ʳ)
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.Maybe.Base using (just; nothing)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Data.Vec.Base using (Vec; toList) renaming ([] to ⟦⟧; _∷_ to _◂_)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; subst)

------------------------------------------------------------------------
-- Growth past the root registration, named once

grow-root : ∀ s (ss : Vec Term (sArity s)) rs σ → HashKeyed σ →
            proj₁ (ingestV σ ss) ⊑ proj₁ (ingest σ ⟨ s , ss , rs ⟩)
grow-root s ss rs σ kd = ⊑-update (keyed-undisturbed (ingestV-keyed ss σ kd) _)

------------------------------------------------------------------------
-- wf (i): every stored hash reconstructs

mutual
  ingest-recon : ∀ t σ → HashKeyed σ →
                 (∀ {h} → h ∈dom σ → Recon σ h) →
                 ∀ {h} → h ∈dom (proj₁ (ingest σ t)) →
                 Recon (proj₁ (ingest σ t)) h
  ingest-recon t@(⟨ s , ss , rs ⟩) σ kd recT mem
    with update-dom (node s (proj₂ (ingestV σ ss)) rs) mem
  ... | inj₁ eq = subst (Recon _) (sym eq) (t , ingest-⇝ t σ kd)
  ... | inj₂ m₁ with ingestV-recon ss σ kd recT m₁
  ...   | (t′ , d) = t′ , ⇝-mono (grow-root s ss rs σ kd) d

  ingestV-recon : ∀ {n} (ts : Vec Term n) σ → HashKeyed σ →
                  (∀ {h} → h ∈dom σ → Recon σ h) →
                  ∀ {h} → h ∈dom (proj₁ (ingestV σ ts)) →
                  Recon (proj₁ (ingestV σ ts)) h
  ingestV-recon ⟦⟧       σ kd recT mem = recT mem
  ingestV-recon (t ◂ ts) σ kd recT mem =
    ingestV-recon ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd)
                  (ingest-recon t σ kd recT) mem

------------------------------------------------------------------------
-- wf (iii)'s provenance: every edge leaves an already-stored hash, or lands in
-- the base store

mutual
  ingest-edges : ∀ t σ {σ₀ : Store} → HashKeyed σ →
                 (∀ {h} → h ∈dom σ → Recon σ h) →
                 (∀ {r} → r ∈ refsT t → r ∈dom σ₀) →
                 ∀ {h r} → (proj₁ (ingest σ t)) ⊢ h ↝ r →
                 (h ∈dom σ) ⊎ (r ∈dom σ₀)
  ingest-edges t@(⟨ s , ss , rs ⟩) σ kd recT rc (t′ , d′ , memr)
    with update-dom (node s (proj₂ (ingestV σ ss)) rs) (⇝-∈dom d′)
  ... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ t σ kd)
  ...   | refl = inj₂ (rc memr)
  ingest-edges t@(⟨ s , ss , rs ⟩) σ kd recT rc (t′ , d′ , memr) | inj₂ m₁ =
    ingestV-edges ss σ kd recT (λ m → rc (∈-++⁺ʳ (toList rs) m))
      (⊑-edges (grow-root s ss rs σ kd) (ingestV-recon ss σ kd recT m₁)
               (t′ , d′ , memr))

  ingestV-edges : ∀ {n} (ts : Vec Term n) σ {σ₀ : Store} → HashKeyed σ →
                  (∀ {h} → h ∈dom σ → Recon σ h) →
                  (∀ {r} → r ∈ refsV ts → r ∈dom σ₀) →
                  ∀ {h r} → (proj₁ (ingestV σ ts)) ⊢ h ↝ r →
                  (h ∈dom σ) ⊎ (r ∈dom σ₀)
  ingestV-edges ⟦⟧       σ kd recT rc (t′ , d′ , memr) = inj₁ (⇝-∈dom d′)
  ingestV-edges (t ◂ ts) σ {σ₀} kd recT rc {h} {r} e = finish (ingestV-edges ts σ₁ kd₁ rec₁ rcs e)
    where
    σ₁   = proj₁ (ingest σ t)
    kd₁  = ingest-keyed t σ kd
    rec₁ = ingest-recon t σ kd recT
    rcs : ∀ {y} → y ∈ refsV ts → y ∈dom σ₀
    rcs m = rc (∈-++⁺ʳ (refsT t) m)
    finish : (h ∈dom σ₁) ⊎ (r ∈dom σ₀) → (h ∈dom σ) ⊎ (r ∈dom σ₀)
    finish (inj₂ m) = inj₂ m
    finish (inj₁ m) =
      ingest-edges t σ kd recT (λ x → rc (∈-++⁺ˡ x))
        (⊑-edges (ingestV-⊑ ts σ₁ kd₁) (rec₁ m) e)

------------------------------------------------------------------------
-- Where an entry came from

mutual
  ingest-prov : ∀ t σ → HashKeyed σ →
                ∀ {g} → g ∈dom (proj₁ (ingest σ t)) →
                (g ∈dom σ)
              ⊎ (∃ λ t′ → ((proj₁ (ingest σ t)) ⊢ g ⇝ t′)
                        × (∀ {r} → r ∈ refsT t′ → r ∈ refsT t))
  ingest-prov t@(⟨ s , ss , rs ⟩) σ kd mem
    with update-dom (node s (proj₂ (ingestV σ ss)) rs) mem
  ... | inj₁ eq = inj₂ (t , subst (λ x → _ ⊢ x ⇝ t) (sym eq) (ingest-⇝ t σ kd)
                          , λ m → m)
  ... | inj₂ m₁ with ingestV-prov ss σ kd m₁
  ...   | inj₁ mσ                = inj₁ mσ
  ...   | inj₂ (t′ , d′ , sub) =
          inj₂ (t′ , ⇝-mono (grow-root s ss rs σ kd) d′
                   , λ m → ∈-++⁺ʳ (toList rs) (sub m))

  ingestV-prov : ∀ {n} (ts : Vec Term n) σ → HashKeyed σ →
                 ∀ {g} → g ∈dom (proj₁ (ingestV σ ts)) →
                 (g ∈dom σ)
               ⊎ (∃ λ t′ → ((proj₁ (ingestV σ ts)) ⊢ g ⇝ t′)
                         × (∀ {r} → r ∈ refsT t′ → r ∈ refsV ts))
  ingestV-prov ⟦⟧       σ kd mem = inj₁ mem
  ingestV-prov (t ◂ ts) σ kd mem with ingestV-prov ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd) mem
  ... | inj₂ (t′ , d′ , sub) =
        inj₂ (t′ , d′ , λ m → ∈-++⁺ʳ (refsT t) (sub m))
  ... | inj₁ m₁ with ingest-prov t σ kd m₁
  ...   | inj₁ mσ = inj₁ mσ
  ...   | inj₂ (t′ , d′ , sub) =
          inj₂ (t′ , ⇝-mono (ingestV-⊑ ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd)) d′
                   , λ m → ∈-++⁺ˡ (sub m))

------------------------------------------------------------------------
-- The half that mentions binding
--
-- wf clause (ii) is stated in terms of closedness, which the signature does not
-- model — so it enters here as a parameter. Only one lemma is needed, not two:
-- the version whose conclusion is pinned to a fixed larger store subsumes the
-- incremental one at `big = the step store`, and it is the one the migration
-- rewrite needs (Arbor.Core.Preservation's note on order-independence).

module WithClosed (Closed : Term → Set) where

  CIn : Store → Hash → Set
  CIn = ClosedIn Closed

  ClosedIn→∈dom : ∀ {σ r} → CIn σ r → r ∈dom σ
  ClosedIn→∈dom (_ , d , _) = ⇝-∈dom d

  mutual
    ingest-tgt-big :
      ∀ t σ (big : Store) → HashKeyed σ →
      (∀ {h} → h ∈dom σ → Recon σ h) →
      (∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → CIn big r) →
      (∀ {r} → r ∈ refsT t → CIn big r) →
      proj₁ (ingest σ t) ⊑ big →
      ∀ {h r} → h ∈dom (proj₁ (ingest σ t)) →
      (proj₁ (ingest σ t)) ⊢ h ↝ r → CIn big r
    ingest-tgt-big t@(⟨ s , ss , rs ⟩) σ big kd recT tgt rc sub mem (t′ , d′ , memr)
      with update-dom (node s (proj₂ (ingestV σ ss)) rs) mem
    ... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ t σ kd)
    ...   | refl = rc memr
    ingest-tgt-big t@(⟨ s , ss , rs ⟩) σ big kd recT tgt rc sub mem (t′ , d′ , memr)
      | inj₂ m₁ =
      ingestV-tgt-big ss σ big kd recT tgt (λ m → rc (∈-++⁺ʳ (toList rs) m))
        (⊑-trans (grow-root s ss rs σ kd) sub) m₁
        (⊑-edges (grow-root s ss rs σ kd) (ingestV-recon ss σ kd recT m₁)
                 (t′ , d′ , memr))

    ingestV-tgt-big :
      ∀ {n} (ts : Vec Term n) σ (big : Store) → HashKeyed σ →
      (∀ {h} → h ∈dom σ → Recon σ h) →
      (∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → CIn big r) →
      (∀ {r} → r ∈ refsV ts → CIn big r) →
      proj₁ (ingestV σ ts) ⊑ big →
      ∀ {h r} → h ∈dom (proj₁ (ingestV σ ts)) →
      (proj₁ (ingestV σ ts)) ⊢ h ↝ r → CIn big r
    ingestV-tgt-big ⟦⟧ σ big kd recT tgt rc sub mem edge = tgt mem edge
    ingestV-tgt-big (t ◂ ts) σ big kd recT tgt rc sub mem edge =
      ingestV-tgt-big ts σ₁ big kd₁ rec₁ tgt₁ rcs sub mem edge
      where
      σ₁   = proj₁ (ingest σ t)
      kd₁  = ingest-keyed t σ kd
      rec₁ = ingest-recon t σ kd recT
      rcs : ∀ {y} → y ∈ refsV ts → CIn big y
      rcs m = rc (∈-++⁺ʳ (refsT t) m)
      tgt₁ = ingest-tgt-big t σ big kd recT tgt (λ m → rc (∈-++⁺ˡ m))
               (⊑-trans (ingestV-⊑ ts σ₁ kd₁) sub)

  ingest-wf : ∀ t σ → HashKeyed σ → WF Closed σ →
              (∀ {r} → r ∈ refsT t → CIn σ r) →
              WF Closed (proj₁ (ingest σ t))
  ingest-wf t σ kd w rc = record
    { recon-total = ingest-recon t σ kd (WF.recon-total w)
    ; tgt-closed  = ingest-tgt-big t σ (proj₁ (ingest σ t)) kd (WF.recon-total w)
                      (λ m e → ClosedIn-mono grow (WF.tgt-closed w m e))
                      (λ m → ClosedIn-mono grow (rc m))
                      ⊑-refl
    ; ref-acyclic = acyclic-transfer grow (WF.recon-total w)
                      (λ m e → ClosedIn→∈dom (WF.tgt-closed w m e))
                      (WF.ref-acyclic w)
                      (ingest-edges t σ kd (WF.recon-total w)
                                    (λ m → ClosedIn→∈dom (rc m)))
    }
    where grow = ingest-⊑ t σ kd
