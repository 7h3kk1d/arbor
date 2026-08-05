{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Evaluation (arbor-core §sec:store, def:value / def:eval)
--
-- Call-by-value to weak head normal form, reading Σ only to unfold
-- references — Cardelli's linking semantics. It takes NO namespace argument,
-- and that is the whole point: N-independence is syntactic here, which is
-- what makes thm:nsb a one-line consequence rather than an argument.
--
-- Two evaluators, as the paper prescribes (decisions.md 2026-07-23):
--   * ⇓, a relation, undefined on divergence. This is the object the
--     metatheory is about, and it stays term-level and pure.
--   * eval, fuel-indexed, total, the implementation p4 actually runs. Its
--     Value/Stuck/StepLimit trichotomy is where cor:fuel lives.
------------------------------------------------------------------------

open import Arbor.Core.Params using (Params)

module Arbor.Core.Eval (P : Params) where

open import Arbor.Core.Store P public

open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Membership.Propositional.Properties using (∈-++⁺ˡ; ∈-++⁺ʳ)
open import Data.List.Relation.Unary.Any using (here)
open import Data.Maybe.Base using (Maybe; just; nothing)
open import Data.Nat.Base using (ℕ; zero; suc; _≤_; _⊔_; z≤n; s≤s)
open import Data.Nat.Properties using (≤-refl; ≤-trans; m≤m⊔n; m≤n⊔m; m≤n⇒m<n∨m≡n)
open import Data.Product using (_×_; _,_; ∃; ∃-syntax; proj₁; proj₂)
open import Data.Sum.Base using (inj₁; inj₂)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; sym; trans; cong; cong₂; subst)
open import Arbor.Prelude using (just-inj)

------------------------------------------------------------------------
-- Values (def:value)
--
-- "For closed terms the only weak-head values are abstractions."

data Value : Term → Set where
  v-lam : ∀ {t} → Value (lam t)

------------------------------------------------------------------------
-- The evaluation relation (def:eval)
--
-- There is no rule for a head `var i`: on closed terms it never occurs at an
-- evaluation position (lem:closed-no-stuck), and p4 classifies it Stuck only
-- to keep its function total. Divergence is the absence of any derivation.

infix 4 _⊢_⇓_
data _⊢_⇓_ (σ : Store) : Term → Term → Set where
  e-lam : ∀ {t} → σ ⊢ lam t ⇓ lam t
  e-ref : ∀ {h t v} → σ ⊢ h ⇝ t → σ ⊢ t ⇓ v → σ ⊢ ref h ⇓ v
  e-app : ∀ {t u t₀ vu v} →
          σ ⊢ t ⇓ lam t₀ → σ ⊢ u ⇓ vu → σ ⊢ beta t₀ vu ⇓ v →
          σ ⊢ app t u ⇓ v

-- Determinism (lem:determinism). "The rules are syntax-directed: the shape of
-- t (and, for E-Ref, Σ(h)) selects at most one rule. The premises are not
-- structurally smaller, so induct on the first derivation, inverting the
-- second at each step."
⇓-det : ∀ {σ t v₁ v₂} → σ ⊢ t ⇓ v₁ → σ ⊢ t ⇓ v₂ → v₁ ≡ v₂
⇓-det e-lam e-lam = refl
⇓-det (e-ref r₁ d₁) (e-ref r₂ d₂) with ⇝-func r₁ r₂
... | refl = ⇓-det d₁ d₂
⇓-det (e-app f₁ a₁ b₁) (e-app f₂ a₂ b₂) with ⇓-det f₁ f₂ | ⇓-det a₁ a₂
... | refl | refl = ⇓-det b₁ b₂

------------------------------------------------------------------------
-- Evaluation is stable under store growth
--
-- The general form of thm:stability. Note it needs NO well-formedness
-- premise on either store — see Meta.agda.
⇓-mono : ∀ {σ σ′ t v} → σ ⊑ σ′ → σ ⊢ t ⇓ v → σ′ ⊢ t ⇓ v
⇓-mono sub e-lam         = e-lam
⇓-mono sub (e-ref r d)   = e-ref (⇝-mono sub r) (⇓-mono sub d)
⇓-mono sub (e-app f a b) = e-app (⇓-mono sub f) (⇓-mono sub a) (⇓-mono sub b)

