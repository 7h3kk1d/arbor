{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- The term store, over any signature (arbor-core §sec:store)
--
-- Arbor.Core.Store, generically: the store, ingest, reconstruction, references
-- and well-formedness, none of which depend on the language. Binding does, so
-- closedness enters as a parameter — wf clause (ii) is stated in terms of it
-- and the signature does not model binders.
------------------------------------------------------------------------

open import Arbor.Sig using (Sig)
open import Arbor.Hash using (HashModel)
import Arbor.Generic.Syntax as GS

module Arbor.Generic.Store
  (Σg : Sig) (HM : HashModel (GS.Node Σg)) where

open HashModel HM public using (Hash; hash; hash-inj)
open HashModel HM using (_≟_)

open GS Σg public using (Shape; sArity; rArity; Node; node; node-shape; node-schil; node-rchil)
open GS.WithHash Σg Hash public

open import Arbor.Prelude using (just-inj)
import Arbor.Prelude
module M = Arbor.Prelude.Map _≟_
open M public using
  ( PMap; ∅; _[_↦_]; _∖_; _∈dom_; _∈ran_; _⊑_
  ; ⊑-refl; ⊑-trans; Undisturbed; ⊑-update
  ; lookup-hit; lookup-miss; remove-hit; remove-miss )

open import Data.List.Base using (List; []; _++_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Data.Vec.Base using (Vec; toList) renaming ([] to ⟦⟧; _∷_ to _◂_)
open import Induction.WellFounded using (WellFounded; Acc; acc)
open import Relation.Binary.PropositionalEquality
  using (_≡_; refl; sym; trans; cong; cong₂; subst)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

------------------------------------------------------------------------
-- The store (def:store)

Store : Set
Store = PMap (Node Hash)

------------------------------------------------------------------------
-- The hash a term ingests to (def:ingest), store-independently

mutual
  hashOf : Term → Hash
  hashOf ⟨ s , ss , rs ⟩ = hash (node s (hashOfV ss) rs)

  hashOfV : ∀ {n} → Vec Term n → Vec Hash n
  hashOfV ⟦⟧       = ⟦⟧
  hashOfV (t ◂ ts) = hashOf t ◂ hashOfV ts

------------------------------------------------------------------------
-- Ingest (def:ingest): register bottom-up, keyed by hash. Content-addressing
-- makes this hash-consing. Reference children are NOT ingested — they name
-- already-stored definitions.

mutual
  ingest : Store → Term → Store × Hash
  ingest σ ⟨ s , ss , rs ⟩ =
    let (σ′ , hs) = ingestV σ ss
        n         = node s hs rs
    in  (σ′ [ hash n ↦ n ] , hash n)

  ingestV : ∀ {n} → Store → Vec Term n → Store × Vec Hash n
  ingestV σ ⟦⟧       = (σ , ⟦⟧)
  ingestV σ (t ◂ ts) =
    let (σ₁ , h)  = ingest σ t
        (σ₂ , hs) = ingestV σ₁ ts
    in  (σ₂ , h ◂ hs)

mutual
  ingest-root : ∀ t σ → proj₂ (ingest σ t) ≡ hashOf t
  ingest-root ⟨ s , ss , rs ⟩ σ =
    cong (λ hs → hash (node s hs rs)) (ingestV-roots ss σ)

  ingestV-roots : ∀ {n} (ts : Vec Term n) σ → proj₂ (ingestV σ ts) ≡ hashOfV ts
  ingestV-roots ⟦⟧       σ = refl
  ingestV-roots (t ◂ ts) σ =
    cong₂ _◂_ (ingest-root t σ) (ingestV-roots ts (proj₁ (ingest σ t)))

------------------------------------------------------------------------
-- Reconstruction (def:ingest), relationally
--
-- Structural children are related POINTWISE, which needs its own relation over
-- Vec. That companion is where the generic version differs in shape from the
-- concrete one — and it is what makes ⇝-func eight lines instead of sixteen
-- cases, since every "these two constructors differ" case collapses into one
-- shape equality.

infix 4 _⊢_⇝_
mutual
  data _⊢_⇝_ (σ : Store) : Hash → Term → Set where
    rec : ∀ {h s} {ss : Vec Hash (sArity s)} {rs} {ts} →
          σ h ≡ just (node s ss rs) →
          σ ⊢ ss ⇝* ts →
          σ ⊢ h ⇝ ⟨ s , ts , rs ⟩

  data _⊢_⇝*_ (σ : Store) : ∀ {n} → Vec Hash n → Vec Term n → Set where
    []  : σ ⊢ ⟦⟧ ⇝* ⟦⟧
    _∷_ : ∀ {n h t} {hs : Vec Hash n} {ts} →
          σ ⊢ h ⇝ t → σ ⊢ hs ⇝* ts → σ ⊢ (h ◂ hs) ⇝* (t ◂ ts)

Recon : Store → Hash → Set
Recon σ h = ∃[ t ] (σ ⊢ h ⇝ t)

ClosedIn : (Term → Set) → Store → Hash → Set
ClosedIn Closed σ h = ∃[ t ] ((σ ⊢ h ⇝ t) × Closed t)

------------------------------------------------------------------------
-- Functionality and monotonicity of reconstruction

mutual
  ⇝-func : ∀ {σ h t u} → σ ⊢ h ⇝ t → σ ⊢ h ⇝ u → t ≡ u
  ⇝-func (rec e₁ c₁) (rec e₂ c₂) with just-inj (trans (sym e₁) e₂)
  ... | refl = cong (λ ts → ⟨ _ , ts , _ ⟩) (⇝*-func c₁ c₂)

  ⇝*-func : ∀ {σ n} {hs : Vec Hash n} {ts us} →
            σ ⊢ hs ⇝* ts → σ ⊢ hs ⇝* us → ts ≡ us
  ⇝*-func []        []        = refl
  ⇝*-func (d₁ ∷ c₁) (d₂ ∷ c₂) = cong₂ _◂_ (⇝-func d₁ d₂) (⇝*-func c₁ c₂)

⇝-∈dom : ∀ {σ h t} → σ ⊢ h ⇝ t → h ∈dom σ
⇝-∈dom (rec e _) = _ , e

mutual
  ⇝-mono : ∀ {σ σ′ h t} → σ ⊑ σ′ → σ ⊢ h ⇝ t → σ′ ⊢ h ⇝ t
  ⇝-mono sub (rec e c) = rec (sub _ _ e) (⇝*-mono sub c)

  ⇝*-mono : ∀ {σ σ′ n} {hs : Vec Hash n} {ts} →
            σ ⊑ σ′ → σ ⊢ hs ⇝* ts → σ′ ⊢ hs ⇝* ts
  ⇝*-mono sub []       = []
  ⇝*-mono sub (d ∷ c)  = ⇝-mono sub d ∷ ⇝*-mono sub c

ClosedIn-mono : ∀ {Closed σ σ′ h} → σ ⊑ σ′ →
                ClosedIn Closed σ h → ClosedIn Closed σ′ h
ClosedIn-mono sub (t , d , c) = t , ⇝-mono sub d , c

------------------------------------------------------------------------
-- Hashing collapses exactly α-equivalence (thm:alpha)
--
-- Again shorter generically: injectivity of ⌈·⌉ gives node equality, record
-- injectivity gives equal shapes and equal child vectors, and the vector case
-- is one induction rather than a case per constructor pair.

mutual
  hashOf-inj : ∀ t u → hashOf t ≡ hashOf u → t ≡ u
  hashOf-inj ⟨ s , ss , rs ⟩ ⟨ s′ , ss′ , rs′ ⟩ e with node-shape (hash-inj e)
  ... | refl = cong₂ (λ ts qs → ⟨ s , ts , qs ⟩)
                     (hashOfV-inj ss ss′ (node-schil (hash-inj e)))
                     (node-rchil (hash-inj e))

  hashOfV-inj : ∀ {n} (ts us : Vec Term n) → hashOfV ts ≡ hashOfV us → ts ≡ us
  hashOfV-inj ⟦⟧       ⟦⟧       e = refl
  hashOfV-inj (t ◂ ts) (u ◂ us) e = cong₂ _◂_ (hashOf-inj t u (head≡ e)) (hashOfV-inj ts us (tail≡ e))
    where
    head≡ : ∀ {n} {a b : Hash} {xs ys : Vec Hash n} → (a ◂ xs) ≡ (b ◂ ys) → a ≡ b
    head≡ refl = refl
    tail≡ : ∀ {n} {a b : Hash} {xs ys : Vec Hash n} → (a ◂ xs) ≡ (b ◂ ys) → xs ≡ ys
    tail≡ refl = refl

------------------------------------------------------------------------
-- References of a stored hash, callers, well-formedness (def:refs, def:wf)

infix 4 _⊢_↝_
_⊢_↝_ : Store → Hash → Hash → Set
σ ⊢ h ↝ r = ∃[ t ] ((σ ⊢ h ⇝ t) × (r ∈ refsT t))

Callers : Store → Hash → Hash → Set
Callers σ h g = σ ⊢ g ↝ h

infix 4 _⊢_↝⁺_
data _⊢_↝⁺_ (σ : Store) : Hash → Hash → Set where
  tc-one  : ∀ {h r}   → σ ⊢ h ↝ r → σ ⊢ h ↝⁺ r
  tc-more : ∀ {h m r} → σ ⊢ h ↝ m → σ ⊢ m ↝⁺ r → σ ⊢ h ↝⁺ r

Acyclic : Store → Set
Acyclic σ = WellFounded (λ r h → σ ⊢ h ↝ r)

record WF (Closed : Term → Set) (σ : Store) : Set where
  field
    recon-total : ∀ {h} → h ∈dom σ → Recon σ h
    tgt-closed  : ∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → ClosedIn Closed σ r
    ref-acyclic : Acyclic σ

------------------------------------------------------------------------
-- Transfer lemmas for a growing store

update-dom : ∀ {σ : Store} n {h} → h ∈dom (σ [ hash n ↦ n ]) →
             (h ≡ hash n) ⊎ (h ∈dom σ)
update-dom {σ} n {h} mem = go h (h ≟ hash n) (proj₂ mem)
  where
  go : ∀ h″ → Dec (h″ ≡ hash n) → (σ [ hash n ↦ n ]) h″ ≡ just (proj₁ mem) →
       (h″ ≡ hash n) ⊎ (h″ ∈dom σ)
  go _  (yes eq) _ = inj₁ eq
  go h″ (no ¬p)  e =
    inj₂ (proj₁ mem , trans (sym (lookup-miss σ {hash n} {h″} n ¬p)) e)

⊑-edges : ∀ {σ σ′ h r} → σ ⊑ σ′ → Recon σ h → σ′ ⊢ h ↝ r → σ ⊢ h ↝ r
⊑-edges sub (t , d) (t′ , d′ , mem) =
  t , d , subst (λ ts → _ ∈ refsT ts) (⇝-func d′ (⇝-mono sub d)) mem

acyclic-transfer :
  ∀ {σ σ′} → σ ⊑ σ′ →
  (∀ {h} → h ∈dom σ → Recon σ h) →
  (∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → r ∈dom σ) →
  Acyclic σ →
  (∀ {h r} → σ′ ⊢ h ↝ r → (h ∈dom σ) ⊎ (r ∈dom σ)) →
  Acyclic σ′
acyclic-transfer {σ} {σ′} sub recT tgt acy back h = acc acc-step
  where
  lift : ∀ {x} → x ∈dom σ → Acc (λ r g → σ ⊢ g ↝ r) x → Acc (λ r g → σ′ ⊢ g ↝ r) x
  lift {x} mx (acc rs) = acc λ {r} edge →
    let old = ⊑-edges sub (recT mx) edge
    in  lift (tgt mx old) (rs old)

  acc-step : ∀ {r} → σ′ ⊢ h ↝ r → Acc (λ y g → σ′ ⊢ g ↝ y) r
  acc-step {r} edge with back edge
  ... | inj₁ mh = lift (tgt mh (⊑-edges sub (recT mh) edge)) (acy r)
  ... | inj₂ mr = lift mr (acy r)

------------------------------------------------------------------------
-- The store is keyed by hash (see Arbor.Core.Store's note: thm:mono's proof
-- needs this and def:wf does not supply it)

HashKeyed : Store → Set
HashKeyed σ = ∀ h n → σ h ≡ just n → h ≡ hash n

keyed-undisturbed : ∀ {σ} → HashKeyed σ → ∀ n → Undisturbed σ (hash n) n
keyed-undisturbed {σ} kd n with σ (hash n) in eq
... | nothing = inj₁ refl
... | just n′ = inj₂ (cong just (sym (hash-inj (kd (hash n) n′ eq))))

keyed-update : ∀ {σ} → HashKeyed σ → ∀ n → HashKeyed (σ [ hash n ↦ n ])
keyed-update {σ} kd n h n′ eq = go h (h ≟ hash n) eq
  where
  go : ∀ h″ → Dec (h″ ≡ hash n) → (σ [ hash n ↦ n ]) h″ ≡ just n′ → h″ ≡ hash n′
  go h″ (no ¬p)   e = kd h″ n′ (trans (sym (lookup-miss σ {hash n} {h″} n ¬p)) e)
  go _  (yes refl) e =
    cong hash (just-inj (trans (sym (lookup-hit σ (hash n) n)) e))

------------------------------------------------------------------------
-- Ingest grows the store, preserves its keying, and round-trips

mutual
  ingest-keyed : ∀ t σ → HashKeyed σ → HashKeyed (proj₁ (ingest σ t))
  ingest-keyed ⟨ s , ss , rs ⟩ σ kd =
    keyed-update (ingestV-keyed ss σ kd) _

  ingestV-keyed : ∀ {n} (ts : Vec Term n) σ → HashKeyed σ →
                  HashKeyed (proj₁ (ingestV σ ts))
  ingestV-keyed ⟦⟧       σ kd = kd
  ingestV-keyed (t ◂ ts) σ kd =
    ingestV-keyed ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd)

mutual
  ingest-⊑ : ∀ t σ → HashKeyed σ → σ ⊑ proj₁ (ingest σ t)
  ingest-⊑ ⟨ s , ss , rs ⟩ σ kd =
    ⊑-trans (ingestV-⊑ ss σ kd)
            (⊑-update (keyed-undisturbed (ingestV-keyed ss σ kd) _))

  ingestV-⊑ : ∀ {n} (ts : Vec Term n) σ → HashKeyed σ →
              σ ⊑ proj₁ (ingestV σ ts)
  ingestV-⊑ ⟦⟧       σ kd = ⊑-refl
  ingestV-⊑ (t ◂ ts) σ kd =
    ⊑-trans (ingest-⊑ t σ kd)
            (ingestV-⊑ ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd))

-- Ingesting a term and reconstructing its root gives the term back.
mutual
  ingest-⇝ : ∀ t σ → HashKeyed σ → proj₁ (ingest σ t) ⊢ proj₂ (ingest σ t) ⇝ t
  ingest-⇝ ⟨ s , ss , rs ⟩ σ kd =
    rec (lookup-hit (proj₁ (ingestV σ ss)) _ _)
        (⇝*-mono (⊑-update (keyed-undisturbed (ingestV-keyed ss σ kd) _))
                 (ingestV-⇝ ss σ kd))

  ingestV-⇝ : ∀ {n} (ts : Vec Term n) σ → HashKeyed σ →
              proj₁ (ingestV σ ts) ⊢ proj₂ (ingestV σ ts) ⇝* ts
  ingestV-⇝ ⟦⟧       σ kd = []
  ingestV-⇝ (t ◂ ts) σ kd =
    ⇝-mono (ingestV-⊑ ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd))
           (ingest-⇝ t σ kd)
    ∷ ingestV-⇝ ts (proj₁ (ingest σ t)) (ingest-keyed t σ kd)
