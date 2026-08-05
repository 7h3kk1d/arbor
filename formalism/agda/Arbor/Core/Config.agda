{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part V — the edit calculus (arbor-core def:config, def:transitions,
-- def:migrate)
--
-- Configurations ⟨Σ, N, E, H⟩, coherence, and the transition relation.
--
-- The transition relation is COMPLETE here — all six rules, migration
-- included — even though the migration rewrite itself is only specified
-- (Arbor.Core.Migrate.RewriteData) rather than constructed. That is
-- deliberate: it means M1's theorems quantify over the real relation and are
-- final, not provisional. thm:nsb's proof, for instance, has to survive the
-- existence of a Migrate rule; it does, because Migrate is simply not one of
-- the three name edits.
--
-- Coherence carries one clause the paper does not state: `keyed`. See
-- Arbor.Core.Store's header and open-questions.md — thm:mono's argument needs
-- it and def:wf does not supply it.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Config (P : Params) where

open import Arbor.Core.Migrate P public

open import Data.List.Base using (List; []; _∷_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Relation.Unary.All using (All)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; proj₁; proj₂)
open import Relation.Binary.PropositionalEquality using (_≡_)

------------------------------------------------------------------------
-- Configurations (def:config)

record Config : Set where
  constructor ⟨_,_,_,_⟩
  field
    store : Store
    names : Namespace
    cache : Cache
    hist  : History

open Config public

------------------------------------------------------------------------
-- Coherence (def:config)
--
--  (a) wf(Σ);
--  (b) closed_Σ(h) for every h ∈ ran(N) — "a name may assert definition-hood
--      only of a closed stored term" (rem:nosort);
--  (c) closed_Σ(h) for every hash in a Some event of H — orphans remain
--      closed, reconstructible terms, which is what makes def:orphan's
--      rendering meaningful;
--  (d) history coherence and Cache-sound.
--
-- Plus `keyed`, discussed in the header.

record Coherent (C : Config) : Set where
  field
    wf           : WF (store C)
    keyed        : HashKeyed (store C)
    names-closed : ∀ {x h} → names C x ≡ just h → ClosedIn (store C) h
    hist-closed  : ∀ {x h τ} → (just h , τ) ∈ events (hist C) x →
                   ClosedIn (store C) h
    hist-coh     : HistCoherent (names C) (hist C)
    cache-sound  : CacheSound (store C) (cache C)

------------------------------------------------------------------------
-- Transitions — (def:transitions), (def:migrate)
--
-- The premises are load-bearing. Ingest's are established by elaboration
-- (lem:elab-premises) but must be stated here or preservation fails.
-- Bind/Rebind require closed_Σ(h) and nothing more — ANY closed stored hash
-- may be named (rem:nosort). Rebind also binds fresh names, as in p11; Bind is
-- its first-bind special case.

infix 2 _⟶_
data _⟶_ : Config → Config → Set where

  t-ingest : ∀ {σ ν ε η t} →
             Closed t →
             (∀ {r} → r ∈ refsT t → ClosedIn σ r) →
             ⟨ σ , ν , ε , η ⟩ ⟶ ⟨ proj₁ (ingest σ t) , ν , ε , η ⟩

  -- Eval grows Σ: the cache is hash-valued, so the computed value is itself
  -- ingested (decisions.md 2026-07-30).
  --
  -- The ClosedIn premise is NOT in the paper's def:transitions, and is a
  -- correction rather than a convenience. thm:wf's Eval case argues "Eval
  -- ingests a value that is closed (evaluation preserves closedness,
  -- lem:closed-no-stuck)" — but that lemma is false without knowing h's target
  -- is closed (Arbor.Core.Counterexamples), and nothing else in the rule
  -- supplies it. It costs nothing operationally: p4 evaluates definitions, and
  -- a named hash is closed by coherence clause (b).
  t-eval   : ∀ {σ ν ε η h v} →
             ClosedIn σ h →
             σ ⊢ ref h ⇓ v →
             ε h ≡ nothing →
             ⟨ σ , ν , ε , η ⟩ ⟶
             ⟨ proj₁ (ingest σ v) , ν , ε [ h ↦ proj₂ (ingest σ v) ] , η ⟩

  t-bind   : ∀ {σ ν ε η x h τ} →
             ν x ≡ nothing →
             ClosedIn σ h →
             ⟨ σ , ν , ε , η ⟩ ⟶
             ⟨ σ , bindName ν x h , ε , appendEvent η x (just h , τ) ⟩

  t-rebind : ∀ {σ ν ε η x h τ} →
             ClosedIn σ h →
             ⟨ σ , ν , ε , η ⟩ ⟶
             ⟨ σ , bindName ν x h , ε , appendEvent η x (just h , τ) ⟩

  t-unbind : ∀ {σ ν ε η x h τ} →
             ν x ≡ just h →
             ⟨ σ , ν , ε , η ⟩ ⟶
             ⟨ σ , unbindName ν x , ε , appendEvent η x (nothing , τ) ⟩

  -- Migrate (def:migrate). The bundle U is validated — every target closed in
  -- Σ′ — and commits all-or-nothing; on failure there is simply no step, so
  -- ⟨N,H⟩ is untouched while Σ may retain candidate hashes.
  t-migrate : ∀ {σ ν ε η x₀ gold gnew sc τ}
              (R : RewriteData σ gold gnew sc) (U : Bundle) →
              ν x₀ ≡ just gold →
              ClosedIn σ gnew →
              (x₀ , gnew) ∈ U →
              All (λ p → ClosedIn (RewriteData.σ′ R) (proj₂ p)) U →
              ⟨ σ , ν , ε , η ⟩ ⟶
              ⟨ RewriteData.σ′ R
              , proj₁ (multiRebind ν η τ U)
              , ε
              , proj₂ (multiRebind ν η τ U) ⟩

------------------------------------------------------------------------
-- The three name edits
--
-- Named so that thm:nsb can be stated about exactly the rules the paper
-- states it about: Bind, Rebind, Unbind.

data NameEdit : Config → Config → Set where
  ne-bind   : ∀ {σ ν ε η x h τ} →
              ν x ≡ nothing → ClosedIn σ h →
              NameEdit ⟨ σ , ν , ε , η ⟩
                       ⟨ σ , bindName ν x h , ε , appendEvent η x (just h , τ) ⟩
  ne-rebind : ∀ {σ ν ε η x h τ} →
              ClosedIn σ h →
              NameEdit ⟨ σ , ν , ε , η ⟩
                       ⟨ σ , bindName ν x h , ε , appendEvent η x (just h , τ) ⟩
  ne-unbind : ∀ {σ ν ε η x h τ} →
              ν x ≡ just h →
              NameEdit ⟨ σ , ν , ε , η ⟩
                       ⟨ σ , unbindName ν x , ε , appendEvent η x (nothing , τ) ⟩

nameEdit-step : ∀ {C C′} → NameEdit C C′ → C ⟶ C′
nameEdit-step (ne-bind fresh cl) = t-bind fresh cl
nameEdit-step (ne-rebind cl)     = t-rebind cl
nameEdit-step (ne-unbind bound)  = t-unbind bound