------------------------------------------------------------------------
-- Evaluation preserves closedness (lem:closed-no-stuck, corrected)
--
-- The paper states this with premises wf(Σ) and closed(t) only. That is FALSE:
-- `closed (ref h)` holds for every h (a reference carries no free variable),
-- while wf constrains only the references of *stored* entries, so nothing stops
-- h from reconstructing to an open term. Arbor.Core.Counterexamples proves the
-- paper's form refuted, with wf(Σ) discharged for an explicit two-entry store.
--
-- The repair is one premise: t's references must denote closed terms. That is
-- exactly Ingest's second premise and coherence clause (b), so it is available
-- wherever the paper uses the lemma — once the Eval transition carries
-- closed_Σ(h), which is why Config.t-eval has that premise.
--
-- The conclusion has to carry RefsClosed too, not just Closed: the App case
-- reduces to `beta t₀ vu` and needs the references of the *value* it got from
-- the function position, which the induction hypothesis must therefore supply.

RefsClosed : Store → Term → Set
RefsClosed σ t = ∀ {r} → r ∈ refsT t → ClosedIn σ r

⇓-closed : ∀ {σ t v} → WF σ → Closed t → RefsClosed σ t →
           σ ⊢ t ⇓ v → Closed v × RefsClosed σ v
⇓-closed wf ct rc e-lam = ct , rc
⇓-closed {σ} wf ct rc (e-ref {h} {t} r d) = ⇓-closed wf ct′ rc′ d
  where
  -- The target is closed by hypothesis, and ⇝-func says that target is t.
  ct′ : Closed t
  ct′ with rc (here refl)
  ... | (t″ , r″ , c″) = subst Closed (sym (⇝-func r r″)) c″
  -- Its own references are closed by wf (ii) — this is the clause's one use.
  rc′ : RefsClosed σ t
  rc′ mem = WF.tgt-closed wf (⇝-∈dom r) (t , r , mem)
⇓-closed {σ} wf (s-app ct₁ ct₂) rc (e-app {t} {u} {t₀} {vu} f a b) =
  ⇓-closed wf (beta-closed st₀ cvu) rcβ b
  where
  ih₁ = ⇓-closed wf ct₁ (λ mem → rc (∈-++⁺ˡ mem)) f
  ih₂ = ⇓-closed wf ct₂ (λ mem → rc (∈-++⁺ʳ (refsT t) mem)) a

  st₀ : 1 ⊢ t₀
  st₀ with proj₁ ih₁
  ... | s-lam d = d

  cvu : Closed vu
  cvu = proj₁ ih₂

  -- refsT (lam t₀) reduces to refsT t₀, so ih₁'s RefsClosed applies directly.
  rcβ : RefsClosed σ (beta t₀ vu)
  rcβ mem with beta-refs t₀ vu mem
  ... | inj₁ m = proj₂ ih₁ m
  ... | inj₂ m = proj₂ ih₂ m

------------------------------------------------------------------------
-- The fueled implementation (rem:nostepcache, cor:fuel)
--
-- p4's evaluator with default_step_limit = 10000. StepLimit is "no
-- derivation *yet*": not a defined result, so Cache-sound forbids storing it.

data Result : Set where
  value     : Term → Result
  stuck     : Result
  steplimit : Result

-- Reconstruction needs fuel too, for the same reason ⇝ is a relation: a
-- store is just a map, and nothing in its type stops its structural children
-- from looping.
--
-- The step functions are top-level rather than `where`-bound so that their
-- defining equations are available to the fuel lemmas below; a where-bound
-- helper cannot be named in a proof.

reconLam : Maybe Term → Maybe Term
reconLam nothing  = nothing
reconLam (just t) = just (lam t)

reconApp : Maybe Term → Maybe Term → Maybe Term
reconApp (just t) (just u) = just (app t u)
reconApp nothing  _        = nothing
reconApp (just _) nothing  = nothing

mutual
  recon : ℕ → Store → Hash → Maybe Term
  recon zero    σ h = nothing
  recon (suc k) σ h = reconNode k σ (σ h)

  reconNode : ℕ → Store → Maybe (Node Hash) → Maybe Term
  reconNode k σ nothing             = nothing
  reconNode k σ (just (nvar i))     = just (var i)
  reconNode k σ (just (nref r))     = just (ref r)
  reconNode k σ (just (nlam c))     = reconLam (recon k σ c)
  reconNode k σ (just (napp c₁ c₂)) = reconApp (recon k σ c₁) (recon k σ c₂)

