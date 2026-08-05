{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part IV — the evaluation cache (arbor-core §sec:cache, def:cache)
--
-- E : Hash ⇀ Hash, a *derived* aspect mapping the hash of a term to the hash
-- of its value, itself ingested into the store. Both key and value are store
-- terms; the cache never touches surface syntax (decisions.md 2026-07-30).
--
-- Its whole discipline is Cache-sound: an entry may be written only for a
-- *defined* evaluation, and once written is never overwritten. rem:nostepcache
-- is the reason StepLimit never gets an entry — it is "no derivation yet", so
-- there is nothing for Cache-sound to be sound about.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Cache (P : Params) where

open import Arbor.Core.Naming P public

open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax)
open import Relation.Binary.PropositionalEquality using (_≡_)

Cache : Set
Cache = PMap Hash

-- Cache-sound (def:cache). Stated with the value existentially quantified
-- rather than as reconstruct(Σ, E(h)) applied: same content, since ⇝ is
-- functional (⇝-func).
CacheSound : Store → Cache → Set
CacheSound σ ε = ∀ {h hv} → ε h ≡ just hv →
                 ∃[ v ] ((σ ⊢ hv ⇝ v) × (σ ⊢ ref h ⇓ v))

-- The mathematical content of cor:cache: a written entry stays valid in every
-- future store. What remains for the transition relation to supply is that no
-- rule ever removes or overwrites an entry.
CacheSound-mono : ∀ {σ σ′ ε} → σ ⊑ σ′ → CacheSound σ ε → CacheSound σ′ ε
CacheSound-mono sub cs eq with cs eq
... | (v , r , d) = v , ⇝-mono sub r , ⇓-mono sub d
