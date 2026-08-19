{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Coherence preservation (arbor-core thm:wf)
--
-- The paper's proof is four sentences; this is what they expand to. The bulk is
-- `ingest`: registering a deep term bottom-up leaves the store well-formed,
-- clause by clause. The acyclicity argument — "ordering new nodes by
-- registration extends any topological order of the old reference graph, so no
-- cycle is created" — is Store.acyclic-transfer, fed by ingest-edges below.
--
-- Each clause of wf is threaded separately rather than as `WF σ`, because the
-- app case needs a clause *for the intermediate store*, which is this very
-- lemma applied to the left subterm.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Preservation (P : Params) where

open import Arbor.Core.Config P public

open import Arbor.Hash using (HashModel)
open HashModel hashModel using (_≟_)

open import Data.List.Base using (List; []; _∷_)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Relation.Unary.All using (All; []; _∷_)
open import Data.Maybe.Base using (just; nothing)
open import Data.List.Membership.Propositional.Properties using (∈-++⁺ˡ; ∈-++⁺ʳ)
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; subst; subst₂)
open import Relation.Nullary.Decidable.Core using (Dec; yes; no)

private
  nil-absurd : ∀ {A : Set} {y : Hash} → y ∈ [] → A
  nil-absurd ()

  one : ∀ {y g : Hash} → y ∈ (g ∷ []) → y ≡ g
  one (here refl) = refl
  one (there ())

ClosedIn→∈dom : ∀ {σ r} → ClosedIn σ r → r ∈dom σ
ClosedIn→∈dom (_ , d , _) = ⇝-∈dom d

------------------------------------------------------------------------
-- Registering the root node: the last step of each ingest

grow-lam : ∀ t σ → HashKeyed σ → proj₁ (ingest σ t) ⊑ proj₁ (ingest σ (lam t))
grow-lam t σ kd = ⊑-update (keyed-undisturbed (ingest-keyed t σ kd) _)

grow-app : ∀ t u σ → HashKeyed σ →
           proj₁ (ingest (proj₁ (ingest σ t)) u) ⊑ proj₁ (ingest σ (app t u))
grow-app t u σ kd =
  ⊑-update (keyed-undisturbed
             (ingest-keyed u (proj₁ (ingest σ t)) (ingest-keyed t σ kd)) _)

------------------------------------------------------------------------
-- wf (i): every stored hash reconstructs

ingest-recon : ∀ t σ → HashKeyed σ →
               (∀ {h} → h ∈dom σ → Recon σ h) →
               ∀ {h} → h ∈dom (proj₁ (ingest σ t)) →
               Recon (proj₁ (ingest σ t)) h

ingest-recon (var i) σ kd rec mem with update-dom (nvar i) mem
... | inj₁ eq = subst (Recon _) (sym eq) (var i , ingest-⇝ (var i) σ kd)
... | inj₂ mσ with rec mσ
...   | (t′ , d) = t′ , ⇝-mono (ingest-⊑ (var i) σ kd) d

ingest-recon (ref g) σ kd rec mem with update-dom (nref g) mem
... | inj₁ eq = subst (Recon _) (sym eq) (ref g , ingest-⇝ (ref g) σ kd)
... | inj₂ mσ with rec mσ
...   | (t′ , d) = t′ , ⇝-mono (ingest-⊑ (ref g) σ kd) d

ingest-recon (lam t) σ kd rec mem
  with update-dom (nlam (proj₂ (ingest σ t))) mem
... | inj₁ eq = subst (Recon _) (sym eq) (lam t , ingest-⇝ (lam t) σ kd)
... | inj₂ m₁ with ingest-recon t σ kd rec m₁
...   | (t′ , d) = t′ , ⇝-mono (grow-lam t σ kd) d

ingest-recon (app t u) σ kd rec mem
  with update-dom (napp (proj₂ (ingest σ t))
                        (proj₂ (ingest (proj₁ (ingest σ t)) u))) mem
... | inj₁ eq = subst (Recon _) (sym eq) (app t u , ingest-⇝ (app t u) σ kd)
... | inj₂ m₂ with ingest-recon u (proj₁ (ingest σ t)) (ingest-keyed t σ kd)
                     (ingest-recon t σ kd rec) m₂
