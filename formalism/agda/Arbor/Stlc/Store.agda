{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part II — the two-sorted store (arbor-stlc §sec:store2)
--
-- An INSTANTIATION, exactly as Arbor.Core.Store is. The store, ingest,
-- reconstruction, references, well-formedness, the migration rewrite and every
-- proof about them come from Arbor.Generic.*, unchanged; this module supplies
-- the STLC signature and specializes the definitions that mention closedness.
--
-- This is what the rung costs at the store layer: a signature and about forty
-- lines. def:entry, def:reconstruct and def:ingest are discharged by
-- instantiation rather than restated, and prop:ty-ident — "hashing collapses
-- type equality" — is arbor-core's thm:alpha at this signature, needing no
-- separate induction.
------------------------------------------------------------------------

open import Arbor.Stlc.Params using (Params)

module Arbor.Stlc.Store (P : Params) where

open Params P public

open import Arbor.Stlc.Node public using
  ( stlcSig; node; nvar; nlamT; napp; ntrue; nfalse; nif; nref; nTBool; nTArr
  ; StlcShape; svar; slamT; sapp; strue; sfalse; sif; sref; sTBool; sTArr )

open import Arbor.Generic.Store stlcSig hashModel public
  hiding (ClosedIn; WF; ClosedIn-mono)
import Arbor.Generic.Store stlcSig hashModel as G

open import Arbor.Stlc.Syntax Hash public using
  ( var; lamT; app; true; false; iff; ref; TBool; TArr
  ; IsTy; ty-bool; ty-arr; IsTm; tm-var; tm-lam; tm-app; tm-true; tm-false
  ; tm-if; tm-ref; ty-no-refs
  ; _⊢_; s-var; s-lam; s-app; s-true; s-false; s-if; s-ref; s-bool; s-arr
  ; Closed; ty-scoped )

------------------------------------------------------------------------
-- The two definitions that mention binding

ClosedIn : Store → Hash → Set
ClosedIn = G.ClosedIn Closed

WF : Store → Set
WF = G.WF Closed

module WF = G.WF

ClosedIn-mono : ∀ {σ σ′ h} → σ ⊑ σ′ → ClosedIn σ h → ClosedIn σ′ h
ClosedIn-mono = G.ClosedIn-mono

------------------------------------------------------------------------
-- prop:ty-ident, free
--
-- "For deep types T, U ingested at root hashes h_T, h_U: h_T = h_U iff T = U.
-- Hence structural type equality IS hash comparison for stored types — the
-- typed analogue of arbor-core's thm:alpha, by the same injectivity
-- induction." Here it is not an analogue but the very same theorem, since
-- types are terms of the one signature.

open import Data.Product using (_,_; _×_)
open import Relation.Binary.PropositionalEquality using (_≡_; cong)

prop-ty-ident : ∀ T U → (hashOf T ≡ hashOf U → T ≡ U)
                      × (T ≡ U → hashOf T ≡ hashOf U)
prop-ty-ident T U = hashOf-inj T U , cong hashOf
