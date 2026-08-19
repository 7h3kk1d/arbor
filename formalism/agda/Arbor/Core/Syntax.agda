{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part I — the object language (arbor-core §sec:lang)
--
-- Core terms (def:core), well-scopedness (def:closed), and shift /
-- substitution / β-contraction (def:beta). Surface terms too, since they
-- share nothing with the store and belong beside the core syntax.
--
-- decisions.md 2026-08-05 (extrinsic scoping): Term is *not* indexed by a
-- scope level. Scoping is the paper's inductive relation ⊢ₙ t, and
-- closedness is ⊢₀ t, carried as an explicit proof exactly where the paper
-- carries it as a premise (Ingest, wf clause (ii), Bind/Rebind). The reason
-- is the store: storage is shallow and hash-consed, so dom(Σ) holds every
-- subterm ever ingested including open ones (rem:nosort — `Var 1` under a
-- `Lam` is an entry), and reconstruct at such a hash yields an open term. An
-- indexed `Term : ℕ → Set` would force reconstruct's codomain to package an
-- existential level that is not even canonical. α-equivalence is definitional
-- either way: that comes from de Bruijn, not from the indexing, so
-- thm:alpha's (⇐) half stays free.
--
-- Module parameter: only Hash. Names never reach the core syntax
-- (prop:namefree), and this module not mentioning Name is that fact.
------------------------------------------------------------------------

open import Relation.Binary.Definitions using (DecidableEquality)

module Arbor.Core.Syntax (Hash : Set) where

open import Data.Empty using (⊥-elim)
open import Data.List.Base using (List; []; _∷_; _++_; length)
open import Data.List.Membership.Propositional using (_∈_)
open import Data.List.Relation.Unary.Any using (here; there)
open import Data.List.Membership.Propositional.Properties using (∈-++⁻; ∈-++⁺ˡ; ∈-++⁺ʳ)
open import Data.Nat.Base using (ℕ; zero; suc; _+_; _≤_; _<_; pred; z≤n; s≤s)
open import Data.Nat.Properties
  using (_≤?_; ≤-refl; ≤-trans; ≤-antisym; ≤-pred; <-irrefl; <⇒≤; <⇒≢; ≰⇒>; +-comm)
  renaming (_≟_ to _≟ℕ_)
open import Data.Product using (Σ; _,_; ∃)
open import Data.Sum.Base using (_⊎_; inj₁; inj₂)
open import Relation.Binary.PropositionalEquality
  using (_≡_; _≢_; refl; sym; trans; cong; cong₂; subst)
open import Relation.Nullary.Decidable.Core using (yes; no)

------------------------------------------------------------------------
-- Core terms (def:core)

data Term : Set where
  var : ℕ → Term
  lam : Term → Term
  app : Term → Term → Term
  ref : Hash → Term

------------------------------------------------------------------------
-- Well-scopedness and closedness (def:closed)
--
-- ⊢ₙ t: "t is scoped under n binders". Note s-ref: a reference is scoped at
-- every level, which is why unfolding one later needs no index adjustment.

infix 4 _⊢_
data _⊢_ : ℕ → Term → Set where
  s-var : ∀ {n i} → i < n     → n ⊢ var i
  s-lam : ∀ {n t} → suc n ⊢ t → n ⊢ lam t
  s-app : ∀ {n t u} → n ⊢ t → n ⊢ u → n ⊢ app t u
  s-ref : ∀ {n h}             → n ⊢ ref h

Closed : Term → Set
Closed t = 0 ⊢ t

------------------------------------------------------------------------
-- Shift, substitution, contraction (def:beta)
--
-- The paper writes one operation ↑(d,c) with d ∈ ℤ. It only ever uses
-- d = 1 and d = -1 (in `beta`), so rather than carry ℤ and truncated
-- subtraction we present the two directions separately: ↑ for ↑(d,c) with
-- d : ℕ, and ↓ for ↑(-1,c). Transcription note, not a semantic change.

↑ : ℕ → ℕ → Term → Term
↑ d c (var k) with c ≤? k
... | yes _ = var (k + d)
... | no  _ = var k
↑ d c (lam t)   = lam (↑ d (suc c) t)
↑ d c (app t u) = app (↑ d c t) (↑ d c u)
↑ d c (ref h)   = ref h

↓ : ℕ → Term → Term
↓ c (var k) with c ≤? k
... | yes _ = var (pred k)
... | no  _ = var k
↓ c (lam t)   = lam (↓ (suc c) t)
↓ c (app t u) = app (↓ c t) (↓ c u)
↓ c (ref h)   = ref h

infix 8 [_↦_]_
[_↦_]_ : ℕ → Term → Term → Term
[ j ↦ s ] (var k) with k ≟ℕ j
... | yes _ = s
... | no  _ = var k
[ j ↦ s ] (lam t)   = lam ([ suc j ↦ ↑ 1 0 s ] t)
[ j ↦ s ] (app t u) = app ([ j ↦ s ] t) ([ j ↦ s ] u)
[ j ↦ s ] (ref h)   = ref h

-- beta(t, v) = ↑(-1,0)( [0 ↦ ↑(1,0) v] t ), the TAPL convention.
beta : Term → Term → Term
beta t v = ↓ 0 ([ 0 ↦ ↑ 1 0 v ] t)

------------------------------------------------------------------------
-- References of a term (def:refs)
--
-- "Since reconstruct stops at ref leaves, refs collects a definition's
-- direct citations, not their transitive closure."

refsT : Term → List Hash
refsT (var _)   = []
refsT (lam t)   = refsT t
refsT (app t u) = refsT t ++ refsT u
refsT (ref h)   = h ∷ []

-- A dependent map over the reference leaves: the replacement for each leaf gets
-- a proof that the leaf really occurs. Needed by the migration rewrite, whose
-- recursive call at a reference is justified by that very membership (it is
-- what makes the target a predecessor in the reference graph).
substRefsD : (t : Term) → (∀ r → r ∈ refsT t → Hash) → Term
substRefsD (var i)   f = var i
substRefsD (lam t)   f = lam (substRefsD t f)
substRefsD (app t u) f = app (substRefsD t (λ r m → f r (∈-++⁺ˡ m)))
                             (substRefsD u (λ r m → f r (∈-++⁺ʳ (refsT t) m)))
substRefsD (ref h)   f = ref (f h (here refl))

-- Two facts about substRefsD the migration rewrite needs.
--
-- The congruence is what makes the rewrite well defined at all: ρ is built by
-- well-founded recursion, so its step function is only determined up to the
-- accessibility proof handed to it, and the two agree only pointwise.
substRefsD-cong : ∀ t {f g : ∀ r → r ∈ refsT t → Hash} →
                  (∀ r m → f r m ≡ g r m) → substRefsD t f ≡ substRefsD t g
substRefsD-cong (var i)   eq = refl
substRefsD-cong (lam t)   eq = cong lam (substRefsD-cong t eq)
substRefsD-cong (app t u) eq =
  cong₂ app (substRefsD-cong t (λ r m → eq r (∈-++⁺ˡ m)))
            (substRefsD-cong u (λ r m → eq r (∈-++⁺ʳ (refsT t) m)))
substRefsD-cong (ref h)   eq = cong ref (eq h (here refl))

-- Rewriting references preserves scoping, at every level. A reference leaf is
-- scoped under any number of binders (s-ref), so exchanging one for another
-- cannot free a variable. In particular a rewritten CLOSED term is closed —
-- which is what lets the migrated store's reference targets be shown closed
-- without tracking the order in which images were registered.
substRefsD-scoped : ∀ {n} t {f : ∀ r → r ∈ refsT t → Hash} →
                    n ⊢ t → n ⊢ substRefsD t f
substRefsD-scoped (var i)   (s-var lt)      = s-var lt
substRefsD-scoped (lam t)   (s-lam sc)      = s-lam (substRefsD-scoped t sc)
substRefsD-scoped (app t u) (s-app sc₁ sc₂) =
  s-app (substRefsD-scoped t sc₁) (substRefsD-scoped u sc₂)
substRefsD-scoped (ref h)   s-ref           = s-ref

-- And every reference of the rewritten term is the image of a reference of the
-- original. This is the provenance fact the acyclicity half of thm:migosc needs:
-- an edge out of a rewritten entry cannot point anywhere the original did not.
substRefsD-refs : ∀ t {f : ∀ r → r ∈ refsT t → Hash} {r′} →
                  r′ ∈ refsT (substRefsD t f) →
                  ∃ λ r → Σ (r ∈ refsT t) (λ m → f r m ≡ r′)
substRefsD-refs (var i)   ()
substRefsD-refs (lam t)   mem = substRefsD-refs t mem
substRefsD-refs (app t u) {f} mem
  with ∈-++⁻ (refsT (substRefsD t (λ r m → f r (∈-++⁺ˡ m)))) mem
... | inj₁ m₁ with substRefsD-refs t m₁
...   | (r , mr , eq) = r , ∈-++⁺ˡ mr , eq
substRefsD-refs (app t u) {f} mem | inj₂ m₂ with substRefsD-refs u m₂
...   | (r , mr , eq) = r , ∈-++⁺ʳ (refsT t) mr , eq
substRefsD-refs (ref h)   {f} (here refl) = h , here refl , refl
substRefsD-refs (ref h)   (there ())

-- Surface terms (def:surface) are deliberately NOT here: they mention Name,
-- and this module's parameter list is the mechanized form of prop:namefree.
-- See Arbor.Core.Surface.

------------------------------------------------------------------------
-- Scoping lemmas
--
-- The price of extrinsic scoping (decisions.md 2026-08-05): under intrinsic
-- indexing these would be typing facts. They are the workhorses of
-- beta-closed, and through it of lem:closed-no-stuck and thm:wf's Eval case.

private
  ≤suc : ∀ {m n} → m ≤ n → m ≤ suc n
  ≤suc z≤n       = z≤n
  ≤suc (s≤s m≤n) = s≤s (≤suc m≤n)

-- Scoping is monotone in the binder count.
⊢-mono : ∀ {t m n} → m ≤ n → m ⊢ t → n ⊢ t
⊢-mono le (s-var i<m)  = s-var (≤-trans i<m le)
⊢-mono le (s-lam d)    = s-lam (⊢-mono (s≤s le) d)
⊢-mono le (s-app d₁ d₂) = s-app (⊢-mono le d₁) (⊢-mono le d₂)
⊢-mono le s-ref        = s-ref

-- Shifting at a cutoff at or above the scope level changes nothing that is in
-- scope, so the level is preserved. With n = c = 0 this says a closed term
-- survives ↑ unchanged in level, which is what beta needs of its argument.
↑-preserves : ∀ {t n c d} → n ≤ c → n ⊢ t → n ⊢ ↑ d c t
↑-preserves {c = c} {d} le (s-var {i = k} k<n) with c ≤? k
... | yes c≤k = ⊥-elim (<-irrefl refl (≤-trans (≤-trans k<n le) c≤k))
... | no  _   = s-var k<n
↑-preserves le (s-lam d)     = s-lam (↑-preserves (s≤s le) d)
↑-preserves le (s-app d₁ d₂) = s-app (↑-preserves le d₁) (↑-preserves le d₂)
↑-preserves le s-ref         = s-ref

-- Shifting up by one raises the level by one, whatever the cutoff.
↑¹-scoped : ∀ {t n} c → n ⊢ t → suc n ⊢ ↑ 1 c t
↑¹-scoped {n = n} c (s-var {i = k} k<n) with c ≤? k
... | yes _ = s-var (subst (λ m → m < suc n) (sym (+-comm k 1)) (s≤s k<n))
... | no  _ = s-var (≤suc k<n)
↑¹-scoped c (s-lam d)     = s-lam (↑¹-scoped (suc c) d)
↑¹-scoped c (s-app d₁ d₂) = s-app (↑¹-scoped c d₁) (↑¹-scoped c d₂)
↑¹-scoped c s-ref         = s-ref

-- Substituting a term of the same level preserves the level.
subst-scoped : ∀ {t s n} j → n ⊢ t → n ⊢ s → n ⊢ [ j ↦ s ] t
subst-scoped {n = n} j (s-var {i = k} k<n) ss with k ≟ℕ j
... | yes _ = ss
... | no  _ = s-var k<n
subst-scoped j (s-lam d)     ss = s-lam (subst-scoped (suc j) d (↑¹-scoped 0 ss))
subst-scoped j (s-app d₁ d₂) ss = s-app (subst-scoped j d₁ ss) (subst-scoped j d₂ ss)
subst-scoped j s-ref         ss = s-ref

------------------------------------------------------------------------
-- "index j does not occur free"
--
-- Needed because beta shifts down after substituting: ↓ 0 is only
-- level-lowering on a term that no longer mentions index 0, which is exactly
-- what substituting at 0 achieves.

data NotFree : ℕ → Term → Set where
  nf-var : ∀ {j k} → k ≢ j → NotFree j (var k)
  nf-lam : ∀ {j t} → NotFree (suc j) t → NotFree j (lam t)
  nf-app : ∀ {j t u} → NotFree j t → NotFree j u → NotFree j (app t u)
  nf-ref : ∀ {j h} → NotFree j (ref h)

-- Anything in scope under n binders mentions no index at or above n.
scoped-notfree : ∀ {t n j} → n ⊢ t → n ≤ j → NotFree j t
scoped-notfree (s-var k<n)   le = nf-var (<⇒≢ (≤-trans k<n le))
scoped-notfree (s-lam d)     le = nf-lam (scoped-notfree d (s≤s le))
scoped-notfree (s-app d₁ d₂) le = nf-app (scoped-notfree d₁ le) (scoped-notfree d₂ le)
scoped-notfree s-ref         le = nf-ref

-- Substituting a closed term at j removes j. The substituted term stays closed
-- as the traversal shifts it under binders (↑-preserves with n = c = 0).
subst-notfree : ∀ {s} t j → 0 ⊢ s → NotFree j ([ j ↦ s ] t)
subst-notfree (var k) j ss with k ≟ℕ j
... | yes _  = scoped-notfree ss z≤n
... | no ¬p  = nf-var ¬p
subst-notfree (lam t)   j ss = nf-lam (subst-notfree t (suc j) (↑-preserves z≤n ss))
subst-notfree (app t u) j ss = nf-app (subst-notfree t j ss) (subst-notfree u j ss)
subst-notfree (ref h)   j ss = nf-ref

-- Shifting down at n lowers the level, provided n is not free.
↓-scoped : ∀ {t n} → suc n ⊢ t → NotFree n t → n ⊢ ↓ n t
↓-scoped {n = n} (s-var {i = k} k<sn) (nf-var k≢n) with n ≤? k
... | yes n≤k = ⊥-elim (k≢n (≤-antisym (≤-pred k<sn) n≤k))
... | no ¬n≤k = s-var (≰⇒> ¬n≤k)
↓-scoped (s-lam d)     (nf-lam nf)      = s-lam (↓-scoped d nf)
↓-scoped (s-app d₁ d₂) (nf-app nf₁ nf₂) = s-app (↓-scoped d₁ nf₁) (↓-scoped d₂ nf₂)
↓-scoped s-ref         nf-ref           = s-ref

------------------------------------------------------------------------
-- β-contraction preserves closedness
--
-- The workhorse of lem:closed-no-stuck. Note the shape: the argument is
-- closed, the body is scoped under exactly the one binder being consumed.

beta-closed : ∀ {t v} → 1 ⊢ t → Closed v → Closed (beta t v)
beta-closed {t} {v} st sv =
  ↓-scoped (subst-scoped 0 st (⊢-mono z≤n s′-closed))
           (subst-notfree t 0 s′-closed)
  where
  s′-closed : 0 ⊢ ↑ 1 0 v
  s′-closed = ↑-preserves z≤n sv

------------------------------------------------------------------------
-- References survive the term operations
--
-- Needed to carry "every reference denotes a closed term" through β-reduction
-- (see Arbor.Core.Eval.⇓-closed).

↑-refs : ∀ d c t → refsT (↑ d c t) ≡ refsT t
↑-refs d c (var k) with c ≤? k
... | yes _ = refl
... | no  _ = refl
↑-refs d c (lam t)   = ↑-refs d (suc c) t
↑-refs d c (app t u) = cong₂ _++_ (↑-refs d c t) (↑-refs d c u)
↑-refs d c (ref h)   = refl

↓-refs : ∀ c t → refsT (↓ c t) ≡ refsT t
↓-refs c (var k) with c ≤? k
... | yes _ = refl
... | no  _ = refl
↓-refs c (lam t)   = ↓-refs (suc c) t
↓-refs c (app t u) = cong₂ _++_ (↓-refs c t) (↓-refs c u)
↓-refs c (ref h)   = refl

subst-refs : ∀ {r} t s j → r ∈ refsT ([ j ↦ s ] t) →
             (r ∈ refsT t) ⊎ (r ∈ refsT s)
subst-refs (var k) s j mem with k ≟ℕ j
... | yes _ = inj₂ mem
... | no  _ = inj₁ mem
subst-refs (lam t) s j mem
  with subst-refs t (↑ 1 0 s) (suc j) mem
... | inj₁ m = inj₁ m
... | inj₂ m = inj₂ (subst (λ hs → _ ∈ hs) (↑-refs 1 0 s) m)
subst-refs (app t u) s j mem with ∈-++⁻ (refsT ([ j ↦ s ] t)) mem
... | inj₁ m with subst-refs t s j m
...    | inj₁ m′ = inj₁ (∈-++⁺ˡ m′)
...    | inj₂ m′ = inj₂ m′
subst-refs (app t u) s j mem | inj₂ m with subst-refs u s j m
...    | inj₁ m′ = inj₁ (∈-++⁺ʳ (refsT t) m′)
...    | inj₂ m′ = inj₂ m′
subst-refs (ref h) s j mem = inj₁ mem

beta-refs : ∀ {r} t v → r ∈ refsT (beta t v) →
            (r ∈ refsT t) ⊎ (r ∈ refsT v)
beta-refs t v mem with subst-refs t (↑ 1 0 v) 0
                         (subst (λ hs → _ ∈ hs) (↓-refs 0 ([ 0 ↦ ↑ 1 0 v ] t)) mem)
... | inj₁ m = inj₁ m
... | inj₂ m = inj₂ (subst (λ hs → _ ∈ hs) (↑-refs 1 0 v) m)
