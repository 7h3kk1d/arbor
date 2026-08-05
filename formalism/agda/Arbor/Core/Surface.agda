{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- Surface terms (arbor-core def:surface)
--
-- Split out from Arbor.Core.Syntax on purpose. Surface terms mention Name;
-- core terms cannot. That the core syntax module takes only `Hash` as a
-- parameter, and this one takes `Name`, is the mechanized content of
-- prop:namefree ("names never enter the store"): there is no core
-- constructor for a name to inhabit, so the property is discharged by the
-- datatype declarations rather than by a proof.
--
-- One identifier leaf, not two (decisions.md 2026-07-30): p4's
-- surface_ast.re has a single Var(string), and a parser cannot know whether
-- an occurrence is λ-bound or free. Resolution decides, innermost binder
-- first (def:elab).
------------------------------------------------------------------------

module Arbor.Core.Surface (Name : Set) where

data Surface : Set where
  svar : Name → Surface
  slam : Name → Surface → Surface
  sapp : Surface → Surface → Surface
