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
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; ∃-syntax; proj₁; proj₂)
open import Induction.WellFounded using (Acc; acc)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; trans; cong; subst)
open import Arbor.Prelude using (nothing≢just)
open import Data.Empty using (⊥-elim)
open import Relation.Nullary.Decidable using (⌊_⌋)
open import Relation.Nullary using (¬_)
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

  -- Whether h is stored, as a DECISION.
  --
  -- rho branches on `h ≟ gold` and on this, and both are terms that occur in
  -- the goal whenever ρ does — so a proof about ρ can `with` on them. The
  -- obvious alternative, branching on `σ h` while carrying a proof that it is
  -- `σ h`, cannot: with-abstraction would have to abstract a term whose type
  -- mentions the term being abstracted. That is why the lookup is packaged as
  -- Dec (Recon σ h) rather than as a Maybe plus an equation.
  reconDec : (h : Hash) → Dec (Recon σ h)
  reconDec h with σ h in look
  ... | just n  = yes (WF.recon-total wf (n , look))
  ... | nothing = no λ R → ⊥-elim (nothing≢just
                    (trans (sym look) (proj₂ (⇝-∈dom (proj₂ R)))))

  mutual
    rho : (h : Hash) → Acc _≺_ h → Hash
    rho h a = rhoDec h a (h ≟ gold) (reconDec h)

    rhoDec : (h : Hash) → Acc _≺_ h → Dec (h ≡ gold) → Dec (Recon σ h) → Hash
    rhoDec h a (yes _) _         = gnew          -- the seed
    rhoDec h a (no _)  (no _)    = h             -- off dom(Σ): identity
    rhoDec h a (no _)  (yes R)   = rhoAt h a R

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
    rho-irr h a₁ a₂ = rhoDec-irr h a₁ a₂ (h ≟ gold) (reconDec h)

    rhoDec-irr : ∀ h (a₁ a₂ : Acc _≺_ h) (dec : Dec (h ≡ gold)) (rd : Dec (Recon σ h)) →
                 rhoDec h a₁ dec rd ≡ rhoDec h a₂ dec rd
    rhoDec-irr h a₁ a₂ (yes _) _        = refl
    rhoDec-irr h a₁ a₂ (no _)  (no _)   = refl
    rhoDec-irr h a₁ a₂ (no _)  (yes R)  = rhoAt-irr h a₁ a₂ R

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

  -- ρ̂, the scope-guarded reference map of def:cascade: reference edges
  -- propagate only into scope, structure always propagates.
  ρ̂ : Hash → Hash
  ρ̂ r = if guarded r then ρ r else r

  -- THE defining equation. Everything downstream cites this rather than rho:
  -- it says ρ at a stored non-seed hash is "rewrite the reconstruction's
  -- reference leaves by ρ̂, then re-hash", with no accessibility proof in sight.
  ρ-at : ∀ {h t} → ¬ (h ≡ gold) → σ ⊢ h ⇝ t →
         ρ h ≡ hashOf (substRefsD t (λ r _ → ρ̂ r))
  ρ-at {h} {t} ¬p d with h ≟ gold | reconDec h | WF.ref-acyclic wf h
  ... | yes p | _             | _      = ⊥-elim (¬p p)
  ... | no _  | no ¬R         | _      = ⊥-elim (¬R (t , d))
  ... | no _  | yes (t′ , d′) | acc rs with ⇝-func d′ d
  ...   | refl = cong hashOf (substRefsD-cong t agree)
    where
    agree : ∀ r (m : r ∈ refsT t) →
            (if guarded r then rho r (rs (t , d′ , m)) else r)
          ≡ (if guarded r then ρ r else r)
    agree r m with guarded r
    ... | false = refl
    ... | true  = rho-irr r (rs (t , d′ , m)) (WF.ref-acyclic wf r)

  -- The seed clause of def:cascade, discharged.
  ρ-seed : ρ gold ≡ gnew
  ρ-seed with gold ≟ gold
  ... | yes _  = refl
  ... | no ¬p  = ⊥-elim (¬p refl)
    where open import Data.Empty using (⊥-elim)

