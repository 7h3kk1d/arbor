{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The STLC rung as a signature (arbor-stlc def:entry)
--
-- Nine shapes: seven term entries and two type entries. Both sorts live in one
-- Shape set over one hash space, which is the paper's own position — def:entry
-- has a single injective ⌈·⌉ over the whole sum and derives the sorts'
-- disjointness as "a corollary, not an axiom".
--
-- Sort discipline is therefore a PREDICATE here (Arbor.Stlc.Syntax), not an
-- index. def:reconstruct instead folds it into definedness, by giving
-- `reconstruct` and `reconstructTy` separate partial functions; one relation
-- plus a well-sortedness predicate says the same thing. Recorded as a
-- presentational divergence in ../../open-questions.md.
--
-- Note nlamT's annotation is a STRUCTURAL child, not a reference: def:entry
-- carries it "by hash", and it must reconstruct, so it counts toward sArity.
-- Only nref contributes reference arity — which is why type entries are
-- isolated vertices of the reference graph (def:reconstruct) by construction.
------------------------------------------------------------------------

module Arbor.Stlc.Node where

open import Arbor.Sig using (Sig)
import Arbor.Generic.Syntax as GS

open import Data.Nat.Base using (ℕ)
open import Data.Vec.Base using (Vec) renaming ([] to ⟦⟧; _∷_ to _◂_)

data StlcShape : Set where
  -- term entries (def:entry, first line)
  svar   : ℕ → StlcShape   -- Var i
  slamT  : StlcShape       -- LamT T t   : annotation + body
  sapp   : StlcShape       -- App t u
  strue  : StlcShape       -- true
  sfalse : StlcShape       -- false
  sif    : StlcShape       -- If c a b
  sref   : StlcShape       -- Ref h
  -- type entries (def:entry, second line)
  sTBool : StlcShape       -- Bool
  sTArr  : StlcShape       -- T ⇒ U

stlcSig : Sig
stlcSig = record
  { Shape  = StlcShape
  ; sArity = λ { (svar _) → 0 ; slamT → 2 ; sapp → 2 ; strue → 0 ; sfalse → 0
               ; sif → 3 ; sref → 0 ; sTBool → 0 ; sTArr → 2 }
  ; rArity = λ { (svar _) → 0 ; slamT → 0 ; sapp → 0 ; strue → 0 ; sfalse → 0
               ; sif → 0 ; sref → 1 ; sTBool → 0 ; sTArr → 0 }
  }

Node : Set → Set
Node = GS.Node stlcSig

open GS stlcSig public using (node)

pattern nvar i      = node (svar i) ⟦⟧ ⟦⟧
pattern nlamT hT h  = node slamT (hT ◂ h ◂ ⟦⟧) ⟦⟧
pattern napp h₁ h₂  = node sapp (h₁ ◂ h₂ ◂ ⟦⟧) ⟦⟧
pattern ntrue       = node strue ⟦⟧ ⟦⟧
pattern nfalse      = node sfalse ⟦⟧ ⟦⟧
pattern nif c a b   = node sif (c ◂ a ◂ b ◂ ⟦⟧) ⟦⟧
pattern nref r      = node sref ⟦⟧ (r ◂ ⟦⟧)
pattern nTBool      = node sTBool ⟦⟧ ⟦⟧
pattern nTArr A B   = node sTArr (A ◂ B ◂ ⟦⟧) ⟦⟧
