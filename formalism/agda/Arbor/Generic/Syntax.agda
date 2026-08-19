{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Nodes and deep terms over a signature
--
-- Arbor.Core.Syntax's Node and Term, generically. The core instance recovers
-- exactly the four constructors of arbor-core def:core, via pattern synonyms,
-- so proofs downstream still read as four cases.
--
-- Node is deliberately OUTSIDE the Hash parameter: it is a functor in its
-- child type, which is what lets Arbor.Hash's HashModel be stated over it
-- without circularity.
------------------------------------------------------------------------

open import Arbor.Sig using (Sig)

module Arbor.Generic.Syntax (Σg : Sig) where

open Sig Σg public

open import Data.List.Base using (List; []; _++_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Membership.Propositional.Properties using (∈-++⁻; ∈-++⁺ˡ; ∈-++⁺ʳ)
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.Nat.Base using (ℕ)
open import Data.Product using (Σ; _×_; _,_; ∃)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Data.Vec.Base using (Vec; toList) renaming ([] to ⟦⟧; _∷_ to _◂_)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; cong; cong₂)

------------------------------------------------------------------------
-- The shallow node (def:node): every child is a hash.

record Node (H : Set) : Set where
  constructor node
  field
    shape : Shape
    schil : Vec H (sArity shape)
    rchil : Vec H (rArity shape)

-- Projections out of a node equality. Needed because a node's child vectors
-- have types depending on its shape, so the shape has to be equated first
-- before Agda can even state that the vectors are.
node-shape : ∀ {H} {n m : Node H} → n ≡ m → Node.shape n ≡ Node.shape m
node-shape refl = refl

node-schil : ∀ {H s} {ss ss′ : Vec H (sArity s)} {rs rs′ : Vec H (rArity s)} →
             node s ss rs ≡ node s ss′ rs′ → ss ≡ ss′
node-schil refl = refl

node-rchil : ∀ {H s} {ss ss′ : Vec H (sArity s)} {rs rs′ : Vec H (rArity s)} →
             node s ss rs ≡ node s ss′ rs′ → rs ≡ rs′
node-rchil refl = refl

------------------------------------------------------------------------
-- Deep terms, for a fixed hash type

module WithHash (Hash : Set) where

  -- Structural children recurse; reference children stay hashes, because a
  -- reference is exactly where the deep form stops and cites a definition.
  data Term : Set where
    ⟨_,_,_⟩ : (s : Shape) → Vec Term (sArity s) → Vec Hash (rArity s) → Term

  ------------------------------------------------------------------------
  -- References (def:refs): direct citations, not their closure.

  mutual
    refsT : Term → List Hash
    refsT ⟨ s , ss , rs ⟩ = toList rs ++ refsV ss

    refsV : ∀ {n} → Vec Term n → List Hash
    refsV ⟦⟧       = []
    refsV (t ◂ ts) = refsT t ++ refsV ts

  ------------------------------------------------------------------------
  -- Rewriting reference leaves, with a membership proof at each one.

  mutual
    substRefsD : (t : Term) → (∀ r → r ∈ refsT t → Hash) → Term
    substRefsD ⟨ s , ss , rs ⟩ f =
      ⟨ s
      , substRefsV ss (λ r m → f r (∈-++⁺ʳ (toList rs) m))
      , mapIn rs (λ r m → f r (∈-++⁺ˡ m))
      ⟩

    substRefsV : ∀ {n} (ts : Vec Term n) → (∀ r → r ∈ refsV ts → Hash) → Vec Term n
    substRefsV ⟦⟧       f = ⟦⟧
    substRefsV (t ◂ ts) f =
      substRefsD t  (λ r m → f r (∈-++⁺ˡ m))
      ◂ substRefsV ts (λ r m → f r (∈-++⁺ʳ (refsT t) m))

    mapIn : ∀ {n} (rs : Vec Hash n) → (∀ r → r ∈ toList rs → Hash) → Vec Hash n
    mapIn ⟦⟧       f = ⟦⟧
    mapIn (r ◂ rs) f = f r (here refl) ◂ mapIn rs (λ x m → f x (there m))