...   | (t′ , d) = t′ , ⇝-mono (grow-app t u σ kd) d

------------------------------------------------------------------------
-- wf (ii): reference targets denote closed terms

ingest-tgt : ∀ t σ → HashKeyed σ →
             (∀ {h} → h ∈dom σ → Recon σ h) →
             (∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → ClosedIn σ r) →
             (∀ {r} → r ∈ refsT t → ClosedIn σ r) →
             ∀ {h r} → h ∈dom (proj₁ (ingest σ t)) →
             (proj₁ (ingest σ t)) ⊢ h ↝ r → ClosedIn (proj₁ (ingest σ t)) r

ingest-tgt (var i) σ kd rec tgt rc mem (t′ , d′ , memr)
  with update-dom (nvar i) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (var i) σ kd)
...   | refl = nil-absurd memr
ingest-tgt (var i) σ kd rec tgt rc mem (t′ , d′ , memr) | inj₂ mσ =
  ClosedIn-mono (ingest-⊑ (var i) σ kd)
    (tgt mσ (⊑-edges (ingest-⊑ (var i) σ kd) (rec mσ) (t′ , d′ , memr)))

ingest-tgt (ref g) σ kd rec tgt rc mem (t′ , d′ , memr)
  with update-dom (nref g) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (ref g) σ kd)
...   | refl = subst (ClosedIn _) (sym (one memr))
                     (ClosedIn-mono (ingest-⊑ (ref g) σ kd) (rc (here refl)))
ingest-tgt (ref g) σ kd rec tgt rc mem (t′ , d′ , memr) | inj₂ mσ =
  ClosedIn-mono (ingest-⊑ (ref g) σ kd)
    (tgt mσ (⊑-edges (ingest-⊑ (ref g) σ kd) (rec mσ) (t′ , d′ , memr)))

ingest-tgt (lam t) σ kd rec tgt rc mem (t′ , d′ , memr)
  with update-dom (nlam (proj₂ (ingest σ t))) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (lam t) σ kd)
...   | refl = ClosedIn-mono (ingest-⊑ (lam t) σ kd) (rc memr)
ingest-tgt (lam t) σ kd rec tgt rc mem (t′ , d′ , memr) | inj₂ m₁ =
  ClosedIn-mono (grow-lam t σ kd)
    (ingest-tgt t σ kd rec tgt rc m₁
      (⊑-edges (grow-lam t σ kd) (ingest-recon t σ kd rec m₁) (t′ , d′ , memr)))

ingest-tgt (app t u) σ kd rec tgt rc mem (t′ , d′ , memr)
  with update-dom (napp (proj₂ (ingest σ t))
                        (proj₂ (ingest (proj₁ (ingest σ t)) u))) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (app t u) σ kd)
...   | refl = ClosedIn-mono (ingest-⊑ (app t u) σ kd) (rc memr)
ingest-tgt (app t u) σ kd rec tgt rc mem (t′ , d′ , memr) | inj₂ m₂ =
  ClosedIn-mono (grow-app t u σ kd)
    (ingest-tgt u σ₁ kd₁ rec₁ tgt₁ rc₁ m₂
      (⊑-edges (grow-app t u σ kd) (ingest-recon u σ₁ kd₁ rec₁ m₂) (t′ , d′ , memr)))
  where
  σ₁   = proj₁ (ingest σ t)
  kd₁  = ingest-keyed t σ kd
  rec₁ = ingest-recon t σ kd rec
  tgt₁ = ingest-tgt t σ kd rec tgt (λ m → rc (∈-++⁺ˡ m))
  rc₁  : ∀ {r} → r ∈ refsT u → ClosedIn σ₁ r
  rc₁ m = ClosedIn-mono (ingest-⊑ t σ kd) (rc (∈-++⁺ʳ (refsT t) m))

------------------------------------------------------------------------
-- wf (iii): every edge either leaves an already-stored hash, or lands in the
-- base store
--
-- This is the provenance fact the acyclicity argument needs: a newly registered
-- node's references are exactly the ref leaves of the term being ingested, and
-- those were required to denote closed — hence stored — terms. So no new edge
-- can close a cycle.

