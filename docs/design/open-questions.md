# Open Questions

Running list. Items graduate to `decisions.md` when resolved, or into specific design docs as they're fleshed out. Add, don't silently remove; if a question is closed, link the decision or doc that resolved it.

---

## Strategic

- **Composition model for languages.** Fixed ladder, feature lattice, shared core IR, or hybrid. Deliberately deferred; see `01-language-model.md`.
- ~~**Project name.**~~ Resolved 2026-04-23: **arbor** (see `decisions.md`). Working directory remains `lc-content-addressed`.
- **Research framing.** What's the thesis this becomes a prospectus *for*? Translation as first-class, content-addressed semantics, something else?

## Tech stack

- **Hazel reuse vs. fresh.** Reuse OCaml + dune + existing Hazel modules, or start clean? Tradeoffs in `08-tech-stack.md` once drafted.
- **Storage layer.** What stores the content-addressed definition store? Flat filesystem, SQLite, custom?

## Associated data

- **Vocabulary for "aspect."** Chosen provisionally to avoid TAPL-kind collision. Alternatives if friction appears: *facet*, *annotation*, *attribute*, *channel*.
- **Conflict handling.** When an asserted entry and a derived entry disagree on the same logical property (annotation says `Int→Int`; inference says `Bool→Bool`), the substrate records both. What's the default surface behavior?
- **Where aspect descriptors live.** Per-language module, global registry, or both? Big practical consequence for the tech stack.
- **Cross-aspect dependencies.** Evaluation depends on types; translation may depend on elaboration. Formalize as an aspect-dependency graph, or handle ad hoc in each procedure?
- **History of asserted entries.** Names and descriptions change. Do we keep edit history, and at what granularity? Ties into eventual branching.
- **Graduating from tag+version to content-addressed procedures.** What concretely triggers that migration? Probably when we have a stable meta-language candidate and enough derived-entry cache volume to justify precise invalidation.

## Translation

Addressed in `05-translation.md` / `decisions.md`:
- ~~Transitive translation policy.~~ Eager closure with hash-based lookup of cached sub-translations.
- ~~Multiple translators per pair.~~ Translators are potential actions; interfaces present choices.
- ~~Bidirectionality.~~ Translators are one-directional; roundtripping not guaranteed.
- ~~Staleness / garbage collection.~~ Deferred; orphan entries accumulate for now.
- ~~Subset shortcut (structural embedding generator).~~ Future work; hand-writing identity-lift translators for now.
- ~~Certifying correctness.~~ Trusted during bootstrap; verification infrastructure is future work.

No active open questions. New ones will accumulate as implementation surfaces them.

## Content addressing

Addressed in `03-content-addressing.md`:
- ~~Canonicalization.~~ Syntactic by default, α-equivalence at minimum, per-language declarations allowed; coarser (semantic) equivalences are per-language opt-ins left open.
- ~~Primitive / builtin identity.~~ Manually-versioned tags, same model as procedure identity.

Still open:
- **Mutual recursion canonicalization.** When a language introduces mutually recursive definitions, we'll need a canonical ordering for the group. Unison's approach is a starting point. Not urgent.
- **Holes and incomplete programs.** If the substrate eventually hashes incomplete programs (Hazel-style editing), how do holes participate in the canonical form? Unique hole identities, subsumption-style matching, or something else?
- **Cross-version primitive aliasing.** If `int:add:v1` and `int:add:v2` differ only cosmetically, callers of v1 are orphaned. Any aliasing story, or just accepted cost?
- **Hash algorithm.** Not locked in; bootstrap accepts full rebuilds. Choice will come when we need one.

## Naming layer

- **Scope of names.** Per-user, per-"workspace," global? Unison uses namespaces; we'll want to punt on structure but should not paint ourselves into a corner.
- **Name → hash resolution timing.** Strictly at edit time, or also at some kind of "load" step for interfaces?
- **Unifying namespace and aspect-store lifecycles.** When branching arrives, does a shared "container" (or whatever name emerges) make sense, or do we keep them parallel? See the cross-reference sections in `02` and `04`.
- **Name structure: hierarchical paths.** Editing-layer convention, substrate-aware paths, or substrate-fixed format? Bound up with when namespace branching enters scope. Sharpened in `04-naming-layer.md` under *Threads under exploration*.
- **Name structure: tags.** Many-to-many name-shaped attachments. Tentative reading is "asserted aspect with relaxed key cardinality"; open whether anything resists that. Sharpened in `04-naming-layer.md` under *Threads under exploration*.
- **Name structure: leaf-names independent of path.** Whether the leaf segment is its own identity, or whether leaf-linking is an editing-layer rename UX over a flat namespace. Sharpened in `04-naming-layer.md` under *Threads under exploration*.

## Interfaces

- **First editor-like interface.** Phase 1 (`09-roadmap.md`) picks among batch runner, REPL, or stateful CLI. Once we're past minimal interfaces, what's the first editor-shaped one — scratch-buffer-style, structured editor, notebook, something else? Open.
- **What does the substrate API look like?** TBD; concrete signatures in `06-architecture.md` once the tech stack is picked.
- **Draft-state transition for hole-aware languages.** When we add hole-aware languages, interfaces offering drafts in those languages may store programs-with-holes directly rather than managing interface-side draft state. How do those interfaces coexist with interfaces still using the draft-state-is-interface-state model for non-hole-aware languages? Do we need a unified API shape that covers both?

## Roadmap

Feeds from `09-roadmap.md`.

- **Phase 1 interface choice.** Batch runner, REPL, or stateful CLI.
- **Phase 2 rendering behavior on referenced-but-unnamed hashes.** Fall back to hash, or inline the referenced subterm?
- **Phase 3: keep or discard Phase 1/2's arithmetic code?** Spirit of disposable prototypes says discard; worth revisiting when we get there.
- **Phase 4 reverse translator.** Is LC → arithmetic interesting, or do we only do the forward direction?
- **Throwaway discipline in practice.** Are we actually willing to discard prototypes, or will we slip into evolving one?
- **When do learnings migrate back to `docs/design/`?** Worth making this habit explicit once prototypes exist.

## Hazel substrate

- **Which Hazel capabilities are non-negotiable inputs?** Typed holes, structured editing, incremental type-checking — which of these do we need from day one, and which can come later? To be sourced from `07-hazel-substrate.md`.
- **Propl24 "computational commons" requirements.** Need to distill these into a concrete requirements checklist the substrate must eventually meet.
