{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part II — the term store (arbor-core §sec:store)
--
-- The store (def:store), ingest and reconstruct (def:ingest), references
-- (def:refs), and well-formedness (def:wf).
--
-- decisions.md 2026-08-05 (partial functions as relations): the paper's
-- reconstruct : Hash ⇀ Term is *partial*, and as an Agda function it would
-- need a termination argument that does not exist in general (a store whose
-- structural children loop is a perfectly good value of type Store). So
-- reconstruct is an inductive relation σ ⊢ h ⇝ t, with functionality
-- (⇝-func) recovering the "partial function" reading. This is the move the
-- paper already made for printing (def:print) and evaluation (def:eval); it
-- costs nothing and removes the only place --safe would have needed fuel.
--
-- Finding (2026-08-05, not in the paper): thm:mono's proof argues "each key
-- is ⌈n⌉ for the very node n stored, so any collision on a key is with an
-- identical node". That step needs to know the store *is* keyed by hash,
-- which no clause of def:wf states. It is isolated below as HashKeyed and
-- carried explicitly; see open-questions.md.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Store (P : Params) where

open Params P public

open import Arbor.Core.Node public using
  ( Node; nvar; nlam; napp; nref
  ; nvar-inj; nlam-inj; napp-inj; nref-inj )
open import Arbor.Hash using (HashModel)
open HashModel hashModel public using (Hash; hash; hash-inj)
open HashModel hashModel using (_≟_)

open import Arbor.Prelude using (just-inj)
open import Arbor.Prelude
module M = Arbor.Prelude.Map _≟_
open M public using
  ( PMap; ∅; _[_↦_]; _∖_; _∈dom_; _∈ran_; _⊑_
  ; ⊑-refl; ⊑-trans; Undisturbed; ⊑-update
  ; lookup-hit; lookup-miss; remove-hit; remove-miss )

open import Arbor.Core.Syntax Hash public

open import Data.List.Membership.Propositional using (_∈_)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Induction.WellFounded using (WellFounded; Acc; acc)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; trans; cong; cong₂; subst)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

------------------------------------------------------------------------
-- The store (def:store)

Store : Set
Store = PMap (Node Hash)

------------------------------------------------------------------------
-- Ingest (def:ingest)
--
-- Registers a deep term bottom-up, keying each node by its hash.
-- Content-addressing makes this hash-consing: re-registering an existing
-- node is a no-op. Ingesting `ref h` does NOT ingest h — the referent is an
-- already-stored definition, cited by hash.

ingest : Store → Term → Store × Hash
ingest σ (var i) = (σ [ hash (nvar i) ↦ nvar i ] , hash (nvar i))
ingest σ (lam t) =
  let (σ₁ , h) = ingest σ t
      n        = nlam h
  in  (σ₁ [ hash n ↦ n ] , hash n)
ingest σ (app t u) =
  let (σ₁ , h₁) = ingest σ  t
      (σ₂ , h₂) = ingest σ₁ u
      n         = napp h₁ h₂
  in  (σ₂ [ hash n ↦ n ] , hash n)
ingest σ (ref h) = (σ [ hash (nref h) ↦ nref h ] , hash (nref h))

-- The root hash a term ingests to does not depend on the store it lands in.
hashOf : Term → Hash
hashOf (var i)   = hash (nvar i)
hashOf (lam t)   = hash (nlam (hashOf t))
hashOf (app t u) = hash (napp (hashOf t) (hashOf u))
hashOf (ref h)   = hash (nref h)

ingest-root : ∀ t σ → proj₂ (ingest σ t) ≡ hashOf t
ingest-root (var i)   σ = refl
ingest-root (lam t)   σ = cong (λ h → hash (nlam h)) (ingest-root t σ)
ingest-root (app t u) σ =
  cong₂′ (ingest-root t σ) (ingest-root u (proj₁ (ingest σ t)))
  where
  cong₂′ : ∀ {h₁ h₂ g₁ g₂} → h₁ ≡ g₁ → h₂ ≡ g₂ →
           hash (napp h₁ h₂) ≡ hash (napp g₁ g₂)
  cong₂′ refl refl = refl