ingest-edges : ∀ t σ {σ₀ : Store} → HashKeyed σ →
               (∀ {h} → h ∈dom σ → Recon σ h) →
               (∀ {r} → r ∈ refsT t → r ∈dom σ₀) →
               ∀ {h r} → (proj₁ (ingest σ t)) ⊢ h ↝ r →
               (h ∈dom σ) ⊎ (r ∈dom σ₀)

ingest-edges (var i) σ kd rec rc (t′ , d′ , memr)
  with update-dom (nvar i) (⇝-∈dom d′)
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (var i) σ kd)
...   | refl = nil-absurd memr
ingest-edges (var i) σ kd rec rc (t′ , d′ , memr) | inj₂ mσ = inj₁ mσ

ingest-edges (ref g) σ {σ₀} kd rec rc (t′ , d′ , memr)
  with update-dom (nref g) (⇝-∈dom d′)
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (ref g) σ kd)
...   | refl = inj₂ (subst (λ y → y ∈dom σ₀) (sym (one memr)) (rc (here refl)))
ingest-edges (ref g) σ {σ₀} kd rec rc (t′ , d′ , memr) | inj₂ mσ = inj₁ mσ

ingest-edges (lam t) σ kd rec rc (t′ , d′ , memr)
  with update-dom (nlam (proj₂ (ingest σ t))) (⇝-∈dom d′)
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (lam t) σ kd)
...   | refl = inj₂ (rc memr)
ingest-edges (lam t) σ kd rec rc (t′ , d′ , memr) | inj₂ m₁ =
  ingest-edges t σ kd rec rc
    (⊑-edges (grow-lam t σ kd) (ingest-recon t σ kd rec m₁) (t′ , d′ , memr))

ingest-edges (app t u) σ kd rec rc (t′ , d′ , memr)
  with update-dom (napp (proj₂ (ingest σ t))
                        (proj₂ (ingest (proj₁ (ingest σ t)) u))) (⇝-∈dom d′)
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (app t u) σ kd)
...   | refl = inj₂ (rc memr)
ingest-edges (app t u) σ {σ₀} kd rec rc {h} {r} (t′ , d′ , memr) | inj₂ m₂ =
  finish (ingest-edges u σ₁ kd₁ rec₁ rcu edge₂)
  where
  σ₁   = proj₁ (ingest σ t)
  kd₁  = ingest-keyed t σ kd
  rec₁ = ingest-recon t σ kd rec

  rcu : ∀ {y} → y ∈ refsT u → y ∈dom σ₀
  rcu m = rc (∈-++⁺ʳ (refsT t) m)

  -- Down one level: the edge is in the store after ingesting u.
  edge₂ : proj₁ (ingest σ₁ u) ⊢ h ↝ r
  edge₂ = ⊑-edges (grow-app t u σ kd)
                  (ingest-recon u σ₁ kd₁ rec₁ m₂) (t′ , d′ , memr)

  -- If u's ingest says the source was already there before u, the edge is one
  -- of t's, so recurse on the left subterm — down one level again.
  finish : (h ∈dom σ₁) ⊎ (r ∈dom σ₀) → (h ∈dom σ) ⊎ (r ∈dom σ₀)
  finish (inj₂ mr) = inj₂ mr
  finish (inj₁ m₁) =
    ingest-edges t σ kd rec (λ m → rc (∈-++⁺ˡ m))
      (⊑-edges (ingest-⊑ u σ₁ kd₁) (rec₁ m₁) edge₂)

------------------------------------------------------------------------
-- Ingest preserves well-formedness

ingest-wf : ∀ t σ → HashKeyed σ → WF σ →
            (∀ {r} → r ∈ refsT t → ClosedIn σ r) →
            WF (proj₁ (ingest σ t))
