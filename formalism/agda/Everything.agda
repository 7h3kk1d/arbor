{-# OPTIONS --safe #-}

------------------------------------------------------------------------
-- arbor-core, mechanized
--
-- The check target: `make check` runs `agda --safe Everything.agda`.
--
-- --safe here is a real claim. The library postulates nothing, uses no
-- TERMINATING pragmas, and leaves no unsolved metas; the paper's single axiom
-- (★) is a field of Arbor.Hash.HashModel, and Arbor.Core.Model exhibits an
-- inhabitant of it, so no proved statement is vacuous. Statements not yet
-- proved appear in Arbor.Core.Meta as named types with no inhabitant, which is
-- why a green build cannot be mistaken for a complete one — and why nothing
-- can quietly lean on an unproved lemma.
--
-- `make status` reports what is proved and what is open, per paper label.
-- `make labels` reports paper labels with no counterpart in Meta.agda.
------------------------------------------------------------------------

module Everything where

-- Shared infrastructure
import Arbor.Prelude
import Arbor.Hash
import Arbor.NodeSig

-- The signature-generic store layer (M4 groundwork): everything about the
-- store that does not depend on the language, so that arbor-stlc instantiates
-- rather than duplicates.
import Arbor.Sig
import Arbor.Generic.Syntax
import Arbor.Generic.Store
import Arbor.Generic.Preservation
import Arbor.Generic.Rewrite

-- The untyped λ instance
import Arbor.Core.Node
import Arbor.Core.Surface
import Arbor.Core.Syntax
import Arbor.Core.Params
import Arbor.Core.Store
import Arbor.Core.Eval
import Arbor.Core.Naming
import Arbor.Core.Cache
import Arbor.Core.History
import Arbor.Core.Migrate
import Arbor.Core.Config
import Arbor.Core.Preservation
import Arbor.Core.Rewrite

-- The paper, mirrored, and a consistency witness for its assumptions
import Arbor.Core.Meta
import Arbor.Core.Model

-- A machine-checked refutation of lem:closed-no-stuck as the paper states it
import Arbor.Core.Counterexamples

-- M4: the STLC rung (paper/arbor-stlc.tex), as a second instantiation of the
-- generic store layer rather than a second copy of it.
import Arbor.Stlc.Node
import Arbor.Stlc.Syntax
import Arbor.Stlc.Params
import Arbor.Stlc.Store
import Arbor.Stlc.Typing
