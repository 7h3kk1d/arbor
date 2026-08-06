# Related Work — Index

**Status:** Index. Sourced 2026-08-03 in two literature sweeps. Annotations are notes and observations, not substrate policy — nothing here is a commitment unless `../design/decisions.md` says so.

## Purpose

An index of the literature arbor is already positioned against, with authors, venues, years, and DOIs. It exists because the design docs invoke a lot of work by surname with no anchor: `../design/12-type-abstraction.md:59` and all of p17 lean on "Harper–Lillibridge and Leroy"; `../design/open-questions.md:98` and both p14 and p15 lean on "Montagu–Rémy"; `../design/00-overview.md:10` names `propl24` as the north star. None of those has a citation anywhere in the repo. Before this sweep, `docs/` contained **two URLs and zero DOIs**, and `formalism/paper/references.bib` held five entries of which exactly one was ever cited.

The point is not completeness. It is that a claim like "no silent breakage" or "α-equivalence by canonicalization" should be stated *against* the existing vocabulary rather than in a private one, and that requires knowing what the existing vocabulary is.

## Relation to the sourcing policy in `07-hazel-substrate.md`

`../design/07-hazel-substrate.md:17` sets a standing policy: sources are "to be drawn on per-topic as design questions arise rather than imported as a bibliography up front," restated at `:21` as "specific papers should be pulled in as design decisions require them, not pre-fetched." **That policy still holds, and this directory is not an exception to it.**

The distinction: `07` is a *forward-looking* farm for Hazel machinery arbor might consume, and its discipline is about not elaborating infrastructure for capabilities arbor has not reached. This directory is *backward-looking* — an index of work arbor's existing claims already sit next to, assembled once because the docs were already invoking these authors without anchors. It changes nothing about when a paper gets read or a technique gets adopted. Items graduate the same way `07`'s do: into `../design/decisions.md` when they harden, into a specific design doc when the detail lands.

Where the two overlap — the Hazel line — `02-structure-editors.md` supplies the citations `07` deliberately omits, and `07` remains the place that says what arbor wants from each and when.

## How to read this

Every item carries a **verification tag**, carried through from the sweeps unchanged:

- `[verified]` — authors, title, venue, and year confirmed against a publisher record, DBLP, Crossref, or the authors' own publication list.
- `[verified — secondary]` — confirmed only via indexes or citing works.
- `[UNVERIFIED — check before citing]` — could not confirm. **Do not cite these without checking.** There are roughly two dozen.
- `[not peer-reviewed — blog / wiki / talk / spec / artifact / software]` — real and often important, but it cannot carry a claim. Say what it is.
- `[no publication exists]` — searched for, does not exist. Unison and Kythe are both in this category.

Items worth reading rather than merely citing are marked **[Priority read]** inline; the ranked list is below.

**Two honest caveats about the sweeps themselves.** Both exhausted their web-search budget partway and finished via Crossref, DBLP, and author-page fetches — which is why some page ranges are missing while authors and venues are solid. And the negative findings below are *failures to find*, from a budget-limited search. They are strong signals, not proofs of absence.

## Theme map