ingest-wf t σ kd wf rc = record
  { recon-total = rec′
  ; tgt-closed  = tgt′
  ; ref-acyclic =
      acyclic-transfer (ingest-⊑ t σ kd) rec
                       (λ m e → ClosedIn→∈dom (tgt m e))
                       (WF.ref-acyclic wf)
                       (ingest-edges t σ kd rec (λ m → ClosedIn→∈dom (rc m)))
  }
  where
  rec  = WF.recon-total wf
  tgt  = WF.tgt-closed wf
  rec′ = ingest-recon t σ kd rec
  tgt′ = ingest-tgt t σ kd rec tgt rc

------------------------------------------------------------------------
-- Naming and history

-- History coherence survives a bind/rebind: the appended event's Maybe-Hash is
-- exactly the new binding.
bind-histcoh : ∀ {ν η x h τ} → HistCoherent ν η →
               HistCoherent (bindName ν x h) (appendEvent η x (just h , τ))
bind-histcoh {ν} {η} {x} {h} {τ} hc y = go y (y ≟N x)
  where
  go : ∀ y′ → Dec (y′ ≡ x) →
       Head (events (appendEvent η x (just h , τ)) y′) (bindName ν x h y′)
  go _  (yes refl) = subst₂ Head (sym (events-hit η x (just h , τ)))
                                 (sym (MN.lookup-hit ν x h)) head-cons
  go y′ (no ¬p)    = subst₂ Head (sym (events-miss η (just h , τ) ¬p))
                                 (sym (MN.lookup-miss ν {x} {y′} h ¬p)) (hc y′)

-- And an unbind: the tombstone's ⊥ matches the name's absence.
unbind-histcoh : ∀ {ν η x τ} → HistCoherent ν η →
                 HistCoherent (unbindName ν x) (appendEvent η x (nothing , τ))
unbind-histcoh {ν} {η} {x} {τ} hc y = go y (y ≟N x)
  where
  go : ∀ y′ → Dec (y′ ≡ x) →
       Head (events (appendEvent η x (nothing , τ)) y′) (unbindName ν x y′)
  go _  (yes refl) = subst₂ Head (sym (events-hit η x (nothing , τ)))
                                 (sym (MN.remove-hit ν x)) head-cons
  go y′ (no ¬p)    = subst₂ Head (sym (events-miss η (nothing , τ) ¬p))
                                 (sym (MN.remove-miss ν {x} {y′} ¬p)) (hc y′)

-- Appending a bind event keeps every recorded hash closed.
append-hist-closed :
  ∀ {σ} η x h τ →
  (∀ {y g τ′} → (just g , τ′) ∈ events η y → ClosedIn σ g) →
  ClosedIn σ h →
  ∀ {y g τ′} → (just g , τ′) ∈ events (appendEvent η x (just h , τ)) y →
  ClosedIn σ g
append-hist-closed {σ} η x h τ old cl {y} {g} {τ′} mem = go y (y ≟N x) mem
  where
  go : ∀ y′ → Dec (y′ ≡ x) →
       (just g , τ′) ∈ events (appendEvent η x (just h , τ)) y′ → ClosedIn σ g
  go _ (yes refl) m
    with subst (λ es → (just g , τ′) ∈ es) (events-hit η x (just h , τ)) m
  ...  | here refl = cl
  ...  | there m′  = old m′
  go y′ (no ¬p) m = old (subst (λ es → (just g , τ′) ∈ es) (events-miss η (just h , τ) ¬p) m)

-- Appending a tombstone cannot introduce a hash at all.
append-hist-closed-⊥ :
  ∀ {σ} η x τ →
  (∀ {y g τ′} → (just g , τ′) ∈ events η y → ClosedIn σ g) →
  ∀ {y g τ′} → (just g , τ′) ∈ events (appendEvent η x (nothing , τ)) y →
  ClosedIn σ g
append-hist-closed-⊥ {σ} η x τ old {y} {g} {τ′} mem = go y (y ≟N x) mem
  where
  go : ∀ y′ → Dec (y′ ≡ x) →
       (just g , τ′) ∈ events (appendEvent η x (nothing , τ)) y′ → ClosedIn σ g
  go _ (yes refl) m
    with subst (λ es → (just g , τ′) ∈ es) (events-hit η x (nothing , τ)) m
  ...  | there m′ = old m′
  go y′ (no ¬p) m = old (subst (λ es → (just g , τ′) ∈ es) (events-miss η (nothing , τ) ¬p) m)

