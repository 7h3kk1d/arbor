{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Binding history and orphans — arbor-core (def:history), (def:orphan)
--
-- H : Name ⇀ ((Hash ∪ {⊥}) × Time)*, newest event first. (Some h, τ) is a
-- bind/rebind; (⊥, τ) is an unbind tombstone. The current binding of x is the
-- head if it is Some, else x is unbound — which is exactly thm:histcoh, and
-- is why HistCoherent below can be stated as one clause relating the head of
-- H(x) to N(x).
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.History (P : Params) where

open import Arbor.Core.Cache P public

open import Data.List.Base using (List; []; _∷_; reverse)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.Maybe.Base using (Maybe; just; nothing; fromMaybe) renaming (map to mapMaybe)
open import Data.Nat.Base using (ℕ; suc)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Relation.Binary.PropositionalEquality using (_≡_; _≢_; refl; cong)
open import Relation.Nullary using (¬_)
open import Relation.Nullary.Decidable.Core using (yes; no)

open import Arbor.Hash using (HashModel)
open HashModel hashModel using (_≟_)

-- ⊥ is `nothing`: an unbind tombstone.
Event : Set
Event = Maybe Hash × Time

History : Set
History = MN.PMap (List Event)

-- H(x) read as a list, undefined-as-empty. Written with fromMaybe rather than
-- a `with`, so that the two lemmas below are congruences on a lookup rather
-- than case analyses that fight with-abstraction.
events : History → Name → List Event
events η x = fromMaybe [] (η x)

appendEvent : History → Name → Event → History
appendEvent η x ev = MN._[_↦_] η x (ev ∷ events η x)

events-hit : ∀ η x ev → events (appendEvent η x ev) x ≡ ev ∷ events η x
events-hit η x ev = cong (fromMaybe []) (MN.lookup-hit η x (ev ∷ events η x))

events-miss : ∀ η {x y} ev → y ≢ x → events (appendEvent η x ev) y ≡ events η y
events-miss η {x} {y} ev ¬p =
  cong (fromMaybe []) (MN.lookup-miss η {x} {y} (ev ∷ events η x) ¬p)

------------------------------------------------------------------------
-- History coherence (thm:histcoh)
--
-- The paper states two biconditionals: N(x) = h iff the head of H(x) is
-- (Some h, _); and x ∉ dom(N) iff H(x) is empty or has a ⊥ head. Both are the
-- single statement "the head's Maybe-Hash *is* N(x)", since ⊥ and
-- "no events" and "unbound" are all `nothing`.

data Head : List Event → Maybe Hash → Set where
  head-nil  : Head [] nothing
  head-cons : ∀ {mh τ es} → Head ((mh , τ) ∷ es) mh

HistCoherent : Namespace → History → Set
HistCoherent ν η = ∀ x → Head (events η x) (ν x)

------------------------------------------------------------------------
-- Orphans (def:orphan)
--
-- "A hash h is an orphan if it appears in some H(x) but is not the current
-- binding of any name." Its version is the 1-indexed position of the FIRST
-- occurrence of h among the Some events of H(x), oldest first — so a hash
-- rebound later re-renders as its first version. The printer shows x(vN);
-- that form is display-only, outside the surface grammar.

boundHashes : List Event → List Hash
boundHashes []                    = []
boundHashes ((just h  , _) ∷ es)  = h ∷ boundHashes es
boundHashes ((nothing , _) ∷ es)  = boundHashes es

Orphan : Namespace → History → Hash → Set
Orphan ν η h =
  (∃[ x ] (h ∈ boundHashes (events η x))) × (¬ (∃[ x ] (ν x ≡ just h)))

firstPos : List Hash → Hash → Maybe ℕ
firstPos []       h = nothing
firstPos (g ∷ gs) h with h ≟ g
... | yes _ = just 1
... | no  _ = mapMaybe suc (firstPos gs h)

-- The N in x(vN).
version : History → Name → Hash → Maybe ℕ
version η x h = firstPos (reverse (boundHashes (events η x))) h
