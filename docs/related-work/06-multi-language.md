# Multi-Language Programs, Interoperability, and Translation

**Status:** Index. Sourced 2026-08-03. Annotations are notes and observations, not substrate policy; see `00-index.md` §"How to read this".

## Why this theme

`../design/00-overview.md:17` states a hard invariant, **no cross-language references**, and `../design/05-translation.md` replaces the boundary with an ahead-of-time, cached, content-addressed translation. The literature's default is the opposite. Matthews & Findler put a boundary term in the language and pay for it at run time, and that model is canonical enough that reviewers will assume arbor uses boundaries unless it says otherwise. Citing it lets arbor state the contrast.

The theme also supplies a theorem shape. p6's `lc-to-stlc:check:v1[ty=…]` is a partial translator that can return `Translation_untypable(string)`, and it is not obvious what correctness means for such a thing. Patterson & Ahmed's "Next 700 Compiler Correctness Theorems" parameterizes compiler correctness over which source and target contexts are related, which makes a partial translator a legitimate theorem shape.

## Multi-language semantics

- **Jacob Matthews & Robert Bruce Findler (POPL 2007 / TOPLAS 2009) — "Operational Semantics for Multi-Language Programs."** POPL pp. 3–10, *SIGPLAN Notices* 42(1), DOI [10.1145/1190215.1190220](https://doi.org/10.1145/1190215.1190220); journal version *TOPLAS* 31(3), art. 12, pp. 1–44, DOI [10.1145/1498926.1498930](https://doi.org/10.1145/1498926.1498930). `[verified]` **[Priority read.]** The boundary/lump embedding, and the canonical model of the question arbor answers differently. They put a boundary term in the language and pay at run time. Arbor forbids cross-language references and replaces the boundary with a cached translation aspect. Needed to state that contrast precisely.
- **Daniel Patterson & Amal Ahmed (SNAPL 2017) — "Linking Types for Multi-Language Software: Have Your Cake and Eat It Too."** LIPIcs 71, pp. 12:1–12:15. DOI [10.4230/LIPIcs.SNAPL.2017.12](https://doi.org/10.4230/LIPIcs.SNAPL.2017.12); arXiv:1711.04559. `[verified]` The nearest existing proposal to "a definition carries information about what it can be linked or translated against," which is what a translation aspect implicitly is.
- **Daniel Patterson & Amal Ahmed (ICFP 2019) — "The Next 700 Compiler Correctness Theorems (Functional Pearl)."** *PACMPL* 3(ICFP), art. 85. DOI [10.1145/3341689](https://doi.org/10.1145/3341689). `[verified]` A functional pearl, not a survey. **[Priority read.]** Supplies the vocabulary for what an arbor translator's correctness theorem is, parameterized over which source/target contexts are related. In this framework `Translation_untypable` is a legitimate theorem shape, not a defect.
- **Daniel Patterson, Noble Mushtak, Andrew Wagner, Amal Ahmed (PLDI 2022) — "Semantic Soundness for Language Interoperability."** DOI [10.1145/3519939.3523703](https://doi.org/10.1145/3519939.3523703); arXiv:2202.13158. `[verified]` The most recent statement that type soundness must be stated across the boundary. Arbor's store invariant discharges that obligation by fiat, not by proof. "We forbid it" is a legitimate answer, but it should be presented as an answer to this question.
- **Daniel Patterson, Jamie Perconti, Christos Dimoulas, Amal Ahmed (PLDI 2017) — "FunTAL: Reasonably Mixing a Functional Language with Assembly."** pp. 495–509. `[verified]` **Fourth author is Christos Dimoulas, not Skorstengaard.**
- **James T. Perconti & Amal Ahmed (ESOP 2014) — "Verifying an Open Compiler Using Multi-Language Semantics."** LNCS 8410. DOI [10.1007/978-3-642-54833-8_8](https://doi.org/10.1007/978-3-642-54833-8_8). `[verified]` Also **Amal Ahmed (SNAPL 2015) — "Verified Compilers for a Multi-Language World,"** LIPIcs 32, pp. 15–31, DOI [10.4230/LIPIcs.SNAPL.2015.15](https://doi.org/10.4230/LIPIcs.SNAPL.2015.15) `[verified]`.

## Gradual typing and migration

The closest existing analogue to translating between adjacent languages in one family, which is what p5's arith→λ and p6's stlc↔lc translators do.

- **Jeremy G. Siek & Walid Taha (Scheme and Functional Programming Workshop 2006) — "Gradual Typing for Functional Languages."** pp. 81–92. `[verified]` **A workshop paper, not a refereed conference paper.**
- **Sam Tobin-Hochstadt & Matthias Felleisen (DLS 2006) — "Interlanguage Migration: From Scripts to Programs."** Cite as *Companion to OOPSLA '06*, pp. 964–974, DOI [10.1145/1176617.1176755](https://doi.org/10.1145/1176617.1176755). `[verified]` Also **"The Design and Implementation of Typed Scheme," POPL 2008,** pp. 395–406, DOI [10.1145/1328438.1328486](https://doi.org/10.1145/1328438.1328486) `[verified]`.
- **Max S. New & Amal Ahmed (ICFP 2018) — "Graduality from Embedding-Projection Pairs."** *PACMPL* 2(ICFP). DOI [10.1145/3236768](https://doi.org/10.1145/3236768). `[verified]`

## Lenses and schema migration — the Cambria line

`../design/05-translation.md:78` and `../design/00-overview.md:18` invoke "Cambria-style schema lenses" as the model for automated or semi-automated translation. This section identifies Cambria and the academic work under it.

- **Geoffrey Litt, Peter van Hardenberg, Orion Henry (October 2020) — "Project Cambria: Translate your data with lenses."** Ink & Switch. [inkandswitch.com/cambria](https://www.inkandswitch.com/cambria/). `[not peer-reviewed — lab essay; no DOI]` A TypeScript library for schema evolution in distributed systems. A lens is a composable bidirectional transformation between two schema versions. Lenses operate on JSON Patch representations of edits, not whole documents, which lets several schema versions collaborate at once. The system keeps a graph of schemas connected by lenses and migrates between distant versions by composing lenses along the shortest path.

  It sharpens three points in `05-translation.md`:
  1. The graph-plus-shortest-path structure is the transitive-translation policy that doc settles as "eager closure with hash-based lookup of cached sub-translations." Cambria is prior art for that shape.
  2. Cambria's lenses are bidirectional. `../design/decisions.md` records arbor's translators as one-directional with no roundtrip guarantee. This is a real divergence: arbor gives up what makes a lens a lens.
  3. Cambria operates on edits; arbor's translators operate on whole definitions. Since a definition is immutable and an edit produces a new hash, edit granularity may not transfer. This needs working through.

  Litt is a co-author of Peritext (`05-naming-versioning.md`), so Cambria sits in the local-first cluster, not the multi-language one. The translation framing is arbor's own reading.
- **Martin Hofmann, Benjamin C. Pierce, Daniel Wagner (POPL 2012) — "Edit Lenses."** pp. 495–508. DOI [10.1145/2103656.2103715](https://doi.org/10.1145/2103656.2103715). `[verified]` The academic work Cambria names as its direct basis: lenses whose domain is edits, not states. Use this peer-reviewed citation where the docs say "Cambria-style."
- **J. Nathan Foster, Michael B. Greenwald, Jonathan T. Moore, Benjamin C. Pierce, Alan Schmitt (TOPLAS 2007) — "Combinators for Bidirectional Tree Transformations: A Linguistic Approach to the View-Update Problem."** 29(3), art. 17; POPL 2005 original. DOI [10.1145/1232420.1232424](https://doi.org/10.1145/1232420.1232424). `[verified]` The foundational lens paper and the view-update framing. Pierce is an author on both lens papers and on TAPL, so the ladder arbor is climbing and the migration machinery it wants share an author.

## Polyglot runtimes

- **Thomas Würthinger, Andreas Wöß, Lukas Stadler, Gilles Duboscq, Doug Simon, Christian Wimmer (DLS 2012) — "Self-Optimizing AST Interpreters."** pp. 73–82. DOI [10.1145/2384577.2384587](https://doi.org/10.1145/2384577.2384587). `[verified]` Also **Würthinger et al. (Onward! 2013) — "One VM to Rule Them All,"** DOI [10.1145/2509578.2509581](https://doi.org/10.1145/2509578.2509581) `[verified]`.
- **Matthias Grimmer, Chris Seaton, Roland Schatz, Thomas Würthinger, Hanspeter Mössenböck (DLS 2015) — "High-Performance Cross-Language Interoperability in a Multi-Language Runtime."** pp. 78–90. DOI [10.1145/2816707.2816714](https://doi.org/10.1145/2816707.2816714). `[verified]` Journal version: **Grimmer, Schatz, Seaton, Würthinger, Luján, Mössenböck (TOPLAS 2018),** 40(2), art. 8, DOI [10.1145/3201898](https://doi.org/10.1145/3201898) `[verified]`. Truffle/GraalVM shares a runtime to get interop. Arbor shares a store and refuses runtime interop. Both are "one substrate, many languages," approached from opposite ends.

## FFI safety and translation validation

- **Michael Furr & Jeffrey S. Foster (PLDI 2005 / TOPLAS 2008) — "Checking Type Safety of Foreign Function Calls."** pp. 62–72; *TOPLAS* 30(4), DOI [10.1145/1377492.1377493](https://doi.org/10.1145/1377492.1377493). `[verified]` The best-verified FFI-safety item (OCaml→C and JNI type inference).
- **George C. Necula (PLDI 2000) — "Translation Validation for an Optimizing Compiler."** DOI [10.1145/349299.349314](https://doi.org/10.1145/349299.349314). `[verified]` The natural precedent for validating an arbor translator's output against its source instead of proving the translator. That fits `CLAUDE.md`'s disposable-prototype posture better than a verification effort would.
- **William J. Bowman (2025) — "Compilation as Multi-Language Semantics."** arXiv:[2509.19613](https://arxiv.org/abs/2509.19613). `[verified as preprint — self-described work-in-progress]` Argues one multi-language reduction system covers both AOT compilation (normalization) and JIT (evaluation). Structurally close to "translation is a cached derived aspect."

## Where it plugs in

- `../design/05-translation.md` — the whole theme. §"Cambria-style schema lenses" (line 78) is the one place that doc names prior art; the citation is now in §"Lenses and schema migration — the Cambria line" above.
- `../design/00-overview.md:17` and `../design/03-content-addressing.md` — the no-cross-language-references invariant answers Matthews & Findler and Patterson et al. (PLDI 2022), and should be presented that way.
- `docs/prototypes/p5-multi-language/00-scope.md`, `docs/prototypes/p6-stlc/00-scope.md` — the arith→λ Church-encoding translator and the partial `lc-to-stlc:check` translator. The Next 700 is the frame for what `Translation_untypable` means.
- `../design/01-language-model.md` — composition options A–D. Truffle/GraalVM is the shared-runtime option this survey adds; Racket `#lang` (already listed there) is the shared-kernel one.
- `../design/02-definitions-and-derived-data.md` — translation-as-derived-aspect, with procedure identity carrying the translator version.

## Gaps and negative findings

- ~~**Cambria has no citation.**~~ Resolved 2026-08-03: Litt, van Hardenberg & Henry, Ink & Switch, October 2020, with Hofmann–Pierce–Wagner edit lenses (POPL 2012) as its academic basis; see §"Lenses and schema migration — the Cambria line". The open question it leaves is that Cambria's lenses are bidirectional and edit-granular while arbor's translators are one-directional and definition-granular.
- **Unison offers no prior art here.** It is single-language. Say so explicitly instead of implying lineage. The multi-language dimension is arbor's own and one of its clearer differences from the system it most resembles.
- **No survey of FFI safety or capability-based interop was reached** before the search budget expired. Chisnall-style work on capability-safe FFI is a plausible gap. `[UNVERIFIED]`
- **Nothing found on translation-as-cached-derived-data.** Bowman's preprint is closest in spirit, and it concerns compilation phases, not a persistent store. The composition "translation output is content-addressed derived data on the source, keyed by translator identity" appears unclaimed. This is a negative finding from a budget-limited search, not a proof.