ingest-root (ref h)   σ = refl

------------------------------------------------------------------------
-- Reconstruct (def:ingest), as a relation
--
-- "reconstruct(Σ,h) follows Σ(h), recursing into nlam/napp children and
-- returning Var i / ref h′ unchanged; it is undefined on h ∉ dom(Σ)."

infix 4 _⊢_⇝_
data _⊢_⇝_ (σ : Store) : Hash → Term → Set where
  r-var : ∀ {h i}         → σ h ≡ just (nvar i)     → σ ⊢ h ⇝ var i
  r-lam : ∀ {h c t}       → σ h ≡ just (nlam c)     → σ ⊢ c ⇝ t →
                            σ ⊢ h ⇝ lam t
  r-app : ∀ {h c₁ c₂ t u} → σ h ≡ just (napp c₁ c₂) → σ ⊢ c₁ ⇝ t → σ ⊢ c₂ ⇝ u →
                            σ ⊢ h ⇝ app t u
  r-ref : ∀ {h r}         → σ h ≡ just (nref r)     → σ ⊢ h ⇝ ref r

-- "reconstruct is defined at h"
Recon : Store → Hash → Set
Recon σ h = ∃[ t ] (σ ⊢ h ⇝ t)

-- The paper's closed_Σ(h): "h ∈ dom(Σ) and closed(reconstruct(Σ,h))".
ClosedIn : Store → Hash → Set
ClosedIn σ h = ∃[ t ] ((σ ⊢ h ⇝ t) × Closed t)

private
  -- Two lookups of the same hash agree. Generalized over the looked-up value
  -- rather than over (σ , h): an implicit `σ h` is not a Miller pattern, so
  -- Agda cannot invert it at the call sites below.
  same : ∀ {m : Maybe (Node Hash)} {n₁ n₂} → m ≡ just n₁ → m ≡ just n₂ → n₁ ≡ n₂
  same e₁ e₂ = just-inj (trans (sym e₁) e₂)

-- Functionality: this is what makes ⇝ a partial *function*, i.e. justifies
-- the paper writing reconstruct(Σ,h) as a term.
⇝-func : ∀ {σ h t u} → σ ⊢ h ⇝ t → σ ⊢ h ⇝ u → t ≡ u
⇝-func (r-var e₁)     (r-var e₂)     = cong var (nvar-inj (same e₁ e₂))
⇝-func (r-var e₁)     (r-lam e₂ _)   with same e₁ e₂
...                                     | ()
⇝-func (r-var e₁)     (r-app e₂ _ _) with same e₁ e₂
...                                     | ()
⇝-func (r-var e₁)     (r-ref e₂)     with same e₁ e₂
...                                     | ()
⇝-func (r-lam e₁ _)   (r-var e₂)     with same e₁ e₂
...                                     | ()
⇝-func (r-lam e₁ d₁)  (r-lam e₂ d₂)  with nlam-inj (same e₁ e₂)
...                                     | refl = cong lam (⇝-func d₁ d₂)
⇝-func (r-lam e₁ _)   (r-app e₂ _ _) with same e₁ e₂
...                                     | ()
⇝-func (r-lam e₁ _)   (r-ref e₂)     with same e₁ e₂
...                                     | ()
⇝-func (r-app e₁ _ _) (r-var e₂)     with same e₁ e₂
...                                     | ()
⇝-func (r-app e₁ _ _) (r-lam e₂ _)   with same e₁ e₂
...                                     | ()
⇝-func (r-app e₁ a₁ b₁) (r-app e₂ a₂ b₂) with same e₁ e₂
...                                     | refl = cong₂ app (⇝-func a₁ a₂) (⇝-func b₁ b₂)
⇝-func (r-app e₁ _ _) (r-ref e₂)     with same e₁ e₂
...                                     | ()
⇝-func (r-ref e₁)     (r-var e₂)     with same e₁ e₂
...                                     | ()
⇝-func (r-ref e₁)     (r-lam e₂ _)   with same e₁ e₂
...                                     | ()
⇝-func (r-ref e₁)     (r-app e₂ _ _) with same e₁ e₂
...                                     | ()
⇝-func (r-ref e₁)     (r-ref e₂)     = cong ref (nref-inj (same e₁ e₂))

