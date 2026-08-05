{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The graph-layer signature
--
-- decisions.md 2026-08-05 (reuse boundary): everything the paper says about
-- the store *as a graph* — well-formedness, callers, the migration rewrite —
-- inspects an entry only through three operations: its structural children,
-- its reference children, and a map over both. It never looks at whether the
-- entry is a lambda. So those definitions are written against this record and
-- instantiated by Arbor.Core.Node, which makes arbor-stlc's two-sorted entry
-- (arbor-stlc def:entry) a second instance rather than a fork.
--
-- The split between `children` and `refsOf` is the one the paper's rewrite
-- turns on (def:cascade): structural edges always propagate, reference edges
-- only into scope. Hence mapE takes two functions.
--
-- Config and the transition relation are deliberately *not* generic:
-- arbor-stlc adds a TypeOf transition, so abstracting there would buy
-- nothing.
------------------------------------------------------------------------

module Arbor.NodeSig where

open import Data.List.Base using (List; map)
open import Function.Base using (id)
open import Relation.Binary.PropositionalEquality using (_≡_)

record NodeSig (F : Set → Set) : Set₁ where
  field
    -- Children reached by containment. In a shallow store these are the
    -- subterm slots (arbor-core def:node).
    children : ∀ {H} → F H → List H
    -- Children reached by citation: the ref leaf's target (def:refs).
    refsOf   : ∀ {H} → F H → List H
    -- The paper's ρ# (def:cascade): rewrite structural children with the
    -- first function, reference children with the second.
    mapE     : ∀ {H} → (H → H) → (H → H) → F H → F H

    children-mapE : ∀ {H} (f g : H → H) (e : F H) →
                    children (mapE f g e) ≡ map f (children e)
    refsOf-mapE   : ∀ {H} (f g : H → H) (e : F H) →
                    refsOf (mapE f g e) ≡ map g (refsOf e)
    -- "Everything else re-hashes to itself" (def:cascade).
    mapE-id       : ∀ {H} (e : F H) → mapE id id e ≡ e
