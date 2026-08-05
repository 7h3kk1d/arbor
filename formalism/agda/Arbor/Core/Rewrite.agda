{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- M3, in progress — constructing the migration rewrite ρ (arbor-core
-- def:cascade)
--
-- What lands here: finiteness as a separate assumption, and ρ itself, defined
-- by well-founded recursion exactly as def:cascade prescribes. What does NOT
-- land here yet: the registration of ρ's image into Σ′, and the proof that Σ′
-- is well-formed. See the header note at the bottom and ../open-questions.md.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Rewrite (P : Params) where

open import Arbor.Core.Preservation P public

open import Arbor.Hash using (HashModel)
open HashModel hashModel using (_≟_)

open import Data.Bool.Base using (Bool; true; false; if_then_else_; _∨_)
open import Data.List.Base using (List; []; _∷_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; ∃-syntax; proj₁; proj₂)
open import Induction.WellFounded using (Acc; acc)
open import Relation.Binary.PropositionalEquality using (_≡_; refl)
open import Relation.Nullary.Decidable using (⌊_⌋)
open import Relation.Nullary.Decidable.Core using (yes; no)

------------------------------------------------------------------------
-- Finiteness, as a separate assumption
--
-- The paper says Σ is a *finite* partial function, but only migration and the
-- decidability results use that. Rather than change def:store — which would
-- ripple through every proof in the development — finiteness is a predicate
-- assumed exactly where it is needed. Migration needs it because ρ's image has
-- to be *registered*, and a bare function cannot be enumerated.

record Finite (σ : Store) : Set where
  field
    support  : List Hash
    complete : ∀ {h} → h ∈dom σ → h ∈ support

------------------------------------------------------------------------
-- The rewrite (def:cascade)
--
--   ρ(g_old) = g_new,  otherwise  ρ(h) = ⌈ρ#(Σ(h))⌉
--
-- with ρ# mapping structural children by ρ and reference children by ρ̂, where
-- ρ̂(r) = ρ(r) for r ∈ P ∪ {g_old} and r otherwise.
--
-- The paper defines this "by recursion on the combined structural+reference
-- graph". Here the two halves are separated, which is what makes it definable
-- without a combined-graph well-foundedness argument: the structural half is
-- ordinary structural recursion on the *reconstruction* (a Term, via
-- substRefsD), and only the reference half needs well-foundedness — supplied by
-- wf clause (iii), already stated as well-foundedness of "is cited by", which
-- is precisely the shape this recursion wants.

module Rho (σ : Store) (wf : WF σ) (gold gnew : Hash) (sc : Scope) where

  -- r ≺ h : h cites r. wf (iii) says this is well-founded.
  _≺_ : Hash → Hash → Set
  r ≺ h = σ ⊢ h ↝ r

  -- ρ̂'s guard: "structural edges always propagate, reference edges only into
  -- scope" — plus the seed itself, which propagates under every strategy.
  guarded : Hash → Bool
  guarded r = sc r ∨ ⌊ r ≟ gold ⌋

  mutual
    rho : (h : Hash) → Acc _≺_ h → Hash
    rho h a with h ≟ gold
    ... | yes _ = gnew
    ... | no  _ with σ h in look
    ...   | nothing = h                     -- off dom(Σ): ρ is the identity
    ...   | just n  = rhoAt h a (WF.recon-total wf (n , look))

    -- Rewrite h's reconstruction, then re-hash. Registration of the image is
    -- the part that still needs Finite σ; see below.
    rhoAt : (h : Hash) → Acc _≺_ h → Recon σ h → Hash
    rhoAt h (acc rs) (t , d) = hashOf (substRefsD t step)
      where
      step : ∀ r → r ∈ refsT t → Hash
      step r mem = if guarded r then rho r (rs (t , d , mem)) else r

  -- ρ as a function on hashes, using wf (iii) to supply accessibility.
  ρ : Hash → Hash
  ρ h = rho h (WF.ref-acyclic wf h)

  -- The seed clause of def:cascade, discharged.
  ρ-seed : ρ gold ≡ gnew
  ρ-seed with gold ≟ gold
  ... | yes _  = refl
  ... | no ¬p  = ⊥-elim (¬p refl)
    where open import Data.Empty using (⊥-elim)

------------------------------------------------------------------------
-- Where this stops, and why
--
-- ρ is defined and its seed clause holds. Two things remain before a
-- RewriteData inhabitant exists, i.e. before thm:migosc stops being discharged
-- from a specification:
--
-- 1. REGISTRATION. Σ′ must extend Σ by every node in the image of ρ#. That is a
--    fold over `Finite.support`, plus a proof that the fold's result is
--    independent of the support list chosen (any two supports agree on dom(Σ),
--    and hashes outside it contribute nothing). Mechanical but not short.
--
-- 2. WELL-FORMEDNESS OF Σ′ — the real obstacle. Clauses (i) and (ii) follow the
--    shape of Preservation.ingest-recon / ingest-tgt. Clause (iii) does not:
--    acyclicity has to be re-established for the rewritten reference graph, and
--    **ρ is not injective**, so acyclicity does not transfer along it. Two
--    distinct entries can rewrite to the same hash, which is exactly the point
--    of content-addressing, and a cycle among ρ-images therefore need not pull
--    back to a cycle in Σ. The paper's argument is the registration ORDER —
--    "ordering new nodes by registration extends any topological order of the
--    old reference graph" (thm:wf's sketch) — so the fold in (1) has to be
--    performed in a dependency order (def:deporder) and the proof has to read
--    that order back out. That is the same machinery lem:cascade-order is
--    about, which is why open-questions.md predicts the two landing together.
--
-- Also unresolved: ρ takes an accessibility proof, so it is a function only up
-- to `rho h a₁ ≡ rho h a₂`, provable by double Acc induction but not yet done.
-- Nothing above depends on it; the first proof about ρ's *values* will.