- `01-content-addressing.md` — why content-address at all rather than mint everything; Merkle DAGs, IPFS/IPLD, Nix and Guix; the Software Heritage identifier papers read in full (intrinsic identifiers, DIO vs. IDO, the six mechanisms, the canonical-representation precondition); hash-consing modulo an equivalence; hashing modulo α-equivalence; minting prior art and the coincidental-convergence hazard; and the finding that Unison has no publication.
- `02-structure-editors.md` — the full Hazel line with DOIs, the earlier structure-editor tradition, MPS and the language workbenches, and the recent counterargument to storing only structure. The citation layer under `../design/07-hazel-substrate.md`.
- `03-modules-abstraction.md` — the p12–p17 line: existentials, translucent sums, Leroy, F-ing Modules, F-zip and open existentials, generativity and type stamps, records and labels. Contains **the Sewell cluster**, the survey's most consequential find.
- `04-incrementality.md` — build systems, self-adjusting and demand-driven computation, Nominal Adapton and the names/hashes parallel, incremental type checking, and the granularity gap arbor cannot reach.
- `05-naming-versioning.md` — semver as a formal object, the empirical breaking-change literature, structured diff and merge, CRDTs and replicated trees, de Bruijn and nominal sets.
- `06-multi-language.md` — boundary semantics, linking types, compiler-correctness theorem shapes for partial translators, polyglot runtimes, translation validation.
- `07-commons.md` — the two PROPL papers, the hypertext and personal-computing lineage, notebook computing and its empirical critiques, and what the published commons vision does *not* contain.
- `08-agentic-vc.md` — agent platforms, the merge-conflict-ML line and its ceilings, the surveys, and the three 2026 preprints that force arbor to restate its claims.
- `09-scid.md` — what SCID actually is, the real ancestry (Linton, Masterscope, the image tradition, ENVY), the code-as-facts query line, and **substrates** as the current named programme.
- `10-malleable-tooling.md` *(sourced 2026-08-06)* — first coverage of the interface layer: moldable development and Glamorous Toolkit (a methodology with papers, an artifact without one), the tailorable-systems lineage from EMACS and Buttons through end-user software engineering, and the current malleable-software wave; the moldable-views-over-an-immutable-store observation.
- `11-benchmarks-and-evaluation.md` *(sourced 2026-08-06)* — the four genres of PL benchmark and challenge (workbench task assignments, metatheory challenge problems, performance suites, environment-evaluation frameworks), the lesson that challenges outlive task-suite websites, and the finding that no benchmark exists for program stores or term-manipulation toolkits.

## Priority reads

Ranked by how much reading each would change what arbor writes down. Fifteen items; several are pairs meant to be read together.

