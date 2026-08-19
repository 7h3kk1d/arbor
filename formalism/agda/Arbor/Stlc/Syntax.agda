{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Part I — the typed object language (arbor-stlc §sec:lang)
--
-- Core terms (def:core) and types (def:ty) are the nine shapes of the
-- signature, recovered as pattern synonyms; well-sortedness (the content
-- def:reconstruct folds into definedness) and scoping (def:closed, carried over
-- from arbor-core) are predicates over them.
--
-- Everything the store layer needs is already proved generically. What lives
-- here is what the signature does not model: sorts, binding, and β.
------------------------------------------------------------------------

open import Arbor.Stlc.Node using
  ( stlcSig; StlcShape
  ; svar; slamT; sapp; strue; sfalse; sif; sref; sTBool; sTArr )
import Arbor.Generic.Syntax as GS

module Arbor.Stlc.Syntax (Hash : Set) where

open GS.WithHash stlcSig Hash public

open import Data.List.Base using (List; []; _∷_; _++_)
open import Data.Empty using (⊥-elim)
open import Data.Nat.Base using (ℕ; zero; suc; _+_; _≤_; _<_; pred; z≤n; s≤s)
open import Data.Nat.Properties
  using (_≤?_; ≤-refl; ≤-trans; <-irrefl) renaming (_≟_ to _≟ℕ_)
open import Relation.Nullary using (¬_)
open import Relation.Nullary.Decidable.Core using (yes; no)
open import Data.Vec.Base using (Vec) renaming ([] to ⟦⟧; _∷_ to _◂_)
open import Relation.Binary.PropositionalEquality using (_≡_; refl; cong; cong₂)

------------------------------------------------------------------------
-- def:core and def:ty, as the shapes of the signature
--
-- Types are terms of the same datatype; what separates them is IsTy below.
-- That is the one presentational divergence from the paper, which separates
-- them by having two reconstruction functions instead.

pattern var i     = ⟨ svar i , ⟦⟧ , ⟦⟧ ⟩
pattern lamT T t  = ⟨ slamT , T ◂ t ◂ ⟦⟧ , ⟦⟧ ⟩
pattern app t u   = ⟨ sapp , t ◂ u ◂ ⟦⟧ , ⟦⟧ ⟩
pattern true      = ⟨ strue , ⟦⟧ , ⟦⟧ ⟩
pattern false     = ⟨ sfalse , ⟦⟧ , ⟦⟧ ⟩
pattern iff c a b = ⟨ sif , c ◂ a ◂ b ◂ ⟦⟧ , ⟦⟧ ⟩
pattern ref h     = ⟨ sref , ⟦⟧ , h ◂ ⟦⟧ ⟩
pattern TBool     = ⟨ sTBool , ⟦⟧ , ⟦⟧ ⟩
pattern TArr A B  = ⟨ sTArr , A ◂ B ◂ ⟦⟧ , ⟦⟧ ⟩

------------------------------------------------------------------------
-- Well-sortedness
--
-- def:reconstruct's content, as a predicate: "a λ whose annotation hash
-- resolves to a term entry simply fails to reconstruct" becomes "a λ whose
-- annotation is not a type is not a well-sorted term".

mutual
  data IsTy : Term → Set where
    ty-bool : IsTy TBool
    ty-arr  : ∀ {A B} → IsTy A → IsTy B → IsTy (TArr A B)

  data IsTm : Term → Set where
    tm-var : ∀ {i} → IsTm (var i)
    tm-lam : ∀ {T t} → IsTy T → IsTm t → IsTm (lamT T t)
    tm-app : ∀ {t u} → IsTm t → IsTm u → IsTm (app t u)
    tm-true  : IsTm true
    tm-false : IsTm false
    tm-if  : ∀ {c a b} → IsTm c → IsTm a → IsTm b → IsTm (iff c a b)
    tm-ref : ∀ {h} → IsTm (ref h)

-- "Type entries are isolated vertices of the reference graph, so they have no
-- callers" (def:reconstruct) — here by construction, since the type shapes have
-- reference arity zero.
ty-no-refs : ∀ {T} → IsTy T → refsT T ≡ []
ty-no-refs ty-bool = refl
ty-no-refs (ty-arr dA dB) rewrite ty-no-refs dA | ty-no-refs dB = refl

------------------------------------------------------------------------
-- Well-scopedness (arbor-core def:closed, extended pointwise)
--
-- "Annotations contain no term variables and pass through untouched"
-- (def:core), so a type is scoped at every level — which is why one predicate
-- serves both sorts and the generic store layer needs no splitting.

infix 4 _⊢_
data _⊢_ : ℕ → Term → Set where
  s-var   : ∀ {n i} → i < n → n ⊢ var i
  s-lam   : ∀ {n T t} → n ⊢ T → suc n ⊢ t → n ⊢ lamT T t
  s-app   : ∀ {n t u} → n ⊢ t → n ⊢ u → n ⊢ app t u
  s-true  : ∀ {n} → n ⊢ true
  s-false : ∀ {n} → n ⊢ false
  s-if    : ∀ {n c a b} → n ⊢ c → n ⊢ a → n ⊢ b → n ⊢ iff c a b
  s-ref   : ∀ {n h} → n ⊢ ref h
  s-bool  : ∀ {n} → n ⊢ TBool
  s-arr   : ∀ {n A B} → n ⊢ A → n ⊢ B → n ⊢ TArr A B

Closed : Term → Set
Closed t = 0 ⊢ t

-- A type is closed at every level.
ty-scoped : ∀ {T} n → IsTy T → n ⊢ T
ty-scoped n ty-bool         = s-bool
ty-scoped n (ty-arr dA dB)  = s-arr (ty-scoped n dA) (ty-scoped n dB)

------------------------------------------------------------------------
-- Shift, substitution, contraction (arbor-core def:beta, extended pointwise)
--
-- "Annotations contain no term variables and pass through untouched, ref stays
-- inert, and the new leaves true/false are closed" (def:core). The annotation
-- of a lamT is a structural child like the body, but only the body goes under
-- the binder — which is the one place the two structural positions of a shape
-- are treated differently.
--
-- As in arbor-core the paper's single ↑(d,c) with d ∈ ℤ is presented as two
-- operations, since it only ever uses d = 1 and d = -1.

↑ : ℕ → ℕ → Term → Term
↑ d c (var k) with c ≤? k
... | yes _ = var (k + d)
... | no  _ = var k
↑ d c (lamT T t)  = lamT (↑ d c T) (↑ d (suc c) t)
↑ d c (app t u)   = app (↑ d c t) (↑ d c u)
↑ d c true        = true
↑ d c false       = false
↑ d c (iff a b e) = iff (↑ d c a) (↑ d c b) (↑ d c e)
↑ d c (ref h)     = ref h
↑ d c TBool       = TBool
↑ d c (TArr A B)  = TArr (↑ d c A) (↑ d c B)

↓ : ℕ → Term → Term
↓ c (var k) with c ≤? k
... | yes _ = var (pred k)
... | no  _ = var k
↓ c (lamT T t)  = lamT (↓ c T) (↓ (suc c) t)
↓ c (app t u)   = app (↓ c t) (↓ c u)
↓ c true        = true
↓ c false       = false
↓ c (iff a b e) = iff (↓ c a) (↓ c b) (↓ c e)
↓ c (ref h)     = ref h
↓ c TBool       = TBool
↓ c (TArr A B)  = TArr (↓ c A) (↓ c B)

infix 8 [_↦_]_
[_↦_]_ : ℕ → Term → Term → Term
[ j ↦ s ] (var k) with k ≟ℕ j
... | yes _ = s
... | no  _ = var k
[ j ↦ s ] (lamT T t)  = lamT ([ j ↦ s ] T) ([ suc j ↦ ↑ 1 0 s ] t)
[ j ↦ s ] (app t u)   = app ([ j ↦ s ] t) ([ j ↦ s ] u)
[ j ↦ s ] true        = true
[ j ↦ s ] false       = false
[ j ↦ s ] (iff a b e) = iff ([ j ↦ s ] a) ([ j ↦ s ] b) ([ j ↦ s ] e)
[ j ↦ s ] (ref h)     = ref h
[ j ↦ s ] TBool       = TBool
[ j ↦ s ] (TArr A B)  = TArr ([ j ↦ s ] A) ([ j ↦ s ] B)

beta : Term → Term → Term
beta t v = ↓ 0 ([ 0 ↦ ↑ 1 0 v ] t)

------------------------------------------------------------------------
-- Shifting is the identity on terms already in scope
--
-- This is what makes the substitution lemma tractable here. Preservation is
-- stated at the empty context (thm:pres), so the value being substituted is
-- CLOSED — and a closed term is unmoved by ↑ and ↓ at any cutoff, so the
-- incremental shifting inside [_↦_] is a no-op throughout the induction.

↑-id : ∀ {t n c} d → n ≤ c → n ⊢ t → ↑ d c t ≡ t
↑-id {c = c} d le (s-var {i = k} k<n) with c ≤? k
... | yes c≤k = ⊥-elim (<-irrefl refl (≤-trans (≤-trans k<n le) c≤k))
... | no  _   = refl
↑-id d le (s-lam sT st)     = cong₂ lamT (↑-id d le sT) (↑-id d (s≤s le) st)
↑-id d le (s-app s₁ s₂)     = cong₂ app (↑-id d le s₁) (↑-id d le s₂)
↑-id d le s-true            = refl
↑-id d le s-false           = refl
↑-id d le (s-if s₁ s₂ s₃)   =
  cong₃ iff (↑-id d le s₁) (↑-id d le s₂) (↑-id d le s₃)
  where
  cong₃ : ∀ {A : Set} (f : A → A → A → A) {a a′ b b′ c′ c″} →
          a ≡ a′ → b ≡ b′ → c′ ≡ c″ → f a b c′ ≡ f a′ b′ c″
  cong₃ f refl refl refl = refl
↑-id d le s-ref             = refl
↑-id d le s-bool            = refl
↑-id d le (s-arr sA sB)     = cong₂ TArr (↑-id d le sA) (↑-id d le sB)

↓-id : ∀ {t n c} → n ≤ c → n ⊢ t → ↓ c t ≡ t
↓-id {c = c} le (s-var {i = k} k<n) with c ≤? k
... | yes c≤k = ⊥-elim (<-irrefl refl (≤-trans (≤-trans k<n le) c≤k))
... | no  _   = refl
↓-id le (s-lam sT st)   = cong₂ lamT (↓-id le sT) (↓-id (s≤s le) st)
↓-id le (s-app s₁ s₂)   = cong₂ app (↓-id le s₁) (↓-id le s₂)
↓-id le s-true          = refl
↓-id le s-false         = refl
↓-id le (s-if s₁ s₂ s₃) = cong₃ iff (↓-id le s₁) (↓-id le s₂) (↓-id le s₃)
  where
  cong₃ : ∀ {A : Set} (f : A → A → A → A) {a a′ b b′ c′ c″} →
          a ≡ a′ → b ≡ b′ → c′ ≡ c″ → f a b c′ ≡ f a′ b′ c″
  cong₃ f refl refl refl = refl
↓-id le s-ref           = refl
↓-id le s-bool          = refl
↓-id le (s-arr sA sB)   = cong₂ TArr (↓-id le sA) (↓-id le sB)

-- And substitution is the identity at an index already out of scope.
subst-id : ∀ {t n j s} → n ≤ j → n ⊢ t → [ j ↦ s ] t ≡ t
subst-id {j = j} le (s-var {i = k} k<n) with k ≟ℕ j
... | yes refl = ⊥-elim (<-irrefl refl (≤-trans k<n le))
... | no  _    = refl
subst-id le (s-lam sT st)   = cong₂ lamT (subst-id le sT) (subst-id (s≤s le) st)
subst-id le (s-app s₁ s₂)   = cong₂ app (subst-id le s₁) (subst-id le s₂)
subst-id le s-true          = refl
subst-id le s-false         = refl
subst-id le (s-if s₁ s₂ s₃) = cong₃ iff (subst-id le s₁) (subst-id le s₂) (subst-id le s₃)
  where
  cong₃ : ∀ {A : Set} (f : A → A → A → A) {a a′ b b′ c′ c″} →
          a ≡ a′ → b ≡ b′ → c′ ≡ c″ → f a b c′ ≡ f a′ b′ c″
  cong₃ f refl refl refl = refl
subst-id le s-ref           = refl
subst-id le s-bool          = refl
subst-id le (s-arr sA sB)   = cong₂ TArr (subst-id le sA) (subst-id le sB)

-- A type is untouched by all three, at any index or cutoff.
ty-↑ : ∀ {A} d c → IsTy A → ↑ d c A ≡ A
ty-↑ d c isA = ↑-id d ≤-refl (ty-scoped c isA)

ty-↓ : ∀ {A} c → IsTy A → ↓ c A ≡ A
ty-↓ c isA = ↓-id ≤-refl (ty-scoped c isA)

ty-subst : ∀ {A s} j → IsTy A → [ j ↦ s ] A ≡ A
ty-subst j isA = subst-id ≤-refl (ty-scoped j isA)

------------------------------------------------------------------------
-- Pointwise equations for the variable cases
--
-- ↑, ↓ and [_↦_] each branch on a decision, so their variable clauses do not
-- reduce on an opaque index. These are the equations proofs actually use.

subst-var-hit : ∀ j s → [ j ↦ s ] (var j) ≡ s
subst-var-hit j s with j ≟ℕ j
... | yes _  = refl
... | no ¬p  = ⊥-elim (¬p refl)

subst-var-miss : ∀ {i j} s → ¬ (i ≡ j) → [ j ↦ s ] (var i) ≡ var i
subst-var-miss {i} {j} s ¬p with i ≟ℕ j
... | yes p = ⊥-elim (¬p p)
... | no  _ = refl

↓-var-lo : ∀ {i c} → i < c → ↓ c (var i) ≡ var i
↓-var-lo {i} {c} lt with c ≤? i
... | yes le = ⊥-elim (<-irrefl refl (≤-trans lt le))
... | no  _  = refl
