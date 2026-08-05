{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Migration (arbor-core §sec:edit: def:strategy, def:substrefs, def:cascade,
-- def:deporder, def:migrate)
--
-- Without mints there is no unit of intent (rem:nosort), so propagation is a
-- WHOLE-STORE structural rewrite and names simply follow their hashes.
--
-- decisions.md 2026-08-05 (migration as a specified relation): the rewrite ρ
-- of def:cascade is defined "by recursion on the combined structural and
-- reference graph", well-founded by wf (i) + (iii). Constructing that
-- recursion in Agda is M3. What lands now is the *specification* it must
-- satisfy — RewriteData — so that the transition relation can be complete
-- from the start and M1's theorems are final rather than provisional. Every
-- M1 theorem needs exactly one field of it: `grows`.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Migrate (P : Params) where

open import Arbor.Core.History P public

open import Data.Bool.Base using (Bool; true; false)
open import Data.List.Base using (List; []; _∷_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; proj₁; proj₂)
open import Relation.Binary.PropositionalEquality using (_≡_)

------------------------------------------------------------------------
-- Scope (def:strategy)
--
-- A strategy induces a propagation scope P ⊆ dom(Σ):
--   Pin ↦ ∅,  Follow ↦ dom(Σ),  Explicit(S) ↦ S.
-- These are three points in the design's two-axis space (scope × user-in-
-- loop); Pin is the empty-scope instance of the same machinery, not a special
-- case. The scope is consulted only where an edge crosses a reference
-- (def:cascade) or a name binding (def:migrate); structure always propagates.

Scope : Set
Scope = Hash → Bool

pin : Scope
pin _ = false

follow : Scope
follow _ = true

explicit : (Hash → Bool) → Scope
explicit S = S

------------------------------------------------------------------------
-- Reference substitution (def:substrefs)
--
-- "ρ rewrites every ref h in t with h ∈ dom(ρ) to ref (ρ h), leaving all else
-- fixed."

substRefs : (Hash → Hash) → Term → Term
substRefs f (var i)   = var i
substRefs f (lam t)   = lam (substRefs f t)
substRefs f (app t u) = app (substRefs f t) (substRefs f u)
substRefs f (ref h)   = ref (f h)

------------------------------------------------------------------------
-- What a migration rewrite is (def:cascade + def:migrate's store half)
--
-- M3 replaces this record with a construction. Until then it is what the
-- Migrate transition cites, and the fields are exactly the paper's claims
-- about ρ that anything downstream uses.

record RewriteData (σ : Store) (gold gnew : Hash) (sc : Scope) : Set where
  field
    -- The rewrite itself, and the store extended by its image.
    ρ     : Hash → Hash
    σ′    : Store
    -- "Σ ⊆ Σ′": the store is accumulate-only. Every M1 theorem uses this and
    -- nothing else about migration.
    grows : σ ⊑ σ′
    -- Registering the ρ-image keys each new node by its own hash, as ingest
    -- does.
    keyed : HashKeyed σ → HashKeyed σ′
    -- The seed: ρ(g_old) = g_new.
    seed  : ρ gold ≡ gnew
    -- thm:migosc clause (c), Preservation: "entries that were not rewritten
    -- keep their original reference edges, so their meaning is unchanged", and
    -- the rewritten ones are registered referents-before-referrers. Included as
    -- a field for the same reason `grows` is: coherence preservation (thm:wf)
    -- quantifies over ALL transitions, Migrate included, so without it thm:wf
    -- could not be proved before M3. The obligation does not vanish — it
    -- transfers to M3's construction of an inhabitant, which must discharge
    -- every field at once.
    wf′   : WF σ → HashKeyed σ → WF σ′

------------------------------------------------------------------------
-- Atomic multi-rebind (def:migrate)
--
-- "Names follow their hashes, guarded by the same scope." The bundle U is
-- committed all-or-nothing: validation is a premise of the Migrate transition,
-- so a bundle that fails it produces no step at all, and ⟨N,H⟩ is untouched.
-- Σ, by contrast, may retain candidate hashes — only ⟨N,H⟩ is transactional.

Bundle : Set
Bundle = List (Name × Hash)

multiRebind : Namespace → History → Time → Bundle → Namespace × History
multiRebind ν η τ []             = (ν , η)
multiRebind ν η τ ((x , h) ∷ bs) =
  multiRebind (bindName ν x h) (appendEvent η x (just h , τ)) τ bs

------------------------------------------------------------------------
-- Dependency order (def:deporder)
--
-- "referents before referrers". By wf (iii) a dependency order always exists
-- (any topological sort of the acyclic reference graph restricted to K);
-- without (iii) it need not (rem:acyclic). Used only by lem:cascade-order,
-- the bridge to p11's sequential implementation — the rewrite itself is
-- order-free by construction.

-- "i occurs strictly before j": scanning the list we meet i, and j lies in
-- what remains.
data Before (i j : Hash) : List Hash → Set where
  hit  : ∀ {ks}    → j ∈ ks         → Before i j (i ∷ ks)
  skip : ∀ {k ks}  → Before i j ks  → Before i j (k ∷ ks)

DependencyOrder : Store → List Hash → Set
DependencyOrder σ ks =
  ∀ {i j} → i ∈ ks → j ∈ ks → σ ⊢ j ↝⁺ i → Before i j ks