1. **Leifer, Peskine, Sewell, Wansbrough (ICFP 2003) — "Global Abstraction-Safe Marshalling with Hash Types."** Abstract-type names generated *by hashing*, so type equality suffices to protect abstraction across a distributed system. This is p12's `Opaque{mint, witness}` with the soundness argument already written. Shortest and most on-point of the cluster; start here. → `03-modules-abstraction.md`
2. **Sewell (POPL 2001) — "Modules, Abstract Types, and Distributed Versioning."** Abstract-type identity *across versions and independently-compiled programs*. Prior art for no-silent-breakage that arbor's docs do not cite. → `03-modules-abstraction.md`
3. **Rossberg, Russo, Dreyer — "F-ing Modules" (JFP 2014 version).** The proof that plain `∀`, `∃`, and records suffice for full ML modules. arbor's p13–p17 architecture stated as a theorem. If arbor cites one modules paper, this. → `03-modules-abstraction.md`
4. **Montagu & Rémy (POPL 2009) — "Modeling Abstract Types in Modules with Open Existential Types."** F-zip splits pack/unpack so a witness is introduced and closed at different points. p17's `Open_local` (scoped, no mint) and top-level `Open` (generative) are the closed and open forms of one operation; this names the avoidance condition. → `03-modules-abstraction.md`
5. **Adams, Griffis, Porter, Satish, Zhao, Omar (POPL 2025) — "Grove."** The nearest formal neighbour: same lab, same problem, opposite identity substrate (unique vertex IDs vs. content hashes). Forces arbor to say what hashes buy over UIDs, and to concede that a hash cannot express a move. → `02-structure-editors.md`
6. **Hammer et al. (OOPSLA 2015) — "Incremental Computation with Names,"** read with **"Refinement Types for Precisely Named Cache Locations."** Names are the crux of cache-keyed incrementality, and name *precision* is hard enough to need a type system. arbor's hashes are precise names by construction — and **minted mark ≈ nominal name, content hash ≈ structural key** is the sharpest available statement of arbor's identity design. → `04-incrementality.md`
7. **Porter, Kirisame, Wei, Panchekha, Omar (OOPSLA 2025) — "Incremental Bidirectional Typing via Order Maintenance."** Occupies the exact granularity gap in arbor's caching story. Read to know what eval-stability does *not* cover: the keystroke-to-keystroke inner loop, where every edit is a fresh hash. → `04-incrementality.md`
8. **Mokhov, Mitchell, Peyton Jones — "Build Systems à la Carte" (JFP 2020 version).** The scheduler/rebuilder taxonomy locates arbor precisely (deep constructive trace, cloud) and supplies minimality and correctness definitions so eval-stability can be stated in existing vocabulary. → `04-incrementality.md`
9. **Erdweg, Bračevac, Kuci, Krebs, Mezini (OOPSLA 2015) — co-contextual type rules,** followed by **Zwaan, van Antwerpen, Visser (OOPSLA 2022).** The one item suggesting a concrete change: co-contextual rules would make a de Bruijn *subterm's* hash sufficient to key a type-check aspect, extending caching below the definition root. Zwaan's context-free/context-sensitive split then maps directly onto arbor's hash-vs-namespace boundary. → `04-incrementality.md`
10. **Erdweg, Lichter, Weiel (OOPSLA 2015) — pluto.** The nearest existing *proof* of arbor's theorem shape: sound plus optimal, dynamic dependencies, content-level requirements. Read for proof structure. → `04-incrementality.md`
11. **Matthews & Findler (POPL 2007 / TOPLAS 2009),** with **Patterson & Ahmed (ICFP 2019) — "The Next 700 Compiler Correctness Theorems."** The first is the boundary model arbor deliberately rejects and will be assumed to be doing; the second gives the theorem shape in which a *partial* translator (`Translation_untypable`) is legitimate rather than a defect. → `06-multi-language.md`
12. **Omar, Coblenz, Madhavapeddy (PROPL 2025) — "A FAIR Case for a Live Computational Commons."** The only citable statement of the commons vision. Its FAIR framing maps onto hash-addressing, the naming layer, and translation aspects almost line for line — and it cites no content-addressing work at all. → `07-commons.md`
13. **Kleppmann, Mulligan, Gomes, Beresford (IEEE TPDS) — "A Highly-Available Move Operation for Replicated Trees."** arbor's hierarchical namespace under concurrent agents *is* a replicated tree with moves. This paper shows Google Drive and Dropbox get it wrong and gives a correct algorithm. → `05-naming-versioning.md`
14. **STORM (arXiv:2605.20563)** with **Fork/Explore/Commit (arXiv:2602.08199)** and **CodeCRDT (arXiv:2510.18893).** Read as a set before writing another word of agentic positioning. They respectively give a direct competitor, retire cheap-forking as a novel claim, and force "immutability prevents collisions" to be stated precisely. → `08-agentic-vc.md`
15. **Lam, Dietrich, Pearce (Onward! 2020) — "Putting the Semantics into Semantic Versioning,"** with **Ochoa et al. (EMSE 2022).** The first is the existing case for compatibility-as-a-checkable-object, and the venue and vocabulary to pitch no-silent-breakage into; the second is the sharpest critique of the motivation — breakage is common but rarely *reaches* clients — which means arbor's pitch must rest on certainty and verification cost. → `05-naming-versioning.md`

Two more that did not make the fifteen but are the best entry points to their themes: **Petricek & Edwards (HATRA 2021), "Typed Image-based Programming with Structure Editing"** (`09-scid.md`) is the closest published statement of arbor's exact problem, and **Maziarz et al. (PLDI 2021), "Hashing Modulo Alpha-Equivalence"** (`01-content-addressing.md`) is the one rigorous result for arbor's core primitive.

## Citation hygiene — corrections to things commonly gotten wrong

Collected here because they cut across themes. Several correct leads that were fed *into* the sweeps.

