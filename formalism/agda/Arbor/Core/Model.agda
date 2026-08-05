{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- A witness that the assumptions are consistent
--
-- The whole development is parameterized over Params, whose interesting field
-- is a HashModel — an opaque Hash with an INJECTIVE hash : Node Hash → Hash.
-- If no such record existed, every theorem in Arbor.Core.Meta would be
-- vacuously true and the build would still be green. So we exhibit one.
--
-- Take Hash to be the free type over Node itself:
--
--     data H : Set where wrap : Node H → H
--
-- `wrap` is injective because Agda's constructors are, so (★) holds by refl.
-- This is the paper's own rejected alternative (agda/README.md,
-- "Rejected alternative: Hash := Term, hash := id") — rejected as the
-- REPRESENTATION, because interning collapses `ref` back to inlining and makes
-- the store semantically inert, weakening thm:nsb and thm:stability. As a
-- consistency witness it is exactly what is wanted and costs nothing: the
-- theorems are proved for an arbitrary HashModel, and this one only shows the
-- class is non-empty.
--
-- The final line instantiates the entire development at this model, which also
-- checks that Params is usable as a module parameter end to end.
------------------------------------------------------------------------

module Arbor.Core.Model where

open import Arbor.Core.Node using (Node; nvar; nlam; napp; nref)
open import Arbor.Core.Params using (Params)
open import Arbor.Hash using (HashModel)

open import Data.Nat.Base using (ℕ)
import Data.Nat.Properties as Nat
open import Data.String using (String)
import Data.String.Properties as Str
open import Relation.Binary.Definitions using (DecidableEquality)
open import Relation.Binary.PropositionalEquality using (_≡_; refl)
open import Relation.Nullary.Decidable.Core using (yes; no)

data H : Set where
  wrap : Node H → H

mutual
  _≟H_ : DecidableEquality H
  wrap m ≟H wrap n with m ≟Nd n
  ... | yes refl = yes refl
  ... | no ¬p    = no λ { refl → ¬p refl }

  _≟Nd_ : DecidableEquality (Node H)
  nvar i   ≟Nd nvar j   with i Nat.≟ j
  ... | yes refl = yes refl
  ... | no ¬p    = no λ { refl → ¬p refl }
  nlam a   ≟Nd nlam b   with a ≟H b
  ... | yes refl = yes refl
  ... | no ¬p    = no λ { refl → ¬p refl }
  napp a b ≟Nd napp c d with a ≟H c | b ≟H d
  ... | yes refl | yes refl = yes refl
  ... | no ¬p    | _        = no λ { refl → ¬p refl }
  ... | _        | no ¬q    = no λ { refl → ¬q refl }
  nref a   ≟Nd nref b   with a ≟H b
  ... | yes refl = yes refl
  ... | no ¬p    = no λ { refl → ¬p refl }
  nvar _   ≟Nd nlam _   = no λ()
  nvar _   ≟Nd napp _ _ = no λ()
  nvar _   ≟Nd nref _   = no λ()
  nlam _   ≟Nd nvar _   = no λ()
  nlam _   ≟Nd napp _ _ = no λ()
  nlam _   ≟Nd nref _   = no λ()
  napp _ _ ≟Nd nvar _   = no λ()
  napp _ _ ≟Nd nlam _   = no λ()
  napp _ _ ≟Nd nref _   = no λ()
  nref _   ≟Nd nvar _   = no λ()
  nref _   ≟Nd nlam _   = no λ()
  nref _   ≟Nd napp _ _ = no λ()

-- (★) holds by constructor injectivity.
hashModel : HashModel Node
hashModel = record
  { Hash     = H
  ; _≟_      = _≟H_
  ; hash     = wrap
  ; hash-inj = λ { refl → refl }
  }

params : Params
params = record
  { hashModel = hashModel
  ; Name      = String
  ; _≟N_      = Str._≟_
  ; Time      = ℕ
  }

-- Instantiate everything at the witness.
import Arbor.Core.Meta
module Instance = Arbor.Core.Meta params