------------------------------------------------------------------------
-- The atomic bundle is a fold of binds, so each property is an induction on it

multiRebind-histcoh : ∀ ν η τ U → HistCoherent ν η →
                      HistCoherent (proj₁ (multiRebind ν η τ U))
                                   (proj₂ (multiRebind ν η τ U))
multiRebind-histcoh ν η τ []             hc = hc
multiRebind-histcoh ν η τ ((x , h) ∷ bs) hc =
  multiRebind-histcoh _ _ τ bs (bind-histcoh hc)

multiRebind-names : ∀ {σ} ν η τ U →
                    (∀ {x h} → ν x ≡ just h → ClosedIn σ h) →
                    All (λ p → ClosedIn σ (proj₂ p)) U →
                    ∀ {x h} → proj₁ (multiRebind ν η τ U) x ≡ just h → ClosedIn σ h
multiRebind-names ν η τ []             nc []         eq = nc eq
multiRebind-names ν η τ ((x , h) ∷ bs) nc (cl ∷ cls) eq =
  multiRebind-names _ _ τ bs one-bind cls eq
  where
  one-bind : ∀ {y g} → bindName ν x h y ≡ just g → ClosedIn _ g
  one-bind e with MN.update-split ν x h e
  ... | inj₁ (_ , refl) = cl
  ... | inj₂ old        = nc old

multiRebind-hist : ∀ {σ} ν η τ U →
                   (∀ {x g τ′} → (just g , τ′) ∈ events η x → ClosedIn σ g) →
                   All (λ p → ClosedIn σ (proj₂ p)) U →
                   ∀ {x g τ′} → (just g , τ′) ∈ events (proj₂ (multiRebind ν η τ U)) x →
                   ClosedIn σ g
multiRebind-hist ν η τ []             hc []         mem = hc mem
multiRebind-hist ν η τ ((x , h) ∷ bs) hc (cl ∷ cls) mem =
  multiRebind-hist _ _ τ bs (append-hist-closed η x h τ hc cl) cls mem

------------------------------------------------------------------------
-- Cache soundness is preserved (the content of cor:cache)

cache-preserved : ∀ {C C′} → Coherent C → C ⟶ C′ →
                  CacheSound (store C′) (cache C′)
cache-preserved coh (t-ingest {σ = σ} {t = t} _ _) =
  CacheSound-mono (ingest-⊑ t σ (Coherent.keyed coh)) (Coherent.cache-sound coh)
cache-preserved coh (t-bind _ _)            = Coherent.cache-sound coh
cache-preserved coh (t-rebind _)            = Coherent.cache-sound coh
cache-preserved coh (t-unbind _)            = Coherent.cache-sound coh
cache-preserved coh (t-migrate R _ _ _ _ _) =
  CacheSound-mono (RewriteData.grows R) (Coherent.cache-sound coh)
-- The Eval case is the only one with content: the freshly written entry must be
-- sound, which is ingest-⇝ ("the value it ingested reconstructs to itself"),
-- and the pre-existing entries must survive, which is CacheSound-mono.
cache-preserved coh (t-eval {σ = σ} {ε = ε} {h = h} {v = v} _ d _) = fresh
  where
  kd   = Coherent.keyed coh
  σ′   = proj₁ (ingest σ v)
  hv   = proj₂ (ingest σ v)
  grow = ingest-⊑ v σ kd

  fresh : CacheSound σ′ (ε [ h ↦ hv ])
  fresh {h″} {hv″} eq with M.update-split ε h hv eq
  ... | inj₁ (refl , refl) = v , ingest-⇝ v σ kd , ⇓-mono grow d
  ... | inj₂ old with Coherent.cache-sound coh old
  ...   | (w , r , dd) = w , ⇝-mono grow r , ⇓-mono grow dd

------------------------------------------------------------------------
-- Coherence preservation (thm:wf)

coherent-preserved : ∀ {C C′} → Coherent C → C ⟶ C′ → Coherent C′