-- A reconstruction witnesses that its root is stored.
⇝-∈dom : ∀ {σ h t} → σ ⊢ h ⇝ t → h ∈dom σ
⇝-∈dom (r-var e)     = _ , e
⇝-∈dom (r-lam e _)   = _ , e
⇝-∈dom (r-app e _ _) = _ , e
⇝-∈dom (r-ref e)     = _ , e

-- Monotonicity of reconstruction under store growth. This is the workhorse
-- of thm:stability: "reconstruct and ⇓ read only entries at hashes reachable
-- from h; all such entries lie in dom(Σ) and, by immutability, are identical
-- in Σ′."
⇝-mono : ∀ {σ σ′ h t} → σ ⊑ σ′ → σ ⊢ h ⇝ t → σ′ ⊢ h ⇝ t
⇝-mono sub (r-var e)       = r-var (sub _ _ e)
⇝-mono sub (r-lam e d)     = r-lam (sub _ _ e) (⇝-mono sub d)
⇝-mono sub (r-app e d₁ d₂) = r-app (sub _ _ e) (⇝-mono sub d₁) (⇝-mono sub d₂)
⇝-mono sub (r-ref e)       = r-ref (sub _ _ e)

ClosedIn-mono : ∀ {σ σ′ h} → σ ⊑ σ′ → ClosedIn σ h → ClosedIn σ′ h
ClosedIn-mono sub (t , d , c) = t , ⇝-mono sub d , c

------------------------------------------------------------------------
-- References of a stored hash (def:refs)

infix 4 _⊢_↝_
_⊢_↝_ : Store → Hash → Hash → Set
σ ⊢ h ↝ r = ∃[ t ] ((σ ⊢ h ⇝ t) × (r ∈ refsT t))

-- Callers (def:callers): "an observation, not a scoping device".
Callers : Store → Hash → Hash → Set
Callers σ h g = σ ⊢ g ↝ h

-- The transitive closure, the paper's callers*(Σ,h). Together with the
-- structure above those entries it is exactly the set a migration can change.
infix 4 _⊢_↝⁺_
data _⊢_↝⁺_ (σ : Store) : Hash → Hash → Set where
  tc-one  : ∀ {h r}   → σ ⊢ h ↝ r → σ ⊢ h ↝⁺ r
  tc-more : ∀ {h m r} → σ ⊢ h ↝ m → σ ⊢ m ↝⁺ r → σ ⊢ h ↝⁺ r

------------------------------------------------------------------------
-- Well-formedness (def:wf)
--
-- (i)   every stored hash reconstructs — no dangling structural child;
-- (ii)  reference targets denote closed terms — the load-bearing p4
--       invariant that lets E-Ref unfold with no shifting;
-- (iii) the reference graph is acyclic.
--
-- (iii) is stated as well-foundedness of "is cited by", which is exactly the
-- form the migration rewrite needs to recurse on (def:cascade), and is why
-- rem:acyclic insists the clause cannot be dropped: ⌈nref h⌉ = h satisfies
-- (i)-(ii).
--
-- Clause (ii) is not circular: whether r is closed depends only on
-- reconstruct(Σ,r), which stops at ref leaves.

Acyclic : Store → Set
Acyclic σ = WellFounded (λ r h → σ ⊢ h ↝ r)

record WF (σ : Store) : Set where
  field
    recon-total : ∀ {h} → h ∈dom σ → Recon σ h
    tgt-closed  : ∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → ClosedIn σ r
    ref-acyclic : Acyclic σ

------------------------------------------------------------------------
-- Transfer lemmas for a growing store
--
-- These are what thm:wf's proof sketch compresses into "no existing node
-- changes, and every newly added node's references point at pre-existing
-- closed entries; ordering new nodes by registration therefore extends any
-- topological order of the old reference graph, so no cycle is created."

-- An update splits the new domain into "the new key" and "the old domain".
update-dom : ∀ {σ : Store} n {h} → h ∈dom (σ [ hash n ↦ n ]) →
             (h ≡ hash n) ⊎ (h ∈dom σ)
