{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The syntax signature
--
-- What the store layer needs to know about a language, and no more: a set of
-- node shapes, and for each shape how many STRUCTURAL children it has and how
-- many REFERENCE children.
--
-- The two counts are what the store's shallow/deep split forces. A stored node
-- has all its children as hashes; a deep term has its structural children
-- recursive and its reference children still hashes — because a reference is
-- exactly where the deep form stops and cites a definition instead
-- (arbor-core def:node, def:core). One signature, filled in two ways.
--
-- Arities rather than position TYPES, deliberately. With `SPos s → Term` the
-- functionality of reconstruction would need function extensionality to equate
-- two children-functions, and --safe does not provide it. `Vec Term (sArity s)`
-- keeps equality structural.
--
-- What the signature does NOT model is binding. Scoping, shift, substitution
-- and β are language-layer and stay concrete; the generic store layer takes
-- closedness as a parameter instead.
------------------------------------------------------------------------

module Arbor.Sig where

open import Data.Nat.Base using (ℕ)

record Sig : Set₁ where
  field
    Shape  : Set
    sArity : Shape → ℕ
    rArity : Shape → ℕ