mutual
  eval : ℕ → Store → Term → Result
  eval zero    σ t         = steplimit
  eval (suc k) σ (var i)   = stuck
  eval (suc k) σ (lam t)   = value (lam t)
  eval (suc k) σ (ref h)   = evalRef k σ (recon k σ h)
  eval (suc k) σ (app t u) = evalApp k σ (eval k σ t) (eval k σ u)

  evalRef : ℕ → Store → Maybe Term → Result
  evalRef k σ nothing  = stuck
  evalRef k σ (just t) = eval k σ t

  evalApp : ℕ → Store → Result → Result → Result
  evalApp k σ (value (lam t₀))  (value w)  = eval k σ (beta t₀ w)
  evalApp k σ (value (lam _))   stuck      = stuck
  evalApp k σ (value (lam _))   steplimit  = steplimit
  evalApp k σ (value (var _))   _          = stuck
  evalApp k σ (value (app _ _)) _          = stuck
  evalApp k σ (value (ref _))   _          = stuck
  evalApp k σ stuck             _          = stuck
  evalApp k σ steplimit         _          = steplimit

------------------------------------------------------------------------
-- cor:fuel — fuel-monotonicity, and the bridge to ⇓
--
-- "eval_k(t) = v ⟹ eval_{k+1}(t) = v, and eval_k(t) = v for some k iff
-- Σ ⊢ t ⇓ v. So StepLimit is precisely the non-stable outcome that
-- rem:nostepcache forbids caching, while Value/Stuck are stable."

private
  reconLam-inv : ∀ {m t} → reconLam m ≡ just t →
                 ∃[ t₁ ] ((m ≡ just t₁) × (t ≡ lam t₁))
  reconLam-inv {just t₁} refl = t₁ , refl , refl

  reconApp-inv : ∀ {m₁ m₂ t} → reconApp m₁ m₂ ≡ just t →
                 ∃[ t₁ ] ∃[ t₂ ] ((m₁ ≡ just t₁) × (m₂ ≡ just t₂) × (t ≡ app t₁ t₂))
  reconApp-inv {just t₁} {just t₂} refl = t₁ , t₂ , refl , refl , refl

  -- The only shape of evalApp that yields a value.
  evalApp-inv : ∀ {k σ r₁ r₂ v} → evalApp k σ r₁ r₂ ≡ value v →
                ∃[ t₀ ] ∃[ w ] ((r₁ ≡ value (lam t₀)) × (r₂ ≡ value w)
                              × (eval k σ (beta t₀ w) ≡ value v))
  evalApp-inv {r₁ = value (lam t₀)} {value w} eq = t₀ , w , refl , refl , eq

  value-inj : ∀ {a b} → value a ≡ value b → a ≡ b
  value-inj refl = refl

------------------------------------------------------------------------
-- Reconstruction is monotone in fuel, sound, and complete

recon-mono : ∀ k σ h {t} → recon k σ h ≡ just t → recon (suc k) σ h ≡ just t
recon-mono (suc k) σ h {t} eq with σ h
... | just (nvar i)     = eq
... | just (nref r)     = eq
... | just (nlam c)     with reconLam-inv eq
...   | (t₁ , e₁ , refl) rewrite recon-mono k σ c e₁ = refl
recon-mono (suc k) σ h {t} eq | just (napp c₁ c₂) with reconApp-inv eq
...   | (t₁ , t₂ , e₁ , e₂ , refl)
        rewrite recon-mono k σ c₁ e₁ | recon-mono k σ c₂ e₂ = refl

recon-mono-≤ : ∀ {k k′} σ h {t} → k ≤ k′ → recon k σ h ≡ just t →
               recon k′ σ h ≡ just t
recon-mono-≤ {k} {k′} σ h {t} = go k k′
  where
  -- Induction on the TARGET fuel: either we are already there, or step down.
  go : ∀ a b → a ≤ b → recon a σ h ≡ just t → recon b σ h ≡ just t
  go zero zero    z≤n e = e
  go a    (suc b) le  e with m≤n⇒m<n∨m≡n le
  ...  | inj₂ refl     = e
  ...  | inj₁ (s≤s l)  = recon-mono b σ h (go a b l e)

recon-sound : ∀ k σ h {t} → recon k σ h ≡ just t → σ ⊢ h ⇝ t
recon-sound (suc k) σ h {t} eq with σ h in look
... | just (nvar i)     rewrite sym (just-inj eq) = r-var look
... | just (nref r)     rewrite sym (just-inj eq) = r-ref look
... | just (nlam c)     with reconLam-inv eq
...   | (t₁ , e₁ , refl) = r-lam look (recon-sound k σ c e₁)
recon-sound (suc k) σ h {t} eq | just (napp c₁ c₂) with reconApp-inv eq
...   | (t₁ , t₂ , e₁ , e₂ , refl) =
        r-app look (recon-sound k σ c₁ e₁) (recon-sound k σ c₂ e₂)