coherent-preserved coh ing@(t-ingest {σ = σ} {t = t} ct rc) = record
  { wf           = ingest-wf t σ kd (Coherent.wf coh) rc
  ; keyed        = ingest-keyed t σ kd
  ; names-closed = λ eq  → ClosedIn-mono grow (Coherent.names-closed coh eq)
  ; hist-closed  = λ mem → ClosedIn-mono grow (Coherent.hist-closed coh mem)
  ; hist-coh     = Coherent.hist-coh coh
  ; cache-sound  = cache-preserved coh ing
  }
  where
  kd   = Coherent.keyed coh
  grow = ingest-⊑ t σ kd

coherent-preserved coh ev@(t-eval {σ = σ} {h = h} {v = v} cl d fresh) = record
  { wf           = ingest-wf v σ kd (Coherent.wf coh) rcv
  ; keyed        = ingest-keyed v σ kd
  ; names-closed = λ eq  → ClosedIn-mono grow (Coherent.names-closed coh eq)
  ; hist-closed  = λ mem → ClosedIn-mono grow (Coherent.hist-closed coh mem)
  ; hist-coh     = Coherent.hist-coh coh
  ; cache-sound  = cache-preserved coh ev
  }
  where
  kd   = Coherent.keyed coh
  grow = ingest-⊑ v σ kd
  -- This is where the corrected lem:closed-no-stuck earns the closed_Σ(h)
  -- premise: without it there is no RefsClosed to feed ⇓-closed.
  rcref : RefsClosed σ (ref h)
  rcref mem = subst (ClosedIn σ) (sym (one mem)) cl
  rcv : ∀ {r} → r ∈ refsT v → ClosedIn σ r
  rcv = proj₂ (⇓-closed (Coherent.wf coh) s-ref rcref d)

coherent-preserved coh (t-bind {ν = ν} {x = x} {h = h} {τ = τ} fr cl) = record
  { wf           = Coherent.wf coh
  ; keyed        = Coherent.keyed coh
  ; names-closed = names′
  ; hist-closed  = append-hist-closed _ x h τ (Coherent.hist-closed coh) cl
  ; hist-coh     = bind-histcoh (Coherent.hist-coh coh)
  ; cache-sound  = Coherent.cache-sound coh
  }
  where
  names′ : ∀ {y g} → bindName ν x h y ≡ just g → ClosedIn _ g
  names′ e with MN.update-split ν x h e
  ... | inj₁ (_ , refl) = cl
  ... | inj₂ old        = Coherent.names-closed coh old

coherent-preserved coh (t-rebind {ν = ν} {x = x} {h = h} {τ = τ} cl) = record
  { wf           = Coherent.wf coh
  ; keyed        = Coherent.keyed coh
  ; names-closed = names′
  ; hist-closed  = append-hist-closed _ x h τ (Coherent.hist-closed coh) cl
  ; hist-coh     = bind-histcoh (Coherent.hist-coh coh)
  ; cache-sound  = Coherent.cache-sound coh
  }
  where
  names′ : ∀ {y g} → bindName ν x h y ≡ just g → ClosedIn _ g
  names′ e with MN.update-split ν x h e
  ... | inj₁ (_ , refl) = cl
  ... | inj₂ old        = Coherent.names-closed coh old

coherent-preserved coh (t-unbind {ν = ν} {x = x} {τ = τ} bound) = record
  { wf           = Coherent.wf coh
  ; keyed        = Coherent.keyed coh
  ; names-closed = λ e → Coherent.names-closed coh (proj₂ (MN.remove-split ν x e))
  ; hist-closed  = append-hist-closed-⊥ _ x τ (Coherent.hist-closed coh)
  ; hist-coh     = unbind-histcoh (Coherent.hist-coh coh)
  ; cache-sound  = Coherent.cache-sound coh
  }

