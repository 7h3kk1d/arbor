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
open import Data.Nat.Base using (ℕ; zero; suc; _<_)
open import Data.Vec.Base using (Vec) renaming ([] to ⟦⟧; _∷_ to _◂_)
open import Relation.Binary.PropositionalEquality using (_≡_; refl)

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
