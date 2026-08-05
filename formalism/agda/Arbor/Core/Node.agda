{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Shallow nodes — arbor-core (def:node) — and the graph-layer instance
--
-- "A node is one constructor deep, with hash-typed children." The entry type
-- is parameterized over the hash type so that Arbor.Hash's HashModel record
-- can mention it without circularity.
--
-- nref's child is a *reference* child, not a structural one: that is the
-- whole point of the distinction (def:cascade — structure always propagates,
-- references only into scope).
------------------------------------------------------------------------

module Arbor.Core.Node where

open import Arbor.NodeSig using (NodeSig)

open import Data.List.Base using (List; []; _∷_; map)
open import Data.Nat.Base using (ℕ)
open import Data.Product using (_×_; _,_)
open import Function.Base using (id)
open import Relation.Binary.PropositionalEquality using (_≡_; refl)

data Node (H : Set) : Set where
  nvar : ℕ → Node H
  nlam : H → Node H
  napp : H → H → Node H
  nref : H → Node H

------------------------------------------------------------------------
-- Constructor injectivity
--
-- Free in Agda, but named because the (⇒) half of thm:alpha is exactly
-- "equal root hashes force equal top nodes (★), hence equal child hashes",
-- and these are the "hence".

nvar-inj : ∀ {H} {i j : ℕ} → nvar {H} i ≡ nvar j → i ≡ j
nvar-inj refl = refl

nlam-inj : ∀ {H} {a b : H} → nlam a ≡ nlam b → a ≡ b
nlam-inj refl = refl

napp-inj : ∀ {H} {a b c d : H} → napp a b ≡ napp c d → (a ≡ c) × (b ≡ d)
napp-inj refl = refl , refl

nref-inj : ∀ {H} {a b : H} → nref a ≡ nref b → a ≡ b
nref-inj refl = refl

------------------------------------------------------------------------
-- The NodeSig instance

children : ∀ {H} → Node H → List H
children (nvar _)     = []
children (nlam c)     = c ∷ []
children (napp c₁ c₂) = c₁ ∷ c₂ ∷ []
children (nref _)     = []

refsOf : ∀ {H} → Node H → List H
refsOf (nvar _)   = []
refsOf (nlam _)   = []
refsOf (napp _ _) = []
refsOf (nref r)   = r ∷ []

-- The paper's ρ# (def:cascade).
mapE : ∀ {H} → (H → H) → (H → H) → Node H → Node H
mapE f g (nvar i)     = nvar i
mapE f g (nlam c)     = nlam (f c)
mapE f g (napp c₁ c₂) = napp (f c₁) (f c₂)
mapE f g (nref r)     = nref (g r)

children-mapE : ∀ {H} (f g : H → H) (e : Node H) →
                children (mapE f g e) ≡ map f (children e)
children-mapE f g (nvar _)     = refl
children-mapE f g (nlam _)     = refl
children-mapE f g (napp _ _)   = refl
children-mapE f g (nref _)     = refl

refsOf-mapE : ∀ {H} (f g : H → H) (e : Node H) →
              refsOf (mapE f g e) ≡ map g (refsOf e)
refsOf-mapE f g (nvar _)   = refl
refsOf-mapE f g (nlam _)   = refl
refsOf-mapE f g (napp _ _) = refl
refsOf-mapE f g (nref _)   = refl

mapE-id : ∀ {H} (e : Node H) → mapE id id e ≡ e
mapE-id (nvar _)   = refl
mapE-id (nlam _)   = refl
mapE-id (napp _ _) = refl
mapE-id (nref _)   = refl

nodeSig : NodeSig Node
nodeSig = record
  { children      = children
  ; refsOf        = refsOf
  ; mapE          = mapE
  ; children-mapE = children-mapE
  ; refsOf-mapE   = refsOf-mapE
  ; mapE-id       = mapE-id
  }
