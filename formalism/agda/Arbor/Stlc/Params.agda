{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- What the STLC rung assumes: arbor-core's Params at the two-sorted signature.
------------------------------------------------------------------------

module Arbor.Stlc.Params where

open import Arbor.Stlc.Node using (Node)
open import Arbor.Hash using (HashModel)
open import Relation.Binary.Definitions using (DecidableEquality)

record Params : Set₁ where
  field
    hashModel : HashModel Node
    Name      : Set
    _≟N_      : DecidableEquality Name
    Time      : Set
