{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The migration rewrite, over any signature (arbor-core def:cascade)
--
-- Arbor.Core.Rewrite, generically. Almost all of it ports unchanged, because
-- almost none of it is term-recursive: ρ recurses on accessibility, the
-- registration fold on the support list, and the acyclicity argument on
-- accessibility again. Only `rewritten` touches the term structure, and it does
-- so through substRefsD, which is already generic.
--
-- One property of the language is needed and cannot be derived: that rewriting
-- references preserves closedness. It is what lets the migrated store's
-- reference targets be shown closed without tracking the order in which images
-- were registered. For a language with binders it holds because a reference
-- leaf is scoped at every level.
------------------------------------------------------------------------

open import Arbor.Sig using (Sig)
open import Arbor.Hash using (HashModel)
import Arbor.Generic.Syntax as GS
import Arbor.Generic.Preservation as GP

module Arbor.Generic.Rewrite
  (Σg : Sig) (HM : HashModel (GS.Node Σg))
  (Closed : GS.WithHash.Term Σg (HashModel.Hash HM) → Set)
  (Closed-subst : ∀ t f → Closed t → Closed (GS.WithHash.substRefsD Σg (HashModel.Hash HM) t f))
  where

open import Arbor.Generic.Preservation Σg HM public
open WithClosed Closed public

open import Arbor.Hash using (HashModel)
open HashModel HM using (_≟_)

open import Data.Bool.Base using (Bool; true; false; if_then_else_; _∨_)
open import Data.Empty using (⊥-elim)
open import Data.List.Base using (List; []; _∷_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (Σ; _×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Induction.WellFounded using (Acc; acc)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; trans; cong; subst)
open import Relation.Nullary using (¬_)
open import Relation.Nullary.Decidable using (⌊_⌋)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)
open import Arbor.Prelude using (nothing≢just)

------------------------------------------------------------------------
-- Scope (def:strategy): a strategy induces a propagation scope.
--   Pin ↦ ∅,  Follow ↦ dom(Σ),  Explicit(S) ↦ S.
-- Consulted only where an edge crosses a reference or a name binding;
-- structure always propagates.

Scope : Set
Scope = Hash → Bool

pin : Scope
pin _ = false

follow : Scope
follow _ = true

explicit : (Hash → Bool) → Scope
explicit S = S

------------------------------------------------------------------------
-- What a migration rewrite must deliver (def:cascade + def:migrate's store
-- half). Constructed at the end of this module.

record RewriteData (σ : Store) (gold gnew : Hash) (sc : Scope) : Set where
  field
    ρ     : Hash → Hash
    σ′    : Store
    grows : σ ⊑ σ′
    keyed : HashKeyed σ → HashKeyed σ′
    seed  : ρ gold ≡ gnew
    wf′   : WF Closed σ → HashKeyed σ → WF Closed σ′

------------------------------------------------------------------------
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

module Rho (σ : Store) (wf : WF Closed σ) (gold gnew : Hash) (sc : Scope) where

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
  rewritten-closed h (t , d) c = Closed-subst t _ c

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
  registerOne-recon s h kd recT with reconDec h
  ... | yes R = ingest-recon (rewritten h R) s kd recT
  ... | no  _ = recT

  registerAll-recon : ∀ s hs → HashKeyed s → (∀ {g} → g ∈dom s → Recon s g) →
                      ∀ {g} → g ∈dom (registerAll s hs) → Recon (registerAll s hs) g
  registerAll-recon s []       kd recT = recT
  registerAll-recon s (h ∷ hs) kd recT =
    registerAll-recon (registerOne s h) hs
      (registerOne-keyed s h kd) (registerOne-recon s h kd recT)

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

    Σ′-recon : HashKeyed σ → WF Closed σ → ∀ {g} → g ∈dom Σ′ → Recon Σ′ g
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
    ρ-closed : HashKeyed σ → CIn σ gnew →
               ∀ r → CIn σ r → CIn Σ′ (ρ r)
    ρ-closed kd cg r (t , d , ct) = decide r (r ≟ gold) d ct
      where
      decide : ∀ r′ → Dec (r′ ≡ gold) → ∀ {t′} → σ ⊢ r′ ⇝ t′ → Closed t′ →
               CIn Σ′ (ρ r′)
      decide r′ (yes refl) d′ c′ =
        subst (CIn Σ′) (sym ρ-seed) (ClosedIn-mono (Σ′-grows kd) cg)
      decide r′ (no ¬p) d′ c′ =
        subst (CIn Σ′) (sym (ρ-at ¬p d′))
          ( rewritten r′ (_ , d′)
          , Σ′-image kd r′ (_ , d′) (⇝-∈dom d′)
          , rewritten-closed r′ (_ , d′) c′ )

    ρ̂-closed : HashKeyed σ → CIn σ gnew →
               ∀ r → CIn σ r → CIn Σ′ (ρ̂ r)
    ρ̂-closed kd cg r cr with guarded r
    ... | false = ClosedIn-mono (Σ′-grows kd) cr
    ... | true  = ρ-closed kd cg r cr

    -- Every reference of a rewritten term is closed in Σ′: by substRefsD-refs
    -- it is the ρ̂-image of a reference of the original, and those were closed
    -- in Σ by wf (ii).
    rewritten-refs-closed :
      HashKeyed σ → CIn σ gnew → WF Closed σ →
      ∀ h (R : Recon σ h) → h ∈dom σ →
      ∀ {r} → r ∈ refsT (rewritten h R) → CIn Σ′ r
    rewritten-refs-closed kd cg w h (t , d) mem memr with substRefsD-refs t memr
    ... | (r₀ , m₀ , eq) =
          subst (CIn Σ′) eq (ρ̂-closed kd cg r₀ (WF.tgt-closed w mem (t , d , m₀)))

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
      (∀ {g r} → g ∈dom s → s ⊢ g ↝ r → CIn Σ′ r) →
      ((R : Recon σ h) → ∀ {r} → r ∈ refsT (rewritten h R) → CIn Σ′ r) →
      registerOne s h ⊑ Σ′ →
      ∀ {g r} → g ∈dom (registerOne s h) → registerOne s h ⊢ g ↝ r → CIn Σ′ r
    registerOne-tgt s h kd recT tgt rcs sub with reconDec h
    ... | yes R = ingest-tgt-big (rewritten h R) s Σ′ kd recT tgt (rcs R) sub
    ... | no  _ = tgt

    registerAll-tgt :
      ∀ s hs → HashKeyed s →
      (∀ {g} → g ∈dom s → Recon s g) →
      (∀ {g r} → g ∈dom s → s ⊢ g ↝ r → CIn Σ′ r) →
      (∀ h → h ∈ hs → (R : Recon σ h) →
             ∀ {r} → r ∈ refsT (rewritten h R) → CIn Σ′ r) →
      registerAll s hs ⊑ Σ′ →
      ∀ {g r} → g ∈dom (registerAll s hs) →
      registerAll s hs ⊢ g ↝ r → CIn Σ′ r
    registerAll-tgt s []       kd recT tgt rcs sub = tgt
    registerAll-tgt s (h ∷ hs) kd recT tgt rcs sub =
      registerAll-tgt (registerOne s h) hs
        (registerOne-keyed s h kd)
        (registerOne-recon s h kd recT)
        (registerOne-tgt s h kd recT tgt (rcs h (here refl))
          (⊑-trans (registerAll-⊑ (registerOne s h) hs (registerOne-keyed s h kd)) sub))
        (λ g m → rcs g (there m))
        sub

    Σ′-tgt : HashKeyed σ → WF Closed σ → CIn σ gnew →
             ∀ {g r} → g ∈dom Σ′ → Σ′ ⊢ g ↝ r → CIn Σ′ r
    Σ′-tgt kd w cg =
      registerAll-tgt σ support kd (WF.recon-total w)
        (λ mem e → ClosedIn-mono (Σ′-grows kd) (WF.tgt-closed w mem e))
        (λ h _ R memr → rewritten-refs-closed kd cg w h R (⇝-∈dom (proj₂ R)) memr)
        ⊑-refl

    ------------------------------------------------------------------------
    -- wf (iii) for Σ′: acyclicity
    --
    -- "g's node was contributed by registering h": g reconstructs, in Σ′, to
    -- something whose references are among h's rewritten term's.
    CameFrom : Hash → Hash → Set
    CameFrom h g =
      ∃ λ (R : Recon σ h) →
      ∃ λ t′ → (Σ′ ⊢ g ⇝ t′)
             × (∀ {r} → r ∈ refsT t′ → r ∈ refsT (rewritten h R))

    registerOne-prov :
      ∀ s h → HashKeyed s → registerOne s h ⊑ Σ′ →
      ∀ {g} → g ∈dom (registerOne s h) →
      (g ∈dom s) ⊎ (∃ λ h′ → (h′ ∈dom σ) × CameFrom h′ g)
    registerOne-prov s h kd sub mem with reconDec h
    ... | no  _ = inj₁ mem
    ... | yes R with ingest-prov (rewritten h R) s kd mem
    ...   | inj₁ mσ                = inj₁ mσ
    ...   | inj₂ (t′ , d′ , sub′) =
            inj₂ (h , ⇝-∈dom (proj₂ R) , (R , t′ , ⇝-mono sub d′ , sub′))

    registerAll-prov :
      ∀ s hs → HashKeyed s → registerAll s hs ⊑ Σ′ →
      ∀ {g} → g ∈dom (registerAll s hs) →
      (g ∈dom s) ⊎ (∃ λ h′ → (h′ ∈dom σ) × CameFrom h′ g)
    registerAll-prov s []       kd sub mem = inj₁ mem
    registerAll-prov s (h ∷ hs) kd sub mem
      with registerAll-prov (registerOne s h) hs (registerOne-keyed s h kd) sub mem
    ... | inj₂ found = inj₂ found
    ... | inj₁ m₁    =
          registerOne-prov s h kd
            (⊑-trans (registerAll-⊑ (registerOne s h) hs (registerOne-keyed s h kd)) sub)
            m₁

    -- An old entry keeps exactly the edges it had: immutability plus
    -- functionality of ⇝ mean its reconstruction cannot change.
    accOld : HashKeyed σ → WF Closed σ → ∀ g → g ∈dom σ →
             Acc _≺_ g → Acc (λ b a → Σ′ ⊢ a ↝ b) g
    accOld kd w g mem (acc rs) = acc λ {y} e →
      let old = ⊑-edges (Σ′-grows kd) (WF.recon-total w mem) e
      in  accOld kd w y (ClosedIn→∈dom (WF.tgt-closed w mem old)) (rs old)

    -- An entry contributed by h's registration has its out-edges among the
    -- ρ̂-images of h's own references, so a guarded one lands on a
    -- σ-predecessor of h and the induction is on σ's accessibility of h.
    -- ρ's non-injectivity never arises: the argument follows the edge forward
    -- and never has to choose a preimage.
    mutual
      accFrom : HashKeyed σ → WF Closed σ → CIn σ gnew →
                ∀ h → h ∈dom σ → Acc _≺_ h →
                ∀ g → CameFrom h g → Acc (λ b a → Σ′ ⊢ a ↝ b) g
      accFrom kd w cg h memh (acc rs) g (R@(t , d) , t′ , d′ , sub) =
        acc λ { {x} (t″ , d″ , memx) →
          reach x (substRefsD-refs t
                     (sub (subst (λ ts → x ∈ refsT ts) (⇝-func d″ d′) memx))) }
        where
        -- ρ̂'s two halves. Out of scope: the reference is unchanged, so the
        -- target is an old entry. In scope: it is y's image, and y ≺ h.
        split : ∀ y → σ ⊢ h ↝ y → (b : Bool) → guarded y ≡ b →
                Acc (λ q p → Σ′ ⊢ p ↝ q) (if b then ρ y else y)
        split y ey false _ =
          accOld kd w y (ClosedIn→∈dom (WF.tgt-closed w memh ey)) (rs ey)
        split y ey true  _ =
          accImage kd w cg y (ClosedIn→∈dom (WF.tgt-closed w memh ey)) (rs ey)

        reach : ∀ x → (∃ λ y → Σ (y ∈ refsT t) (λ m → ρ̂ y ≡ x)) →
                Acc (λ b a → Σ′ ⊢ a ↝ b) x
        reach x (y , my , refl) = split y (t , d , my) (guarded y) refl

      -- y's image is contributed by y's own registration — unless y is the
      -- seed, whose image g_new was already in Σ.
      accImage : HashKeyed σ → WF Closed σ → CIn σ gnew →
                 ∀ y → y ∈dom σ → Acc _≺_ y →
                 Acc (λ b a → Σ′ ⊢ a ↝ b) (ρ y)
      accImage kd w cg y memy ay = decide y (y ≟ gold) memy ay
        where
        decide : ∀ y′ → Dec (y′ ≡ gold) → y′ ∈dom σ → Acc _≺_ y′ →
                 Acc (λ b a → Σ′ ⊢ a ↝ b) (ρ y′)
        decide y′ (yes refl) m a =
          subst (Acc (λ b a → Σ′ ⊢ a ↝ b)) (sym ρ-seed)
                (accOld kd w gnew (ClosedIn→∈dom cg) (WF.ref-acyclic w gnew))
        decide y′ (no ¬p) m a with WF.recon-total w m
        ... | R@(t , d) =
              subst (Acc (λ b a → Σ′ ⊢ a ↝ b)) (sym (ρ-at ¬p d))
                (accFrom kd w cg y′ m a (hashOf (rewritten y′ R))
                   (R , rewritten y′ R , Σ′-image kd y′ R m , λ z → z))

    Σ′-acyclic : HashKeyed σ → WF Closed σ → CIn σ gnew → Acyclic Σ′
    Σ′-acyclic kd w cg g = dispatch (Σ′ g) refl
      where
      dispatch : (m : Maybe (Node Hash)) → Σ′ g ≡ m →
                 Acc (λ b a → Σ′ ⊢ a ↝ b) g
      dispatch nothing eq = acc λ e →
        ⊥-elim (nothing≢just (trans (sym eq) (proj₂ (⇝-∈dom (proj₁ (proj₂ e))))))
      dispatch (just n) eq =
        found (registerAll-prov σ support kd ⊑-refl (n , eq))
        where
        found : (g ∈dom σ) ⊎ (∃ λ h → (h ∈dom σ) × CameFrom h g) →
                Acc (λ b a → Σ′ ⊢ a ↝ b) g
        found (inj₁ mσ)           = accOld kd w g mσ (WF.ref-acyclic w g)
        found (inj₂ (h , mh , c)) = accFrom kd w cg h mh (WF.ref-acyclic w h) g c

    ------------------------------------------------------------------------
    -- wf for Σ′, and a RewriteData inhabitant
    --
    -- def:cascade, constructed rather than specified. thm:migosc clause (c) is
    -- no longer a promise: the record now has a witness.

    Σ′-wf : HashKeyed σ → CIn σ gnew → WF Closed Σ′
    Σ′-wf kd cg = record
      { recon-total = Σ′-recon kd wf
      ; tgt-closed  = Σ′-tgt kd wf cg
      ; ref-acyclic = Σ′-acyclic kd wf cg
      }

    rewriteData : HashKeyed σ → CIn σ gnew → RewriteData σ gold gnew sc
    rewriteData kd cg = record
      { ρ     = ρ
      ; σ′    = Σ′
      ; grows = Σ′-grows kd
      ; keyed = λ kd′ → Σ′-keyed kd′
      ; seed  = ρ-seed
      ; wf′   = λ _ kd′ → Σ′-wf kd′ cg
      }

------------------------------------------------------------------------
-- M3, done — what was built and what it cost
--
-- The migration rewrite of def:cascade, CONSTRUCTED rather than specified:
-- ρ by well-founded recursion on wf clause (iii), its seed clause and defining
-- equation, the registration of its image, and all three wf clauses for the
-- migrated store. `rewriteData` assembles them into a RewriteData inhabitant
-- for any finite well-formed store, so thm:migosc clause (c) is no longer a
-- promise projected out of an empty record.
--
-- Two findings, both of which revised the plan that preceded them.
--
-- 1. THE FOLD NEEDS NO DEPENDENCY ORDER. The earlier plan (and
--    open-questions.md) held that registration must proceed referents-first,
--    since registering h's image needs h's rewritten references closed. That is
--    true of an INCREMENTAL proof and false of the construction: every entry of
--    Σ is in `support`, so every image is registered somewhere in the fold, and
--    ingest only ever adds — so the reference targets are closed in Σ′ whatever
--    order the fold ran in (ρ̂-closed). What made this expressible is
--    ingest-tgt-big, which pins the conclusion to the final store instead of
--    the accumulator. Consequence: no topological sort, and lem:cascade-order
--    is not a prerequisite for thm:migosc — it stays what the paper calls it, a
--    bridge to p11's sequential implementation.
--
-- 2. ρ'S NON-INJECTIVITY IS NOT THE OBSTACLE IT LOOKED LIKE. It is real —
--    distinct entries can rewrite to one hash, which is content-addressing
--    working as intended — and it does defeat the obvious argument, that
--    acyclicity transfers along ρ pointwise. But the accessibility proof never
--    has to choose a preimage: it follows an edge FORWARD, from an entry
--    contributed by h's registration to a ρ̂-image of one of h's own references,
--    and recurses on σ's accessibility of that reference. A colliding hash
--    carries the same node by (★), hence the same out-edges, so which preimage
--    one came from is not information the argument ever needs.
--
-- The one thing genuinely assumed rather than derived is finiteness, and only
-- registration uses it — hence `Finite` as a separate record rather than a
-- change to def:store, which would have rippled through every proof in the
-- development for the sake of one fold.