------------------------------------------------------------------------
-- Registering the image (def:cascade: "every node in the image of ρ# is
-- registered; Σ′ is Σ extended by them")
--
-- Registration is ingesting each entry's rewritten reconstruction. Ingest is
-- hash-consing, so re-registering a node already present is a no-op and the
-- fold may visit `support` in ANY order — see the note at the bottom on why
-- that is not obvious and what it changes.

  rewritten : (h : Hash) → Recon σ h → Term
  rewritten h (t , d) = substRefsD t (λ r _ → ρ̂ r)

  -- A rewritten term has the same scope as the original: substRefsD only
  -- exchanges reference leaves, which are scoped at every level.
  rewritten-closed : ∀ h (R : Recon σ h) → Closed (proj₁ R) → Closed (rewritten h R)
  rewritten-closed h (t , d) c = substRefsD-scoped t c

  registerOne : Store → Hash → Store
  registerOne s h with reconDec h
  ... | yes R = proj₁ (ingest s (rewritten h R))
  ... | no  _ = s

  registerAll : Store → List Hash → Store
  registerAll s []       = s
  registerAll s (h ∷ hs) = registerAll (registerOne s h) hs

  ------------------------------------------------------------------------
  -- Two of RewriteData's fields fall out, and neither depends on the order

  registerOne-keyed : ∀ s h → HashKeyed s → HashKeyed (registerOne s h)
  registerOne-keyed s h kd with reconDec h
  ... | yes R = ingest-keyed (rewritten h R) s kd
  ... | no  _ = kd

  registerOne-⊑ : ∀ s h → HashKeyed s → s ⊑ registerOne s h
  registerOne-⊑ s h kd with reconDec h
  ... | yes R = ingest-⊑ (rewritten h R) s kd
  ... | no  _ = ⊑-refl

  registerAll-keyed : ∀ s hs → HashKeyed s → HashKeyed (registerAll s hs)
  registerAll-keyed s []       kd = kd
  registerAll-keyed s (h ∷ hs) kd =
    registerAll-keyed (registerOne s h) hs (registerOne-keyed s h kd)

  registerAll-⊑ : ∀ s hs → HashKeyed s → s ⊑ registerAll s hs
  registerAll-⊑ s []       kd = ⊑-refl
  registerAll-⊑ s (h ∷ hs) kd =
    ⊑-trans (registerOne-⊑ s h kd)
            (registerAll-⊑ (registerOne s h) hs (registerOne-keyed s h kd))

  ------------------------------------------------------------------------
  -- wf (i) for a registration: every stored hash still reconstructs
  --
  -- Order-independent, because ingest-recon needs no premise about the term's
  -- references — only that the base store already reconstructs everywhere.

  registerOne-recon : ∀ s h → HashKeyed s → (∀ {g} → g ∈dom s → Recon s g) →
                      ∀ {g} → g ∈dom (registerOne s h) → Recon (registerOne s h) g
  registerOne-recon s h kd rec with reconDec h
  ... | yes R = ingest-recon (rewritten h R) s kd rec
  ... | no  _ = rec

  registerAll-recon : ∀ s hs → HashKeyed s → (∀ {g} → g ∈dom s → Recon s g) →
                      ∀ {g} → g ∈dom (registerAll s hs) → Recon (registerAll s hs) g
  registerAll-recon s []       kd rec = rec
  registerAll-recon s (h ∷ hs) kd rec =
    registerAll-recon (registerOne s h) hs
      (registerOne-keyed s h kd) (registerOne-recon s h kd rec)

  ------------------------------------------------------------------------
  -- The image is present
  --
  -- The enabling lemma for clauses (ii) and (iii): once h's image has been
  -- registered ANYWHERE in the fold, it is present in the final store and
  -- reconstructs to the rewritten term. Later steps only add, so nothing can
  -- undo it — which is precisely why the fold's order does not matter.

  rewritten-func : ∀ h (R₁ R₂ : Recon σ h) → rewritten h R₁ ≡ rewritten h R₂
  rewritten-func h (t₁ , d₁) (t₂ , d₂) with ⇝-func d₁ d₂
  ... | refl = refl

  registerOne-image : ∀ s h (R : Recon σ h) → HashKeyed s →
                      registerOne s h ⊢ hashOf (rewritten h R) ⇝ rewritten h R
  registerOne-image s h R kd with reconDec h
  ... | no ¬R = ⊥-elim (¬R R)
  ... | yes R′ rewrite rewritten-func h R′ R =
        subst (λ g → proj₁ (ingest s (rewritten h R)) ⊢ g ⇝ rewritten h R)
              (ingest-root (rewritten h R) s)
              (ingest-⇝ (rewritten h R) s kd)

  registerAll-image : ∀ s hs h (R : Recon σ h) → HashKeyed s → h ∈ hs →
                      registerAll s hs ⊢ hashOf (rewritten h R) ⇝ rewritten h R
  registerAll-image s (g ∷ hs) h R kd (here refl) =
    ⇝-mono (registerAll-⊑ (registerOne s h) hs (registerOne-keyed s h kd))
           (registerOne-image s h R kd)
  registerAll-image s (g ∷ hs) h R kd (there m) =
    registerAll-image (registerOne s g) hs h R (registerOne-keyed s g kd) m

  ------------------------------------------------------------------------
  -- Σ′, and the RewriteData fields discharged so far

  module _ (fin : Finite σ) where
    open Finite fin

    Σ′ : Store
    Σ′ = registerAll σ support

    Σ′-grows : HashKeyed σ → σ ⊑ Σ′
    Σ′-grows kd = registerAll-⊑ σ support kd

    Σ′-keyed : HashKeyed σ → HashKeyed Σ′
    Σ′-keyed kd = registerAll-keyed σ support kd

    Σ′-recon : HashKeyed σ → WF σ → ∀ {g} → g ∈dom Σ′ → Recon Σ′ g
    Σ′-recon kd w = registerAll-recon σ support kd (WF.recon-total w)

    -- Every entry of Σ that has an image has that image in Σ′.
    Σ′-image : HashKeyed σ → ∀ h (R : Recon σ h) → h ∈dom σ →
               Σ′ ⊢ hashOf (rewritten h R) ⇝ rewritten h R
    Σ′-image kd h R mem = registerAll-image σ support h R kd (complete mem)

    -- Every ρ̂-image of a closed entry is closed in Σ′.
    --
    -- This is the mathematical content of wf clause (ii) for the migrated
    -- store, and where the fold's order-independence is actually cashed: the
    -- image of r is in Σ′ because r is in dom(Σ) and therefore in `support`,
    -- NOT because it happened to be registered before whoever references it.
    --
    -- The seed decision is an ARGUMENT to `decide`, for the same reason it is
    -- one in rho: `with`-abstracting it would rewrite the goal past the point
    -- where ρ-seed and ρ-at apply, since both are stated about the unabstracted
    -- ρ. Generalizing r′ is what lets `refl` substitute at all — r itself is
    -- fixed by the enclosing clause.
    ρ-closed : HashKeyed σ → ClosedIn σ gnew →
               ∀ r → ClosedIn σ r → ClosedIn Σ′ (ρ r)
    ρ-closed kd cg r (t , d , ct) = decide r (r ≟ gold) d ct
      where
      decide : ∀ r′ → Dec (r′ ≡ gold) → ∀ {t′} → σ ⊢ r′ ⇝ t′ → Closed t′ →
               ClosedIn Σ′ (ρ r′)
      decide r′ (yes refl) d′ c′ =
        subst (ClosedIn Σ′) (sym ρ-seed) (ClosedIn-mono (Σ′-grows kd) cg)
      decide r′ (no ¬p) d′ c′ =
        subst (ClosedIn Σ′) (sym (ρ-at ¬p d′))
          ( rewritten r′ (_ , d′)
          , Σ′-image kd r′ (_ , d′) (⇝-∈dom d′)
          , rewritten-closed r′ (_ , d′) c′ )

    ρ̂-closed : HashKeyed σ → ClosedIn σ gnew →
               ∀ r → ClosedIn σ r → ClosedIn Σ′ (ρ̂ r)
    ρ̂-closed kd cg r cr with guarded r
    ... | false = ClosedIn-mono (Σ′-grows kd) cr
    ... | true  = ρ-closed kd cg r cr

    -- Every reference of a rewritten term is closed in Σ′: by substRefsD-refs
    -- it is the ρ̂-image of a reference of the original, and those were closed
    -- in Σ by wf (ii).
    rewritten-refs-closed :
      HashKeyed σ → ClosedIn σ gnew → WF σ →
      ∀ h (R : Recon σ h) → h ∈dom σ →
      ∀ {r} → r ∈ refsT (rewritten h R) → ClosedIn Σ′ r
    rewritten-refs-closed kd cg w h (t , d) mem memr with substRefsD-refs t memr
    ... | (r₀ , m₀ , eq) =
          subst (ClosedIn Σ′) eq (ρ̂-closed kd cg r₀ (WF.tgt-closed w mem (t , d , m₀)))

    ------------------------------------------------------------------------
    -- wf (ii) for Σ′
    --
    -- The fold carries the clause with its conclusion pinned to Σ′ throughout
    -- (ingest-tgt-big), which is what removes the order dependence: a step
    -- never has to know that the images it references were registered earlier,
    -- only that they are in Σ′ at the end.

    registerOne-tgt :
      ∀ s h → HashKeyed s →
      (∀ {g} → g ∈dom s → Recon s g) →
      (∀ {g r} → g ∈dom s → s ⊢ g ↝ r → ClosedIn Σ′ r) →
      ((R : Recon σ h) → ∀ {r} → r ∈ refsT (rewritten h R) → ClosedIn Σ′ r) →
      registerOne s h ⊑ Σ′ →
      ∀ {g r} → g ∈dom (registerOne s h) → registerOne s h ⊢ g ↝ r → ClosedIn Σ′ r
    registerOne-tgt s h kd rec tgt rcs sub with reconDec h
    ... | yes R = ingest-tgt-big (rewritten h R) s Σ′ kd rec tgt (rcs R) sub
    ... | no  _ = tgt

    registerAll-tgt :
      ∀ s hs → HashKeyed s →
      (∀ {g} → g ∈dom s → Recon s g) →
      (∀ {g r} → g ∈dom s → s ⊢ g ↝ r → ClosedIn Σ′ r) →
      (∀ h → h ∈ hs → (R : Recon σ h) →
             ∀ {r} → r ∈ refsT (rewritten h R) → ClosedIn Σ′ r) →
      registerAll s hs ⊑ Σ′ →
      ∀ {g r} → g ∈dom (registerAll s hs) →
      registerAll s hs ⊢ g ↝ r → ClosedIn Σ′ r
    registerAll-tgt s []       kd rec tgt rcs sub = tgt
    registerAll-tgt s (h ∷ hs) kd rec tgt rcs sub =
      registerAll-tgt (registerOne s h) hs
        (registerOne-keyed s h kd)
        (registerOne-recon s h kd rec)
        (registerOne-tgt s h kd rec tgt (rcs h (here refl))
          (⊑-trans (registerAll-⊑ (registerOne s h) hs (registerOne-keyed s h kd)) sub))
        (λ g m → rcs g (there m))
        sub

    Σ′-tgt : HashKeyed σ → WF σ → ClosedIn σ gnew →
             ∀ {g r} → g ∈dom Σ′ → Σ′ ⊢ g ↝ r → ClosedIn Σ′ r
    Σ′-tgt kd w cg =
      registerAll-tgt σ support kd (WF.recon-total w)
        (λ mem e → ClosedIn-mono (Σ′-grows kd) (WF.tgt-closed w mem e))
        (λ h _ R memr → rewritten-refs-closed kd cg w h R (⇝-∈dom (proj₂ R)) memr)
        ⊑-refl

------------------------------------------------------------------------
-- Where this stops, and why
--
-- Discharged so far, for a migration rewrite over a finite well-formed store:
-- ρ (by well-founded recursion on wf clause (iii)), its independence from the
-- accessibility proof (rho-irr), its defining equation (ρ-at) and seed clause
-- (ρ-seed); the registration fold and Σ′; and of RewriteData's six fields, ρ,
-- σ′, grows, keyed and seed. Only `wf′` is left.
--
-- FINDING (2026-08-07). An earlier note here — and open-questions.md — claimed
-- the fold must run in a DEPENDENCY ORDER, on the grounds that ρ is not
-- injective so acyclicity cannot transfer along it, and that the paper's
-- "referents are registered before referrers" argument (thm:wf's sketch) is
-- therefore load-bearing. **That is true of an incremental proof and false of
-- the construction.** Registering h's image needs h's rewritten references to
-- be closed *in the accumulator*, which does force an order; but nothing needs
-- to be proved at the accumulator. Every entry of Σ is in `support`, so every
-- image is registered somewhere in the fold, and ingest only ever adds — so the
-- reference targets are closed in Σ′ whatever order the fold ran in. That is
-- ρ̂-closed above, and it is why registerAll takes `support` as given rather
-- than a topological sort of it.
--
-- Consequences: no toposort is needed, and lem:cascade-order is NOT a
-- prerequisite for thm:migosc — it stays what the paper calls it, a bridge to
-- p11's sequential implementation, rather than machinery the construction
-- depends on.
--
-- What remains for `wf′`:
--
-- 1. Clause (ii) for Σ′. ρ̂-closed supplies the content; the plumbing is a
--    variant of Preservation.ingest-tgt whose CONCLUSION is stated in a fixed
--    larger store (with `ingest s t ⊑ big` as a premise) rather than in the
--    step store. The present ingest-tgt cannot be reused directly precisely
--    because its conclusion is incremental. Mechanical, ~80 lines.
--
-- 2. Clause (iii), acyclicity of Σ′. The argument, now that order is not
--    available: an edge out of a registered image goes, by substRefsD-refs, to
--    a ρ̂-image of a reference of the original — and if that reference was
--    guarded, its target is a σ-predecessor. So accessibility transfers along ρ
--    by induction on σ's Acc, without ρ being injective: what makes it work is
--    that the NODE at a colliding hash is the same node (by (★)), hence has the
--    same out-edges, so the argument never has to choose a preimage. Store's
--    acyclic-transfer is the wrong shape for this — it wants new edges to land
--    in the OLD domain, and these land in the image — so it needs a sibling
--    lemma keyed on ρ rather than on dom(Σ).