update-dom {σ} n {h} mem = go h (h ≟ hash n) (proj₂ mem)
  where
  go : ∀ h″ → Dec (h″ ≡ hash n) → (σ [ hash n ↦ n ]) h″ ≡ just (proj₁ mem) →
       (h″ ≡ hash n) ⊎ (h″ ∈dom σ)
  go _  (yes eq) _ = inj₁ eq
  go h″ (no ¬p)  e =
    inj₂ (proj₁ mem , trans (sym (lookup-miss σ {hash n} {h″} n ¬p)) e)

-- An already-stored hash keeps exactly the reference edges it had: its
-- reconstruction cannot change (immutability + functionality).
⊑-edges : ∀ {σ σ′ h r} → σ ⊑ σ′ → Recon σ h → σ′ ⊢ h ↝ r → σ ⊢ h ↝ r
⊑-edges sub (t , d) (t′ , d′ , mem) =
  t , d , subst (λ ts → _ ∈ refsT ts) (⇝-func d′ (⇝-mono sub d)) mem

-- Well-foundedness transfers to a larger store, provided every edge out of a
-- *newly* stored hash lands in the old domain.
acyclic-transfer :
  ∀ {σ σ′} → σ ⊑ σ′ →
  (∀ {h} → h ∈dom σ → Recon σ h) →                       -- old wf (i)
  (∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → r ∈dom σ) →          -- old wf (ii), weakened
  Acyclic σ →
  (∀ {h r} → σ′ ⊢ h ↝ r → (h ∈dom σ) ⊎ (r ∈dom σ)) →     -- new edges reach back
  Acyclic σ′
acyclic-transfer {σ} {σ′} sub recon tgt acy back h = acc acc-step
  where
  -- Accessibility of an old hash lifts, because its edges are unchanged.
  lift : ∀ {x} → x ∈dom σ → Acc (λ r g → σ ⊢ g ↝ r) x → Acc (λ r g → σ′ ⊢ g ↝ r) x
  lift {x} mx (acc rs) = acc λ {r} edge →
    let old = ⊑-edges sub (recon mx) edge
    in  lift (tgt mx old) (rs old)

  acc-step : ∀ {r} → σ′ ⊢ h ↝ r → Acc (λ y g → σ′ ⊢ g ↝ y) r
  acc-step {r} edge with back edge
  ... | inj₁ mh = lift (tgt mh (⊑-edges sub (recon mh) edge)) (acy r)
  ... | inj₂ mr = lift mr (acy r)

------------------------------------------------------------------------
-- The store is keyed by hash
--
-- Not a clause of def:wf, but tacitly used by thm:mono's proof. See the
-- header note.

HashKeyed : Store → Set
HashKeyed σ = ∀ h n → σ h ≡ just n → h ≡ hash n

-- Under HashKeyed, writing a node at its own hash cannot disturb an existing
-- entry: a collision on the key is a collision on the hash, hence by (★) the
-- very same node.
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
-- Ingest grows the store and preserves its keying (feeds thm:mono)

ingest-keyed : ∀ t σ → HashKeyed σ → HashKeyed (proj₁ (ingest σ t))
ingest-keyed (var i) σ kd = keyed-update kd (nvar i)
ingest-keyed (lam t) σ kd = keyed-update (ingest-keyed t σ kd) _
ingest-keyed (app t u) σ kd =
  keyed-update (ingest-keyed u (proj₁ (ingest σ t)) (ingest-keyed t σ kd)) _
ingest-keyed (ref h) σ kd = keyed-update kd (nref h)

ingest-⊑ : ∀ t σ → HashKeyed σ → σ ⊑ proj₁ (ingest σ t)
ingest-⊑ (var i) σ kd = ⊑-update (keyed-undisturbed kd (nvar i))
ingest-⊑ (lam t) σ kd =
  ⊑-trans (ingest-⊑ t σ kd)
          (⊑-update (keyed-undisturbed (ingest-keyed t σ kd) _))
ingest-⊑ (app t u) σ kd =
  ⊑-trans (ingest-⊑ t σ kd)
    (⊑-trans (ingest-⊑ u (proj₁ (ingest σ t)) (ingest-keyed t σ kd))
             (⊑-update (keyed-undisturbed
               (ingest-keyed u (proj₁ (ingest σ t)) (ingest-keyed t σ kd)) _)))