coherent-preserved coh mig@(t-migrate {ν = ν} {η = η} {τ = τ} R U bound clnew inU all) =
  record
  { wf           = RewriteData.wf′ R (Coherent.wf coh) (Coherent.keyed coh)
  ; keyed        = RewriteData.keyed R (Coherent.keyed coh)
  ; names-closed = multiRebind-names ν η τ U oldNames all
  ; hist-closed  = multiRebind-hist ν η τ U oldHist all
  ; hist-coh     = multiRebind-histcoh ν η τ U (Coherent.hist-coh coh)
  ; cache-sound  = cache-preserved coh mig
  }
  where
  grow = RewriteData.grows R

  oldNames : ∀ {y g} → ν y ≡ just g → ClosedIn (RewriteData.σ′ R) g
  oldNames eq = ClosedIn-mono grow (Coherent.names-closed coh eq)

  oldHist : ∀ {y g τ′} → (just g , τ′) ∈ events η y → ClosedIn (RewriteData.σ′ R) g
  oldHist mem = ClosedIn-mono grow (Coherent.hist-closed coh mem)

------------------------------------------------------------------------
-- wf (ii), stated in a fixed larger store
--
-- ingest-tgt above concludes in the STEP store, which is the right shape when
-- well-formedness is re-established after every ingest. The migration rewrite
-- cannot work that way: registering an entry's image needs its rewritten
-- references closed, and those are closed in the FINAL store — the one holding
-- every image — not necessarily in the accumulator, whose contents depend on
-- the order the fold happened to run in.
--
-- So the same induction, with the conclusion (and the reference premise) moved
-- to a fixed `big`. This is what lets Arbor.Core.Rewrite register images in any
-- order at all.

ingest-tgt-big :
  ∀ t σ (big : Store) → HashKeyed σ →
  (∀ {h} → h ∈dom σ → Recon σ h) →
  (∀ {h r} → h ∈dom σ → σ ⊢ h ↝ r → ClosedIn big r) →
  (∀ {r} → r ∈ refsT t → ClosedIn big r) →
  proj₁ (ingest σ t) ⊑ big →
  ∀ {h r} → h ∈dom (proj₁ (ingest σ t)) →
  (proj₁ (ingest σ t)) ⊢ h ↝ r → ClosedIn big r

ingest-tgt-big (var i) σ big kd rec tgt rc sub mem (t′ , d′ , memr)
  with update-dom (nvar i) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (var i) σ kd)
...   | refl = nil-absurd memr
ingest-tgt-big (var i) σ big kd rec tgt rc sub mem (t′ , d′ , memr) | inj₂ mσ =
  tgt mσ (⊑-edges (ingest-⊑ (var i) σ kd) (rec mσ) (t′ , d′ , memr))

ingest-tgt-big (ref g) σ big kd rec tgt rc sub mem (t′ , d′ , memr)
  with update-dom (nref g) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (ref g) σ kd)
...   | refl = subst (ClosedIn big) (sym (one memr)) (rc (here refl))
ingest-tgt-big (ref g) σ big kd rec tgt rc sub mem (t′ , d′ , memr) | inj₂ mσ =
  tgt mσ (⊑-edges (ingest-⊑ (ref g) σ kd) (rec mσ) (t′ , d′ , memr))

ingest-tgt-big (lam t) σ big kd rec tgt rc sub mem (t′ , d′ , memr)
  with update-dom (nlam (proj₂ (ingest σ t))) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (lam t) σ kd)
...   | refl = rc memr
ingest-tgt-big (lam t) σ big kd rec tgt rc sub mem (t′ , d′ , memr) | inj₂ m₁ =
  ingest-tgt-big t σ big kd rec tgt rc
    (⊑-trans (grow-lam t σ kd) sub) m₁
    (⊑-edges (grow-lam t σ kd) (ingest-recon t σ kd rec m₁) (t′ , d′ , memr))

ingest-tgt-big (app t u) σ big kd rec tgt rc sub mem (t′ , d′ , memr)
  with update-dom (napp (proj₂ (ingest σ t))
                        (proj₂ (ingest (proj₁ (ingest σ t)) u))) mem