- **Lillibridge & Harper (POPL 1994)**, not "Harper & Lillibridge" — author order is Lillibridge first, per Harper's own list. Affects `../design/12-type-abstraction.md:59` and `docs/prototypes/p17-translucent-modules/00-scope.md:4`.
- **Dreyer, Crary, Harper (POPL 2003)** is "A Type **System** for Higher-Order Modules"; Harper's page informally says "Type Theory."
- **Adapton** is Hammer, Khoo, Hicks, Foster — not Acar or Van Horn.
- **"Incremental Type-Checking for Free"** is Zwaan, van Antwerpen, Visser (OOPSLA 2022). The Pacak/Erdweg/Szabó OOPSLA 2020 paper is "A Systematic Approach to Deriving Incremental Type Checkers."
- **"Refinement Types for Precisely Named Cache Locations"** — Economou and Narasimhamurthy, not Headley.
- **PIE** is a *Programming* journal article, not a conference paper.
- **"Incremental Processing of Structured Data in Datalog"** is GPCE 2022.
- **Memo functions** is Michie alone, *Nature* 218:19–22 — not Bellman.
- **codeQuest** is ECOOP 2006, not ICSE 2006.
- **APIDiff** is SANER 2018, not SBES 2018.
- **"Understanding Breaking Changes in the Wild"** — first author is Jayasuriya, not Dietrich.
- **Semantic history slicing** is Li, Rubin, Chechik — not "Li, Chandra, Rountev." There is no ICSE 2017 paper of that title.
- **"Structured Merge with Auto-Tuning"** is ASE 2012, not 2011, with a different author list from the FSE 2011 semistructured-merge paper.
- **Peritext** — second author is Sarah Lim (not "Lord"); venue is PACM HCI / CSCW, not OOPSLA or TOCHI.
- **FunTAL** — fourth author is Christos Dimoulas, not Skorstengaard.
- **"The Next 700 Compiler Correctness Theorems"** is a functional pearl, not a survey.
- **Tobin-Hochstadt & Felleisen, "Interlanguage Migration"** — cite the *Companion to OOPSLA '06*, pp. 964–974.
- **Siek & Taha (2006)** is a workshop paper (Scheme and Functional Programming), not a refereed conference paper.
- **Barista** is **Amy** J. Ko & Brad Myers, not Andrew.
- **Deuce** has four authors — Grace Lu is routinely dropped.
- **`cargo-semver-checks`**, not `rust-semverver` (deprecated since ~2023).
- **"Coding at the Speed of Touch"** is Sean McDirmid (Onward! 2011), not Jonathan Edwards. Edwards has no "Natural Programming" or "Direct Programming" — the former is Brad Myers' CMU project. His dblp page conflates at least three people of that name.
- **de Bruijn 1972** circulates as both vol. 34 and vol. 75 of *Indagationes Mathematicae*; the JSL review lists both. `formalism/paper/references.bib` says 34.
- **Grove** has six authors — Adams, Griffis, Porter, Satish, Zhao, Omar — and DOI `10.1145/3704909`. The `references.bib` entry said "Michael D. Adams and others" with no DOI, now fixed.

## Negative findings

Things that were searched for and do not exist, or exist only as something weaker than they are usually cited as. Several of these are recorded as entries in `../design/open-questions.md`.

