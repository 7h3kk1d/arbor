{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Everything the development assumes, in one record
--
-- Threading four module parameters through nine modules is unreadable, so
-- the assumed data is bundled and every downstream module takes a single
-- `(P : Params)`. Nothing here is postulated: Arbor.Core.Model exhibits an
-- inhabitant, so no theorem in this library is vacuous.
--
--   hashModel  arbor-core def:hash, including (★)
--   Name       def:ns — "an opaque string type"; the substrate imposes no
--              hierarchy and no ambiguity (decisions.md 2026-07-23)
--   Time       def:history — "fresh timestamps are drawn monotonically; we
--              leave the clock abstract"
------------------------------------------------------------------------

module Arbor.Core.Params where

open import Arbor.Core.Node using (Node)
open import Arbor.Hash using (HashModel)

open import Relation.Binary.Definitions using (DecidableEquality)

record Params : Set₁ where
  field
    hashModel : HashModel Node
    Name      : Set
    _≟N_      : DecidableEquality Name
    Time      : Set
