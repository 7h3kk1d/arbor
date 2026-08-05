{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The hash model — arbor-core (def:hash)
--
-- The paper's Hash is "an opaque type with an injective map ⌈·⌉ : Node →
-- Hash", injectivity (★) idealizing cryptographic collision-resistance.
-- Here that is a *record*, and the whole development is parameterized over
-- an inhabitant of it. So the paper's one axiom is one record field rather
-- than a postulate: nothing in this library postulates anything.
--
-- The record is parameterized over the entry functor F, and `hash` has type
-- F Hash → Hash. That indirection is forced: a node's children are hashes,
-- so Node mentions Hash, and a record with both `Hash : Set` and
-- `hash : Node → Hash` as fields would be circular. Writing entries as
-- `F : Set → Set` breaks the knot, and it is also what lets the graph layer
-- serve a second language later (see Arbor.NodeSig).
--
-- arbor-core rem:concrete-hash: p4's BLAKE2B-over-tag-bytes is one
-- realization of this record, and no proof inspects the encoding. A witness
-- that the record is inhabited at all — so that no theorem below is
-- vacuous — is Arbor.Core.Model.
------------------------------------------------------------------------

module Arbor.Hash where

open import Relation.Binary.Definitions using (DecidableEquality)
open import Relation.Binary.PropositionalEquality using (_≡_)

record HashModel (F : Set → Set) : Set₁ where
  field
    Hash     : Set
    _≟_      : DecidableEquality Hash
    hash     : F Hash → Hash
    -- (★): the paper's sole axiom.
    hash-inj : ∀ {m n : F Hash} → hash m ≡ hash n → m ≡ n