ingest-⊑ (ref h) σ kd = ⊑-update (keyed-undisturbed kd (nref h))

-- Ingesting a term and reconstructing its root gives the term back.
--
-- The paper leans on this twice without stating it: Cache-sound's premise
-- ("the value it ingested reconstructs to itself", thm:wf's Eval case) and
-- lem:cascade-order's re-ingestion step.
ingest-⇝ : ∀ t σ → HashKeyed σ → proj₁ (ingest σ t) ⊢ proj₂ (ingest σ t) ⇝ t
ingest-⇝ (var i) σ kd = r-var (lookup-hit σ (hash (nvar i)) (nvar i))
ingest-⇝ (ref h) σ kd = r-ref (lookup-hit σ (hash (nref h)) (nref h))
ingest-⇝ (lam t) σ kd =
  r-lam (lookup-hit σ₁ (hash (nlam h)) (nlam h))
        (⇝-mono grow (ingest-⇝ t σ kd))
  where
  σ₁   = proj₁ (ingest σ t)
  h    = proj₂ (ingest σ t)
  grow = ⊑-update (keyed-undisturbed (ingest-keyed t σ kd) (nlam h))
ingest-⇝ (app t u) σ kd =
  r-app (lookup-hit σ₂ (hash n) n)
        (⇝-mono (⊑-trans grow₁₂ grow₂₃) (ingest-⇝ t σ kd))
        (⇝-mono grow₂₃ (ingest-⇝ u σ₁ kd₁))
  where
  σ₁     = proj₁ (ingest σ t)
  h₁     = proj₂ (ingest σ t)
  kd₁    = ingest-keyed t σ kd
  σ₂     = proj₁ (ingest σ₁ u)
  h₂     = proj₂ (ingest σ₁ u)
  kd₂    = ingest-keyed u σ₁ kd₁
  n      = napp h₁ h₂
  grow₁₂ = ingest-⊑ u σ₁ kd₁
  grow₂₃ = ⊑-update (keyed-undisturbed kd₂ n)

------------------------------------------------------------------------
-- Hashing collapses exactly α-equivalence (thm:alpha)
--
-- The (⇐) direction is that hashOf is a function. The (⇒) direction is this,
-- by induction using (★): equal root hashes force equal top nodes, hence
-- equal child hashes, hence (IH) equal subterms.

hashOf-inj : ∀ t u → hashOf t ≡ hashOf u → t ≡ u
hashOf-inj (var i) (var j) e = cong var (nvar-inj (hash-inj e))
hashOf-inj (var _) (lam _) e with hash-inj e
...                             | ()
hashOf-inj (var _) (app _ _) e with hash-inj e
...                             | ()
hashOf-inj (var _) (ref _) e with hash-inj e
...                             | ()
hashOf-inj (lam _) (var _) e with hash-inj e
...                             | ()
hashOf-inj (lam t) (lam u) e = cong lam (hashOf-inj t u (nlam-inj (hash-inj e)))
hashOf-inj (lam _) (app _ _) e with hash-inj e
...                             | ()
hashOf-inj (lam _) (ref _) e with hash-inj e
...                             | ()
hashOf-inj (app _ _) (var _) e with hash-inj e
...                             | ()
hashOf-inj (app _ _) (lam _) e with hash-inj e
...                             | ()
hashOf-inj (app t₁ t₂) (app u₁ u₂) e with napp-inj (hash-inj e)
... | (e₁ , e₂) = cong₂ app (hashOf-inj t₁ u₁ e₁) (hashOf-inj t₂ u₂ e₂)
hashOf-inj (app _ _) (ref _) e with hash-inj e
...                             | ()
hashOf-inj (ref _) (var _) e with hash-inj e
...                             | ()
hashOf-inj (ref _) (lam _) e with hash-inj e
...                             | ()
hashOf-inj (ref _) (app _ _) e with hash-inj e
...                             | ()
hashOf-inj (ref a) (ref b) e = cong ref (nref-inj (hash-inj e))
