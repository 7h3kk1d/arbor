# Hazel as Substrate — Requirements, Solutions, and Sources

**Status:** Stub. This document is a sourcing farm, not a finished design chapter.

## Purpose

Two jobs:

1. **Collect Hazel-specific requirements** that the substrate must eventually satisfy — drawn from the Hazel research program, in particular the *computational commons* vision in the propl24 paper.
2. **Collect Hazel-specific solutions and artifacts** worth reusing or adapting — papers, modules, techniques, UX ideas. Items here are candidates for consumption by other design docs (type checking, editor affordances, hole semantics, etc.).

Items in this doc are inputs. When one of them turns into a concrete design decision, the decision lands in `decisions.md` and the detail lands in the relevant `0N-*.md`, with a back-reference here.

## Known-relevant sources

- **propl24.** Outlines the *computational commons* vision. Primary source for the substrate-level requirements this project is trying to satisfy. ([hazel.org](https://hazel.org/))
- **Hazel publication list.** Broader source list at [hazel.org](https://hazel.org/). To be drawn on per-topic as design questions arise rather than imported as a bibliography up front.

## Research lines worth pulling from

Candidate Hazel research lines likely to contribute useful ideas or machinery to this substrate. Each entry names the contribution, where it plugs in, and a phase hint (day-one, later, speculative). Specific papers should be pulled in as design decisions require them, not pre-fetched.

### 1. Typed holes as first-class terms

Hazel's foundational move: treat a hole as a well-typed term parameterized by a context type. All operations (type-checking, editing, evaluation) extend to handle holes without special cases. A program with holes is a well-formed program in a hole-aware language, not a partial artifact.

- **Contributes to.** The hole-aware language machinery required by requirement #1 below. Defines canonical form for holes, how they interact with type systems, how type-checking treats them, how they hash.
- **Plugs into.** `03-content-addressing.md` (canonicalization of holes — unique hole identities or subsumption-style matching?), per-language modules in `06-architecture.md`, any hole-aware language we add.
- **Phase.** Later — when we add our first hole-aware language.

### 2. Live programming with holes — evaluation of incomplete programs

Hazel evaluates programs containing holes, producing *indeterminate* results that represent what evaluation would do once holes are filled. This lets you get live feedback on works in progress without first completing them.

- **Contributes to.** The evaluation story for hole-aware languages. Lets us run `evaluate` on any program, complete or not, and get a useful result.
- **Plugs into.** Evaluation aspect for hole-aware languages. Also reshapes the pure-tier API in `06-architecture.md`: when drafts are stored programs in hole-aware languages, live feedback runs via stateful operations rather than interface-side pure calls.
- **Phase.** Later — when we add a hole-aware language with evaluation.

### 3. Bidirectional typing for hole-aware languages

Hazel's type checker is bidirectional: every subterm is either *synthesized* (type inferred from structure) or *analyzed* (checked against an expected type). Holes fit cleanly because the analysis mode tells you what a hole's type should be even when you don't know what the hole will contain.

- **Contributes to.** Type-checker design for any hole-aware language we add. Also tends to produce better error localization than unification-based checkers.
- **Plugs into.** Per-language modules in `06-architecture.md`; type-check aspects in `02-definitions-and-derived-data.md`.
- **Phase.** Later.

### 4. Structure-editing calculi

Hazel formalizes edit actions themselves (insert, delete, move cursor) as typed operations on an AST-with-cursor that preserve well-formedness by construction. Editing is a small, proved calculus rather than a pile of string operations.

- **Contributes to.** Interface-side editing infrastructure if we build a structured editor. Potentially more: if edits become storable artifacts, they become derived data — "this edit produced this new definition" — and inherit the content-addressed machinery.
- **Plugs into.** Interface layer (`06-architecture.md`). Substrate implications are speculative (edits-as-derived-data).
- **See also.** Research line #10 (Grove) — a collaborative extension of the structure-editing-calculus idea, applied to concurrent editing via a CmRDT.
- **Phase.** Later / interface.

### 5. Total, incremental type checking

Hazel's type-checkers are total (they return a result for *every* program) and designed with incrementality in mind. Useful for live feedback UX where every keystroke needs a fresh type verdict.

- **Contributes to.** Type-check procedure design for hole-aware languages. Relevant for live feedback latency, not correctness.
- **Plugs into.** Per-language type-check procedures; interaction with interface-tier live feedback.
- **Phase.** Later.

### 6. Projectional / tile-based editing (tylr and related)

Hazel's ongoing line of work on non-traditional structure editing — tiles, movable structure, text-like UX over AST-level primitives. Explores what editing feels like when you edit ASTs directly without fragile intermediate concrete syntax.

- **Contributes to.** Interface-layer directions if we build a structured editor.
- **Plugs into.** Interface layer; no substrate implications.
- **Phase.** Later / interface; UX-direction, not substrate.

### 7. Computational commons (propl24)

The vision paper. A shared, content-addressed, collaborative ecosystem for programs. The north star for this project.

- **Contributes to.** Requirements for the whole substrate.
- **Plugs into.** Every design doc, particularly `00-overview.md` and the eventual roadmap.
- **Phase.** Ongoing; extract concrete requirements per-topic rather than en masse.

### 8. AI-assisted hole filling (recent)

Recent Hazel-adjacent work exploring LLM-suggested completions for holes. Treats filling as an action the environment proposes and the user accepts or rejects.

- **Contributes to.** Interface-layer affordances. May motivate treating filling as a discrete, recordable action (an edit-as-derived-data, or a "suggested filling" aspect that the user can promote).
- **Plugs into.** Interface layer. Tangentially: translation-like aspect work (suggestions as candidate derived data awaiting user approval).
- **Phase.** Speculative.

### 9. Semantics of incomplete / erroneous programs

The broader research program around giving meaningful semantics to programs that would traditionally be rejected. Error localization, gradual typing, "fill-and-resume" evaluation (filling a hole lets evaluation continue from where it paused), and related ideas.

- **Contributes to.** The "trust but find issues" style of interaction — interfaces that show what a program would do even when parts are missing or ill-typed.
- **Plugs into.** Type-check and evaluation aspects; interface presentation of partial results.
- **Phase.** Later.

### 10. Grove: Collaborative structure editing as a CmRDT (POPL 2025)

Grove attacks the collaborative-editing and merge problem at the AST level, without patch-synthesis (diff) or three-way merge algorithms.

**The problem Grove solves — by example.** Start with:

```
f = λx.
  let y = x + 1 in
  y * 2
```

Alice wraps the body in an outer `let z = "start" in …`. Bob, concurrently, changes `y * 2` to `y * 3`. A clean merge should produce Alice's wrap *and* Bob's inner change. Call this the **relocation-modification problem**: one edit restructures (relocates a subtree inside something new); another modifies inside the relocated region.

Tree-diff-based merging needs a heuristic to recognize Bob's modified subtree as "the same place" Alice moved it to — and heuristics fail unevenly on intertwined edits. Unison's naming layer + tree diffs handle clean cases well (and handle *update propagation to callers* beautifully — a related but different problem), but relocation-modification is where they degrade.

**Grove's move: identity built into the data structure.** The edit state is a labeled directed multigraph; every vertex and edge has a UID assigned at creation and never changed. Edits are edge insertions and permanent edge deletions (a 2P-set CRDT). All edits commute, so merging is trivial — apply each user's edits in any order. In the example above, Alice's edits and Bob's edits touch different edges and commute by construction. No heuristic required. Conflicts that genuinely can't be resolved by commutativity (multi-parent vertices, cycles, multiple values at one position) become **first-class syntactic constructs** inside a tree decomposition, resolvable by ordinary editing.

**What we want from this.** We're not adopting Grove now. Current-phase scope is single-user, commit-based editing, where Unison-style approaches are sufficient. But the long-term vision — especially any form of real-time collaborative editing, or branching with intertwined concurrent edits — eventually runs into the relocation-modification problem, and Grove is the most developed answer available.

**Open architectural question.** Where Grove-like machinery would live isn't settled. It could be a distinct layer in our architecture (a future *Edit State* between Language and Interface), or something more orthogonal — a capability certain languages or aspects provide, rather than a new core layer. Deliberately not deciding now.

**Plugs into.**
- `04-naming-layer.md` — when branching and merging arrive, Grove is where we look first.
- `03-content-addressing.md` — collaborative editing forces a distinction between UID identity (at the edit layer) and content-hash identity (at the committed layer). Not a problem to solve today, but real.
- Hole-aware language modules — Grove extends the hole principle to conflicts. Both are first-class syntactic constructs representing something that "isn't normal AST" but should still type-check.

**Phase.** Later, explicitly targeted. When we take on branching and merging, this is the starting point.

## Requirements checklist (populated as claims land)

Concrete requirements the substrate must eventually meet, distilled from the sources above.

- **Hole-aware languages as first-class citizens.** Some languages in this system will encode incompleteness (typed holes) as part of their AST. The substrate must be able to store, hash, translate, and reason about programs-with-holes in those languages natively. The draft/committed distinction dissolves for hole-aware languages: an incomplete program is a well-formed program. The substrate needs no generic "partial program" machinery; each hole-aware language declares its own AST and canonicalizer. Interfaces building on hole-aware languages may therefore store live editing state directly, rather than managing interface-side draft buffers. See `06-architecture.md` (Interface layer) and `03-content-addressing.md` (non-goals) for how this reshapes the draft-state story.
- _TBD: further requirements extracted from propl24_
- _TBD: requirements extracted from live-programming / semantics-of-incomplete-programs line_

## Candidate reuses

Specific Hazel solutions or modules that may transplant into this substrate. Items should name the source, the technique, and the doc or open question they plug into.

- **Hazel core calculus implementation.** Host-language modules defining Hazel's AST, bidirectional type-checking, and hole-aware evaluation. Likely transplantable or adaptable as one of our languages (or a family of them), once we're ready to add a hole-aware language. Feeds research lines #1, #2, #3.
- **Grove Workbench.** Reference OCaml implementation from the Grove paper. Language-parametric: generates data structures and algorithms given a syntax-tree specification. Candidate for direct reuse or structural inspiration when we eventually take on collaborative editing and merging. Feeds research line #10.
- _Others TBD as we dig further into the Hazel codebase; captured concretely in `08-tech-stack.md` once we do a codebase pass._

## Open questions from the Hazel angle

Tracked in `open-questions.md` under "Hazel substrate." Major ones:

- Which Hazel capabilities are day-one inputs vs. later inputs?
- Can the propl24 commons requirements be expressed as a concrete checklist against this substrate's API?
