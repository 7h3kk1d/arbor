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

  ------------------------------------------------------------------------
  -- What the migration rewrite needs of substRefsD
  --
  -- The congruence is what makes the rewrite well defined at all: ρ is built by
  -- well-founded recursion, so its step function is determined only up to the
  -- accessibility proof handed to it, and two such agree only pointwise.

  mutual
    substRefsD-cong : ∀ t {f g : ∀ r → r ∈ refsT t → Hash} →
                      (∀ r m → f r m ≡ g r m) → substRefsD t f ≡ substRefsD t g
    substRefsD-cong ⟨ s , ss , rs ⟩ eq =
      cong₂ (λ a b → ⟨ s , a , b ⟩)
        (substRefsV-cong ss (λ r m → eq r _))
        (mapIn-cong   rs (λ r m → eq r _))

    substRefsV-cong : ∀ {n} (ts : Vec Term n) {f g : ∀ r → r ∈ refsV ts → Hash} →
                      (∀ r m → f r m ≡ g r m) → substRefsV ts f ≡ substRefsV ts g
    substRefsV-cong ⟦⟧       eq = refl
    substRefsV-cong (t ◂ ts) eq =
      cong₂ _◂_ (substRefsD-cong t  (λ r m → eq r _))
                (substRefsV-cong ts (λ r m → eq r _))

    mapIn-cong : ∀ {n} (rs : Vec Hash n) {f g : ∀ r → r ∈ toList rs → Hash} →
                 (∀ r m → f r m ≡ g r m) → mapIn rs f ≡ mapIn rs g
    mapIn-cong ⟦⟧       eq = refl
    mapIn-cong (r ◂ rs) eq = cong₂ _◂_ (eq r _) (mapIn-cong rs (λ x m → eq x _))

  -- And that every reference of a rewritten term is the image of a reference of
  -- the original — the provenance fact acyclicity turns on.

  mutual
    substRefsD-refs : ∀ t {f : ∀ r → r ∈ refsT t → Hash} {r′} →
                      r′ ∈ refsT (substRefsD t f) →
                      ∃ λ r → Σ (r ∈ refsT t) (λ m → f r m ≡ r′)
    substRefsD-refs ⟨ s , ss , rs ⟩ {f} mem
      with ∈-++⁻ (toList (mapIn rs (λ r m → f r (∈-++⁺ˡ m)))) mem
    ... | inj₁ m₁ with mapIn-refs rs m₁
    ...   | (r , mr , eq) = r , ∈-++⁺ˡ mr , eq
    substRefsD-refs ⟨ s , ss , rs ⟩ {f} mem | inj₂ m₂ with substRefsV-refs ss m₂
    ...   | (r , mr , eq) = r , ∈-++⁺ʳ (toList rs) mr , eq

    substRefsV-refs : ∀ {n} (ts : Vec Term n) {f : ∀ r → r ∈ refsV ts → Hash} {r′} →
                      r′ ∈ refsV (substRefsV ts f) →
                      ∃ λ r → Σ (r ∈ refsV ts) (λ m → f r m ≡ r′)
    substRefsV-refs ⟦⟧ ()
    substRefsV-refs (t ◂ ts) {f} mem
      with ∈-++⁻ (refsT (substRefsD t (λ r m → f r (∈-++⁺ˡ m)))) mem
    ... | inj₁ m₁ with substRefsD-refs t m₁
    ...   | (r , mr , eq) = r , ∈-++⁺ˡ mr , eq
    substRefsV-refs (t ◂ ts) {f} mem | inj₂ m₂ with substRefsV-refs ts m₂
    ...   | (r , mr , eq) = r , ∈-++⁺ʳ (refsT t) mr , eq

    mapIn-refs : ∀ {n} (rs : Vec Hash n) {f : ∀ r → r ∈ toList rs → Hash} {r′} →
                 r′ ∈ toList (mapIn rs f) →
                 ∃ λ r → Σ (r ∈ toList rs) (λ m → f r m ≡ r′)
    mapIn-refs ⟦⟧       ()
    mapIn-refs (r ◂ rs) (here refl) = r , here refl , refl
    mapIn-refs (r ◂ rs) (there m) with mapIn-refs rs m
    ... | (y , my , eq) = y , there my , eq
