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
open import Relation.Binary.PropositionalEquality using (_≡_; refl; cong)
open import Relation.Nullary.Decidable using (⌊_⌋)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

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

  -- Both the seed decision and the store lookup are ARGUMENTS, not `with`
  -- scrutinees. A `with` here compiles to an opaque generated function, and
  -- every property of ρ below would then be an ill-typed with-abstraction —
  -- which is exactly what happened on the first attempt. Passing `σ h` together
  -- with a proof that it is `σ h` is the inspect idiom, done by hand.
  mutual
    rho : (h : Hash) → Acc _≺_ h → Hash
    rho h a = rhoDec h a (h ≟ gold) (σ h) refl

    rhoDec : (h : Hash) → Acc _≺_ h → Dec (h ≡ gold) →
             (m : Maybe (Node Hash)) → σ h ≡ m → Hash
    rhoDec h a (yes _) _        _    = gnew          -- the seed
    rhoDec h a (no _)  nothing  _    = h             -- off dom(Σ): identity
    rhoDec h a (no _)  (just n) look = rhoAt h a (WF.recon-total wf (n , look))

    -- Rewrite h's reconstruction, then re-hash. ρ̂ is the guard on each
    -- reference leaf; structure is carried by substRefsD, which is why no
    -- combined-graph recursion is needed.
    rhoAt : (h : Hash) → Acc _≺_ h → Recon σ h → Hash
    rhoAt h (acc rs) (t , d) =
      hashOf (substRefsD t (λ r m → if guarded r then rho r (rs (t , d , m)) else r))

  -- ρ is a function, not merely a recipe
  --
  -- rhoAt takes an accessibility proof, so before any property of ρ's *values*
  -- can be stated, the value has to be shown independent of which proof was
  -- supplied. Acc is propositional up to funext, which --safe does not give us;
  -- this double induction establishes the instance that is needed without it.
  -- Note where substRefsD-cong is used: the two step functions agree only
  -- pointwise, never definitionally.
  mutual
    rho-irr : ∀ h (a₁ a₂ : Acc _≺_ h) → rho h a₁ ≡ rho h a₂
    rho-irr h a₁ a₂ = rhoDec-irr h a₁ a₂ (h ≟ gold) (σ h) refl

    rhoDec-irr : ∀ h (a₁ a₂ : Acc _≺_ h) (dec : Dec (h ≡ gold))
                 (m : Maybe (Node Hash)) (look : σ h ≡ m) →
                 rhoDec h a₁ dec m look ≡ rhoDec h a₂ dec m look
    rhoDec-irr h a₁ a₂ (yes _) _        _    = refl
    rhoDec-irr h a₁ a₂ (no _)  nothing  _    = refl
    rhoDec-irr h a₁ a₂ (no _)  (just n) look =
      rhoAt-irr h a₁ a₂ (WF.recon-total wf (n , look))

    rhoAt-irr : ∀ h (a₁ a₂ : Acc _≺_ h) (R : Recon σ h) →
                rhoAt h a₁ R ≡ rhoAt h a₂ R
    rhoAt-irr h (acc rs₁) (acc rs₂) (t , d) = cong hashOf (substRefsD-cong t agree)
      where
      agree : ∀ r (m : r ∈ refsT t) →
              (if guarded r then rho r (rs₁ (t , d , m)) else r)
            ≡ (if guarded r then rho r (rs₂ (t , d , m)) else r)
      agree r m with guarded r
      ... | false = refl
      ... | true  = rho-irr r (rs₁ (t , d , m)) (rs₂ (t , d , m))

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
-- Resolved since: ρ's independence from the accessibility proof (`rho-irr`), so
-- it is a function and not merely a recipe; `substRefsD-cong`, which that needs;
-- and `substRefsD-refs`, which says every reference of a rewritten term is the
-- image of a reference of the original — the provenance fact clause (iii) will
-- turn on.
--
-- Next step, and the one that turns ρ from a recursion into an equation:
--
--     ρ-at : ¬ (h ≡ gold) → σ ⊢ h ⇝ t →
--            ρ h ≡ hashOf (substRefsD t (λ r _ → if guarded r then ρ r else r))
--
-- All three ingredients are now present (rho-irr to discharge the accessibility
-- proof, ⇝-func to identify the reconstruction, substRefsD-cong to move between
-- the step functions). Everything downstream cites this rather than rho.