- **No peer-reviewed publication exists for Unison.** Zero DBLP hits. Its design claims — hashes as identity, names as metadata, never-invalidated caches, no diamond-dependency problem — have never been formally stated, let alone proven. `formalism/`'s theorems appear to be the first formal treatment of this model, which is arbor's largest single citation opportunity.
- **SCID is not an academic literature.** One hobbyist essay, one c2 thread, one notability-flagged Wikipedia stub. The current named programme for the idea is **substrates** (Edwards, Petricek, Hirschfeld; Substrates 2025 at ‹Programming›), and Edwards' own vision statement lists "database-based substrates" and "static typing viability" as open problems.
- **"Agentic version control" has zero DBLP hits**, and neither major LLM-agents-for-SE survey has a version-control subcategory. But two claims are already spoken for: cheap forking is an engineered OS primitive with microsecond numbers, and 100% structural convergence still leaves 5–10% semantic conflicts.
- **The published computational-commons vision has no content-addressing component.** The PROPL 2025 paper's 53 references contain no Ostrom, Nelson, Engelbart, Unison, or content-addressing citation; identity there is extrinsic (DOIs, ORCIDs). That gap is arbor's to fill, not to inherit. **Ostrom is an editorial addition** to this line, not lineage — flag it as such if used.
- **No NSF award is titled or abstracted around a "computational commons."** Cite the PROPL papers, not grant text.
- **There are two commons papers, and the repo cites neither.** `propl24` = Bandukwala, Blinn, Omar (PROPL 2024), this project's own position paper, referenced four times in `docs/` as a bare token with no authors or venue. `propl25` = Omar, Coblenz, Madhavapeddy (PROPL 2025), the later paper where the FAIR framing and Fairground live, absent from `docs/` entirely. Both now cited.
- **No academic treatment of Git's object model.** Use Bird et al. (MSR 2009) plus *Pro Git* ch. 10.
- **No citable Roslyn or Hejlsberg incremental-compilation design document.** "Compiler as a service" is product terminology.
- **No canonical "program database" paper.** The phrase means Microsoft's PDB format in practice; the real ancestors are Linton (1984), Masterscope, Damokles, and Desert.
- **Artifacts without papers, cited as such:** Kythe, Glean, LSIF, SCIP, Mergiraf, Dolt, Darklang, Perkeep, IPLD, Automerge, Salsa, Blockly, Lamdu, IBM VisualAge. Glean's often-quoted "title" is its repo tagline.
- **Nobody has published the primary-store-vs-derived-index argument.** Kythe, Glean, CodeQL, and SCIP are all derived indices over files. Skarupke's blog post is the best statement of the other side, and arbor sits on the unclaimed one.
- **Substrates 2025's accepted submissions were never posted.** Likely the richest untapped source here; worth emailing the organizers.
- **No prior work found on canonicalized-AST hashing as a program-store primitive** beyond Maziarz et al., which treats it as an algorithm rather than a storage model. Nor on translation-as-cached-content-addressed-derived-data. Both are failures to find, not proofs of absence.
- **p17's rank rule has no precedent found.** Sigs in the ML tradition are order-sensitive telescopes; binding opaques by label-hash sort order may be novel or may be under-searched.
- **Roughly two dozen items are `[UNVERIFIED]`,** listed per theme under §"Gaps and negative findings" in each file. The highest-risk ones for arbor specifically: the Lillibridge & Harper DOI, Reynolds 1983, and the several 2026 arXiv preprints confirmed from metadata only.

**Spot-check, 2026-08-03.** Twelve DOIs across themes were resolved against Crossref and DataCite: Grove, Maziarz et al., Montagu & Rémy, Leifer et al., F-ing Modules, Omar et al. (PROPL 2025), Mitchell & Plotkin, Nominal Adapton, Zwaan et al., Porter et al., Zhao et al., and Stack Graphs. **All twelve resolved to the claimed title, year, venue, and author list.** Grove's six-author list and Porter et al.'s ordering are both now confirmed rather than inferred from hazel.org.

## Non-goals (current phase)

- **Not a reading plan.** The priority list is ranked by expected impact on what arbor writes down, not sequenced against the roadmap in `../design/09-roadmap.md`.
- **Not a BibTeX mirror.** `formalism/paper/references.bib` carries targeted entries only — the names the design docs already lean on. Mirroring ~200 entries into a paper that cites one would cut against the posture in `CLAUDE.md` §"Conventions".
- **Not a positioning document.** Where a finding sharpens or threatens an arbor claim, it is noted per item and, where it is really a question, filed in `../design/open-questions.md`. Deciding what arbor claims is not this directory's job.
- **Not comprehensive.** Two budget-limited sweeps. Themes E and H (incrementality, agentic) are the best covered; multi-language interop and the record-calculus literature are the thinnest.

## Connections

- `../design/07-hazel-substrate.md:12` — the "items here are inputs, decisions land in `decisions.md`" contract, which this directory adopts unchanged. `:17` and `:21` are the sourcing policy addressed above.
- `../design/00-overview.md:61` §"Document map" — where this directory is registered.
- `../design/open-questions.md` §"Related work" — the five findings from these sweeps that are questions rather than citations.
- `../design/prototype-findings.md` — the observational, non-prescriptive posture this directory shares; findings here are about the literature rather than the prototypes.
- `formalism/paper/references.bib` — the machine-readable subset, and the only place in the repo where a citation is load-bearing.
- `../design/decisions.md` — empty of citations so far. The α-equivalence hashing citation (Maziarz et al.) is the most likely first graduation.