... | inj₁ eq with ⇝-func (subst (λ x → _ ⊢ x ⇝ t′) eq d′) (ingest-⇝ (app t u) σ kd)
...   | refl = rc memr
ingest-tgt-big (app t u) σ big kd rec tgt rc sub mem (t′ , d′ , memr) | inj₂ m₂ =
  ingest-tgt-big u σ₁ big kd₁ rec₁ tgt₁ rcu (⊑-trans (grow-app t u σ kd) sub) m₂
    (⊑-edges (grow-app t u σ kd) (ingest-recon u σ₁ kd₁ rec₁ m₂) (t′ , d′ , memr))
  where
  σ₁   = proj₁ (ingest σ t)
  kd₁  = ingest-keyed t σ kd
  rec₁ = ingest-recon t σ kd rec
  tgt₁ = ingest-tgt-big t σ big kd rec tgt (λ m → rc (∈-++⁺ˡ m))
           (⊑-trans (⊑-trans (ingest-⊑ u σ₁ kd₁) (grow-app t u σ kd)) sub)
  rcu : ∀ {r} → r ∈ refsT u → ClosedIn big r
  rcu m = rc (∈-++⁺ʳ (refsT t) m)

------------------------------------------------------------------------
-- Where an entry came from
--
-- After an ingest, a stored hash is either one the base store already had, or
-- one of the ingested term's own nodes — in which case it reconstructs to a
-- subterm, whose references are among the whole term's. Clause (iii) for a
-- migration needs this: to bound an entry's out-edges you must first know which
-- term put it there.

ingest-prov : ∀ t σ → HashKeyed σ →
              ∀ {g} → g ∈dom (proj₁ (ingest σ t)) →
              (g ∈dom σ)
            ⊎ (∃ λ t′ → ((proj₁ (ingest σ t)) ⊢ g ⇝ t′)
                      × (∀ {r} → r ∈ refsT t′ → r ∈ refsT t))

ingest-prov (var i) σ kd mem with update-dom (nvar i) mem
... | inj₂ mσ = inj₁ mσ
... | inj₁ eq = inj₂ (var i , subst (λ x → _ ⊢ x ⇝ var i) (sym eq)
                                    (ingest-⇝ (var i) σ kd)
                             , λ m → m)

ingest-prov (ref g) σ kd mem with update-dom (nref g) mem
... | inj₂ mσ = inj₁ mσ
... | inj₁ eq = inj₂ (ref g , subst (λ x → _ ⊢ x ⇝ ref g) (sym eq)
                                    (ingest-⇝ (ref g) σ kd)
                            , λ m → m)

ingest-prov (lam t) σ kd mem with update-dom (nlam (proj₂ (ingest σ t))) mem
... | inj₁ eq = inj₂ (lam t , subst (λ x → _ ⊢ x ⇝ lam t) (sym eq)
                                    (ingest-⇝ (lam t) σ kd)
                            , λ m → m)
... | inj₂ m₁ with ingest-prov t σ kd m₁
...   | inj₁ mσ = inj₁ mσ
...   | inj₂ (t′ , d′ , sub) =
        inj₂ (t′ , ⇝-mono (grow-lam t σ kd) d′ , sub)

ingest-prov (app t u) σ kd mem
  with update-dom (napp (proj₂ (ingest σ t))
                        (proj₂ (ingest (proj₁ (ingest σ t)) u))) mem
... | inj₁ eq = inj₂ (app t u , subst (λ x → _ ⊢ x ⇝ app t u) (sym eq)
                                      (ingest-⇝ (app t u) σ kd)
                              , λ m → m)
... | inj₂ m₂ with ingest-prov u (proj₁ (ingest σ t)) (ingest-keyed t σ kd) m₂
...   | inj₂ (t′ , d′ , sub) =
        inj₂ (t′ , ⇝-mono (grow-app t u σ kd) d′
                 , λ m → ∈-++⁺ʳ (refsT t) (sub m))
...   | inj₁ m₁ with ingest-prov t σ kd m₁
...     | inj₁ mσ = inj₁ mσ
...     | inj₂ (t′ , d′ , sub) =
          inj₂ (t′ , ⇝-mono (⊑-trans (ingest-⊑ u (proj₁ (ingest σ t))
                                               (ingest-keyed t σ kd))
                                     (grow-app t u σ kd)) d′
                   , λ m → ∈-++⁺ˡ (sub m))