recon-complete : ∀ {σ h t} → σ ⊢ h ⇝ t → ∃[ k ] (recon k σ h ≡ just t)
recon-complete {σ} {h} (r-var e) = 1 , cong (reconNode 0 σ) e
recon-complete {σ} {h} (r-ref e) = 1 , cong (reconNode 0 σ) e
recon-complete {σ} {h} (r-lam {c = c} e d) with recon-complete d
... | (k , ek) =
      suc k , trans (cong (reconNode k σ) e) (cong reconLam ek)
recon-complete {σ} {h} (r-app {c₁ = c₁} {c₂ = c₂} e d₁ d₂)
  with recon-complete d₁ | recon-complete d₂
... | (k₁ , e₁) | (k₂ , e₂) =
      suc (k₁ ⊔ k₂) ,
      trans (cong (reconNode (k₁ ⊔ k₂) σ) e)
            (cong₂ reconApp (recon-mono-≤ σ c₁ (m≤m⊔n k₁ k₂) e₁)
                            (recon-mono-≤ σ c₂ (m≤n⊔m k₁ k₂) e₂))

------------------------------------------------------------------------
-- Evaluation is monotone in fuel, sound, and complete

eval-mono : ∀ k σ t {v} → eval k σ t ≡ value v → eval (suc k) σ t ≡ value v
eval-mono (suc k) σ (lam t)   eq = eq
eval-mono (suc k) σ (ref h)   eq with recon k σ h in look
... | nothing  with eq
...   | ()
eval-mono (suc k) σ (ref h)   eq | just t =
      trans (cong (evalRef (suc k) σ) (recon-mono k σ h look))
            (eval-mono k σ t eq)
eval-mono (suc k) σ (app t u) eq with evalApp-inv eq
... | (t₀ , w , e₁ , e₂ , e₃) =
      trans (cong₂ (evalApp (suc k) σ) (eval-mono k σ t e₁) (eval-mono k σ u e₂))
            (eval-mono k σ (beta t₀ w) e₃)

eval-mono-≤ : ∀ {k k′} σ t {v} → k ≤ k′ → eval k σ t ≡ value v →
              eval k′ σ t ≡ value v
eval-mono-≤ {k} {k′} σ t {v} = go k k′
  where
  go : ∀ a b → a ≤ b → eval a σ t ≡ value v → eval b σ t ≡ value v
  go zero zero    z≤n e = e
  go a    (suc b) le  e with m≤n⇒m<n∨m≡n le
  ...  | inj₂ refl     = e
  ...  | inj₁ (s≤s l)  = eval-mono b σ t (go a b l e)

eval-sound : ∀ k σ t {v} → eval k σ t ≡ value v → σ ⊢ t ⇓ v
eval-sound (suc k) σ (lam t)   eq rewrite sym (value-inj eq) = e-lam
eval-sound (suc k) σ (ref h)   eq with recon k σ h in look
... | nothing with eq
...   | ()
eval-sound (suc k) σ (ref h)   eq | just t =
      e-ref (recon-sound k σ h look) (eval-sound k σ t eq)
eval-sound (suc k) σ (app t u) eq with evalApp-inv eq
... | (t₀ , w , e₁ , e₂ , e₃) =
      e-app (eval-sound k σ t e₁) (eval-sound k σ u e₂)
            (eval-sound k σ (beta t₀ w) e₃)

eval-complete : ∀ {σ t v} → σ ⊢ t ⇓ v → ∃[ k ] (eval k σ t ≡ value v)
eval-complete e-lam = 1 , refl
eval-complete {σ} (e-ref {h} {t} r d) with recon-complete r | eval-complete d
... | (k₁ , e₁) | (k₂ , e₂) =
      suc (k₁ ⊔ k₂) ,
      trans (cong (evalRef (k₁ ⊔ k₂) σ) (recon-mono-≤ σ h (m≤m⊔n k₁ k₂) e₁))
            (eval-mono-≤ σ t (m≤n⊔m k₁ k₂) e₂)
eval-complete {σ} (e-app {t} {u} {t₀} {w} f a b)
  with eval-complete f | eval-complete a | eval-complete b
... | (k₁ , e₁) | (k₂ , e₂) | (k₃ , e₃) =
      suc K ,
      trans (cong₂ (evalApp K σ)
              (eval-mono-≤ σ t (m≤m⊔n k₁ (k₂ ⊔ k₃)) e₁)
              (eval-mono-≤ σ u (≤-trans (m≤m⊔n k₂ k₃) (m≤n⊔m k₁ (k₂ ⊔ k₃))) e₂))
            (eval-mono-≤ σ (beta t₀ w)
              (≤-trans (m≤n⊔m k₂ k₃) (m≤n⊔m k₁ (k₂ ⊔ k₃))) e₃)
  where K = k₁ ⊔ (k₂ ⊔ k₃)
