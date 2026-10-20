# Hazel as Substrate — Requirements, Solutions, and Sources

**Status:** Stub. A sourcing farm, not a finished design chapter.

## Purpose

Two jobs:

1. **Collect Hazel-specific requirements** the substrate must eventually satisfy, drawn from the Hazel research program and in particular the *computational commons* vision of the propl24 paper.
2. **Collect Hazel-specific solutions and artifacts** worth reusing or adapting: papers, modules, techniques, UX ideas. Other design docs (type checking, editor affordances, hole semantics) can draw on them.

Items here are inputs. When one becomes a design decision, the decision goes in `decisions.md`, the detail goes in the relevant `0N-*.md`, and a back-reference is left here.

## Known-relevant sources

- **propl24.** Outlines the *computational commons* vision and is the primary source for the substrate-level requirements this project aims at ([hazel.org](https://hazel.org/)). Full citation (added 2026-08-03): **Bandukwala, Blinn & Omar, "Toward a Live, Rich, Composable, and Collaborative Planetary Compute Engine," PROPL 2024** (Programming for the Planet, co-located with POPL), London, January 2024.
- **propl25.** A later commons paper: **Omar, Coblenz & Madhavapeddy, "A FAIR Case for a Live Computational Commons," PROPL 2025**, pp. 8–13, DOI [10.1145/3759536.3763802](https://doi.org/10.1145/3759536.3763802). It proposes *Fairground* and frames the commons around the FAIR principles (findable, accessible, interoperable, reusable), the most directly usable rubric for the requirements checklist below. It reaches the commons through DOIs, ORCIDs, and a version-control platform, with no content-addressing component anywhere in its 53 references. That gap is what this substrate is for.
- **Hazel publication list.** At [hazel.org](https://hazel.org/). Drawn on per topic as design questions arise, not imported up front as a bibliography.
- **Back-reference (2026-08-03).** Per the contract above, citations for the research lines below (all fourteen Hazel papers, with authors, venues, and DOIs) now live in `../related-work/02-structure-editors.md`, and the label, module, and existential material the type-abstraction line uses is in `../related-work/03-modules-abstraction.md`. That directory is backward-looking (what arbor's existing claims sit next to) and does not change the per-topic policy above about when a paper is read or a technique adopted. Two of its findings bear on lines below, both filed in `open-questions.md` §"Related work": the POPL 2024 totality result versus arbor's ingest-time rejection of ill-typed terms (lines #5, #9), and Grove's minted vertex UIDs versus content hashes (line #10).

## Research lines worth pulling from

Hazel research lines likely to contribute ideas or machinery. Each entry names the contribution, where it plugs in, and a phase hint (day-one, later, speculative). Specific papers are pulled in as design decisions need them.

### 1. Typed holes as first-class terms

Hazel's foundational move: a hole is a well-typed term parameterized by a context type, and type checking, editing, and evaluation all handle holes without special cases. A program with holes is a well-formed program in a hole-aware language, not a partial artifact.

- **Contributes to.** The hole-aware language machinery of requirement #1 below: canonical form for holes, their interaction with type systems, how type checking treats them, and how they hash.
- **Plugs into.** `03-content-addressing.md` (canonicalizing holes: unique hole identities or subsumption-style matching?), per-language modules in `06-architecture.md`, and any hole-aware language we add.
- **Phase.** Later, with the first hole-aware language.

### 2. Live programming with holes — evaluation of incomplete programs

Hazel evaluates programs containing holes, producing *indeterminate* results that show what evaluation will do once the holes are filled. This gives live feedback on unfinished work.

- **Contributes to.** Evaluation for hole-aware languages: `evaluate` runs on any program, complete or not, and returns a useful result.
- **Plugs into.** The evaluation aspect for hole-aware languages. It also reshapes the pure tier in `06-architecture.md`: when drafts are stored programs, live feedback runs through stateful operations instead of interface-side pure calls.
- **Phase.** Later, with a hole-aware language that has evaluation.

### 3. Bidirectional typing for hole-aware languages

Hazel's type checker is bidirectional: each subterm is either *synthesized* (type inferred from structure) or *analyzed* (checked against an expected type). Holes fit because analysis mode supplies a hole's expected type even when its contents are unknown.

- **Contributes to.** Type-checker design for any hole-aware language. Tends to localize errors better than unification-based checkers.
- **Plugs into.** Per-language modules in `06-architecture.md`; type-check aspects in `02-definitions-and-derived-data.md`.
- **Phase.** Later.

### 4. Structure-editing calculi

Hazel formalizes edit actions (insert, delete, move cursor) as typed operations on an AST-with-cursor that preserve well-formedness by construction. Editing becomes a small, proved calculus.

- **Contributes to.** Interface-side editing infrastructure for a structured editor. If edits become storable artifacts, they are also derived data ("this edit produced this definition") and inherit the content-addressed machinery.
- **Plugs into.** The Interface layer (`06-architecture.md`). Substrate implications (edits as derived data) are speculative.
- **See also.** Line #10 (Grove), which extends the idea to concurrent editing via a CmRDT.
- **Phase.** Later / interface.

### 5. Total, incremental type checking

Hazel's type checkers are total, returning a result for every program, and designed for incrementality, which suits live feedback where every keystroke needs a fresh verdict.

- **Contributes to.** Type-check procedures for hole-aware languages. Relevant to live-feedback latency, not correctness.
- **Plugs into.** Per-language type-check procedures and interface-tier live feedback.
- **Phase.** Later.

### 6. Projectional / tile-based editing (tylr and related)

Hazel's ongoing work on non-traditional structure editing: tiles, movable structure, text-like UX over AST-level primitives, editing ASTs directly without fragile intermediate concrete syntax.

- **Contributes to.** Interface directions for a structured editor.
- **Plugs into.** The Interface layer only; no substrate implications.
- **Phase.** Later / interface.

### 7. Computational commons (propl24)

The vision paper: a shared, content-addressed, collaborative ecosystem for programs, and this project's north star.

- **Contributes to.** Requirements for the whole substrate.
- **Plugs into.** Every design doc, particularly `00-overview.md` and the roadmap.
- **Phase.** Ongoing. Extract concrete requirements per topic.

### 8. AI-assisted hole filling (recent)

Recent Hazel-adjacent work on LLM-suggested hole completions, treating a filling as an action the environment proposes and the user accepts or rejects.

- **Contributes to.** Interface affordances. May motivate treating a filling as a discrete, recordable action, either an edit-as-derived-data or a "suggested filling" aspect the user can promote.
- **Plugs into.** The Interface layer; tangentially, translation-like aspect work (suggestions as candidate derived data awaiting approval).
- **Phase.** Speculative.

### 9. Semantics of incomplete / erroneous programs

The broader program of giving meaningful semantics to programs that would traditionally be rejected: error localization, gradual typing, fill-and-resume evaluation (filling a hole lets paused evaluation continue), and related ideas.

- **Contributes to.** Interfaces that show what a program would do even when parts are missing or ill-typed.
- **Plugs into.** Type-check and evaluation aspects; interface presentation of partial results.
- **Phase.** Later.

### 10. Grove: Collaborative structure editing as a CmRDT (POPL 2025)

Grove handles collaborative editing and merge at the AST level, without patch synthesis (diff) or three-way merge.

**The problem, by example.** Start with:

```
f = λx.
  let y = x + 1 in
  y * 2
```

Alice wraps the body in an outer `let z = "start" in …`. Bob concurrently changes `y * 2` to `y * 3`. A clean merge has both Alice's wrap and Bob's change. Call this the **relocation-modification problem**: one edit relocates a subtree inside something new, and another modifies inside the relocated region.

Tree-diff merging needs a heuristic to recognize Bob's modified subtree as the place Alice moved, and heuristics fail unevenly on intertwined edits. Unison's naming layer and tree diffs handle clean cases, and handle update propagation to callers (a related but different problem) well, but degrade on relocation-modification.

**Grove's move: identity built into the data structure.** The edit state is a labeled directed multigraph in which every vertex and edge gets a UID at creation that never changes. Edits are edge insertions and permanent edge deletions (a 2P-set CRDT). All edits commute, so merging applies each user's edits in any order. In the example, Alice's and Bob's edits touch different edges and commute by construction. Conflicts commutativity cannot resolve (multi-parent vertices, cycles, multiple values at one position) become first-class syntactic constructs inside a tree decomposition, resolvable by ordinary editing.

**What we want from it.** Grove is not adopted now. The current phase is single-user, commit-based editing, where Unison-style approaches suffice. Real-time collaborative editing, or branching with intertwined concurrent edits, eventually runs into relocation-modification, and Grove is the most developed answer available.

**Open architectural question.** Where Grove-like machinery would live is unsettled. It could be a new layer (a future *Edit State* between Language and Interface), or a capability certain languages or aspects provide. Not decided.

**Plugs into.**
- `04-naming-layer.md`: when branching and merging arrive, Grove is the first place to look.
- `03-content-addressing.md`: collaborative editing forces a distinction between UID identity at the edit layer and content-hash identity at the committed layer. Real, but not today's problem.
- Hole-aware language modules: Grove extends the hole principle to conflicts. Both are first-class syntactic constructs for something that is not normal AST but should still type-check.

**Phase.** Later, explicitly targeted as the starting point for branching and merging.

## Requirements checklist (populated as claims land)

Concrete requirements the substrate must eventually meet, distilled from the sources above.

- **Hole-aware languages as first-class citizens.** Some languages will encode incompleteness (typed holes) in their AST, and the substrate must store, hash, translate, and reason about programs with holes in those languages natively. Each such language declares its own AST and canonicalizer; no generic partial-program machinery is needed. Interfaces over hole-aware languages may store live editing state directly instead of keeping interface-side draft buffers. See `06-architecture.md` (Interface layer) and `03-content-addressing.md` (non-goals).
- _TBD: further requirements extracted from propl24_
- _TBD: requirements extracted from live-programming / semantics-of-incomplete-programs line_

## Candidate reuses

Specific Hazel solutions or modules that may transplant here. Each item names the source, the technique, and what it plugs into.

- **Hazel core calculus implementation.** Host-language modules for Hazel's AST, bidirectional type checking, and hole-aware evaluation. Likely transplantable or adaptable as one of our languages, or a family of them, once we add a hole-aware language. Feeds lines #1, #2, #3.
- **Grove Workbench.** The Grove paper's reference OCaml implementation. Language-parametric: it generates data structures and algorithms from a syntax-tree specification. A candidate for direct reuse or structural inspiration for collaborative editing and merging. Feeds line #10.
- _Others TBD after a pass over the Hazel codebase; to be recorded in `08-tech-stack.md`._

## Open questions from the Hazel angle

Tracked in `open-questions.md` under "Hazel substrate." Major ones:

- Which Hazel capabilities are day-one inputs and which are later?
- Can the propl24 commons requirements be expressed as a concrete checklist against this substrate's API?
