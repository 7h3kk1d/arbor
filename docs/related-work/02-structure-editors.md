# Structure Editors, Projectional Editing, and Holes

**Status:** Index. Sourced 2026-08-03. Annotations are notes and observations, not substrate policy — see `00-index.md` §"How to read this".

## Why this theme

`../design/07-hazel-substrate.md` is arbor's forward-looking sourcing farm for this material, and deliberately carries no citations. This file supplies them: authors, venues, years, and DOIs for all fourteen Hazel papers plus the surrounding structure-editor tradition. The two should be read together — `07` says *what arbor might consume and when*, this says *what the thing actually is and where to find it*.

Two items here matter beyond citation hygiene. **Grove (POPL 2025)** is the nearest formal artifact to arbor anywhere in the literature — same lab, same problem, opposite identity substrate — and reading it forces arbor to say precisely what content hashes buy over minted unique IDs, and where they lose. **Total Type Error Localization (POPL 2024)** proves that *every* expression can be given a meaning, which puts p9's `Ill_typed`-rejects-ingest behaviour in direct tension with both that result and arbor's own commit-and-report posture.

## The Hazel line

Verified against [hazel.org](https://hazel.org/)'s own publication list. Cyrus Omar's group at Michigan; the source of the *computational commons* framing (see `07-commons.md`).

- **Omar, Voysey, Hilton, Aldrich, Hammer (POPL 2017) — "Hazelnut: A Bidirectionally Typed Structure Editor Calculus."** DOI [10.1145/3009837.3009900](https://doi.org/10.1145/3009837.3009900); arXiv:1607.04180. `[verified]` Edit actions over statically-meaningful incomplete terms; holes as the mechanism keeping every editor state typed. The origin of the "every intermediate state is meaningful" discipline p8/p9 adopt.
- **Omar, Voysey, Hilton, Sunshine, Le Goues, Aldrich, Hammer (SNAPL 2017) — "Toward Semantic Foundations for Program Editors."** DOI [10.4230/LIPIcs.SNAPL.2017.11](https://doi.org/10.4230/LIPIcs.SNAPL.2017.11); arXiv:1703.08694. `[verified]` The position paper, and the best short statement of why editors need semantics rather than syntax.
- **Omar, Voysey, Chugh, Hammer (POPL 2019) — "Live Functional Programming with Typed Holes."** *PACMPL* 3(POPL), art. 14. DOI [10.1145/3290327](https://doi.org/10.1145/3290327); arXiv:1805.00155. `[verified]` Evaluation *through* holes, with indeterminate results and hole closures. Directly relevant to what an eval-cache aspect can even mean for a term containing holes — a question p9 raises and does not answer.
- **Omar, Moon, Blinn, Voysey, Collins, Chugh (PLDI 2021) — "Filling Typed Holes with Live GUIs."** DOI [10.1145/3453483.3454059](https://doi.org/10.1145/3453483.3454059). `[verified]` Livelits: compositional GUI-valued literals with lexically-scoped splices.
- **(ICFP 2020) — "Program Sketching with Live Bidirectional Evaluation."** arXiv:1911.00583. `[verified]`
- **(TyDe 2022) — "tylr: A Tiny Tile-Based Structure Editor."** `[verified — title/venue/year via hazel.org]` Research line #6 in `../design/07-hazel-substrate.md`.
- **(OOPSLA 2023) — "Live Pattern Matching with Typed Holes."** Distinguished Paper. `[verified — via hazel.org]`
- **(VL/HCC 2023) — "Gradual Structure Editing with Obligations."** `[verified — via hazel.org]`
- **Zhao, Maroof, Dukkipati, Blinn, Pan, Omar (POPL 2024) — "Total Type Error Localization and Recovery with Holes."** *PACMPL* 8(POPL). Distinguished Paper; Agda-mechanized. DOI [10.1145/3632910](https://doi.org/10.1145/3632910). `[verified]` **[Priority read.]** The marked lambda calculus: *every* expression, well-typed or not, gets a meaning. This is the principled version of what p9's three-outcome checker (`Type_of` / `Type_with_holes` / `Ill_typed`) approximates — and note that arbor's `Ill_typed` **rejects ingest**, which is precisely the totality this paper argues against, and also cuts against arbor's own commit-and-report posture. Filed as an open question.
- **(OOPSLA 2024) — "Statically Contextualizing Large Language Models with Typed Holes."** `[verified — via hazel.org]` Research line #8's actual citation.
- **(TFP 2024) — "Polymorphism with Typed Holes."** `[verified — via hazel.org]`
- **Adams, Griffis, Porter, Satish, Zhao, Omar (POPL 2025) — "Grove: A Bidirectionally Typed Collaborative Structure Editor Calculus."** *PACMPL* 9(POPL). DOI [10.1145/3704909](https://doi.org/10.1145/3704909); [PDF](https://hazel.org/papers/grove-popl25.pdf); artifact DOI [10.5281/zenodo.14026532](https://doi.org/10.5281/zenodo.14026532). `[verified]` **[Priority read.]** No patch synthesis, no three-way merge: edits derive from the action log, all edits commute (a CmRDT), and the core datatype is a **labeled directed multigraph with uniquely identified vertices and edges** so concurrent relocation survives. **The single most directly comparable formal artifact to arbor** — same lab, same problem, opposite identity substrate (minted per-vertex UIDs vs. content hashes). Reading it forces arbor to say exactly what hashes buy over unique IDs, and to concede where they lose: a hash cannot express "this node moved." Note `references.bib`'s existing entry credits only "Michael D. Adams and others" with no DOI; the full author list is above.
- **Porter, Kirisame, Wei, Panchekha, Omar (OOPSLA 2025) — "Incremental Bidirectional Typing via Order Maintenance."** *PACMPL* 9(OOPSLA2). Distinguished Paper. DOI [10.1145/3763117](https://doi.org/10.1145/3763117); arXiv:[2504.08946](https://arxiv.org/abs/2504.08946). `[verified — author list and ordering confirmed against Crossref 2026-08-03]` **[Priority read.]** Discussed in `04-incrementality.md`, where it belongs analytically: it solves exactly the keystroke-to-keystroke inner loop that content-hash caching cannot touch, because every keystroke is a fresh hash.
- **Moon (OOPSLA 2025) — "Syntactic Completions with Material Obligations."** arXiv:2508.16848. `[verified]`
- **(VL/HCC 2025) — "Hazel Deriver"** and **(HATRA 2025) — decomposable type highlighting.** arXiv:2506.10781. `[verified — via hazel.org]`

## The earlier structure-editor tradition

- **Tim Teitelbaum & Thomas Reps (CACM 1981) — "The Cornell Program Synthesizer: A Syntax-Directed Programming Environment."** 24(9):563–573. DOI [10.1145/358746.358755](https://doi.org/10.1145/358746.358755). `[verified]` **[Priority read — for one sentence.]** "Programs are not text; they are hierarchical compositions of computational structures and should be edited, executed, and debugged in an environment that consistently acknowledges and reinforces this viewpoint." That sentence is the thesis the entire structure-editing and code-in-database line argues for; cite it rather than the SCID wiki (see `09-scid.md`).
- **Borras, Clement, Despeyroux, Incerpi, Kahn, Lang, Pascual (SDE 3, 1988) — "Centaur: the system."** pp. 14–24. DOI [10.1145/64135.65005](https://doi.org/10.1145/64135.65005). `[verified]` The generic, language-parameterized structure-editing environment — ancestor of the language workbenches.
- **Mentor** (Donzeau-Gouge, Huet, Kahn, Lang) — the commonly-cited item is "Programming environments based on structured editors: the MENTOR experience," INRIA research report, c. 1980. `[UNVERIFIED — check before citing]`
- **Amir Ali Khwaja & Joseph E. Urban (SAC 1993) — "Syntax-Directed Editing Environments: Issues and Features."** pp. 230–237. DOI [10.1145/162754.162882](https://doi.org/10.1145/162754.162882). `[verified]` The survey. Useful chiefly for the *historical objections* to structure editing that Hazel and Pantograph later answer.
- **Amy J. Ko & Brad A. Myers (CHI 2006) — "Barista: An Implementation Framework for Enabling New Tools, Interaction Techniques and Views in Code Editors."** pp. 387–396. DOI [10.1145/1124772.1124831](https://doi.org/10.1145/1124772.1124831). `[verified]` (**Amy** J. Ko, not Andrew.)
- **Resnick et al. (CACM 2009) — "Scratch: Programming for All."** 52(11):60–67. DOI [10.1145/1592761.1592779](https://doi.org/10.1145/1592761.1592779). `[verified]` Companion: **Maloney, Resnick, Rusk, Silverman, Eastmond (TOCE 2010) — "The Scratch Programming Language and Environment,"** 10(4), DOI [10.1145/1868358.1868363](https://doi.org/10.1145/1868358.1868363). `[verified]` Blockly has no paper.
- **Reps & Teitelbaum — *The Synthesizer Generator*** (1984/1989), the natural follow-on to the Cornell Program Synthesizer. `[UNVERIFIED — check before citing]`
- **Agda holes / Idris interactive editing / Lamdu.** The likely citations are Ulf Norell's 2007 Chalmers PhD thesis (*Towards a Practical Programming Language Based on Dependent Type Theory*) and Edwin Brady's Idris work; Lamdu has no publication. `[UNVERIFIED — check before citing]`

## Projectional editing and language workbenches

- **Markus Voelter (GTTSE IV, LNCS 7680, Springer 2013) — "Language and IDE Modularization and Composition with MPS."** pp. 383–430. DOI [10.1007/978-3-642-35992-7_11](https://doi.org/10.1007/978-3-642-35992-7_11). `[verified]` **Cite this, not** Voelter & Pech (ICSE 2012), which is a two-page tool demo. MPS is the industrial proof that projectional editing over a structured store works — programs are stored as models, never as text, which makes it the largest deployed code-in-database system. The famous trade-off: language composition becomes easy, text-editor muscle memory and diff/merge become bespoke. `../design/01-language-model.md:55` already name-drops MPS with no source.
- **Erdweg, van der Storm, Völter et al. (SLE 2013) — "The State of the Art in Language Workbenches: Conclusions from the Language Workbench Challenge."** LNCS 8225, pp. 197–217. DOI [10.1007/978-3-319-02654-1_11](https://doi.org/10.1007/978-3-319-02654-1_11). `[verified]` 22 authors; the survey that maps the space `../design/01-language-model.md` §"Language workbench landscape (notes)" sketches from memory.
- **Voelter (ISoLA 2018) — "Fusing Modeling and Programming into Language-Oriented Programming."** pp. 309–339. `[verified — secondary]`

## Structure editing layered over text

The design point p7/p9/p15's web UI keeps rediscovering, and the one place recent work argues *against* arbor's direction.

- **Chugh, Hempel, Spradlin, Albers (PLDI 2016) — "Programmatic and Direct Manipulation, Together at Last"** (Sketch-n-Sketch). `[verified]` Follow-on: **Hempel, Lubin, Chugh (UIST 2019) — "Sketch-n-Sketch: Output-Directed Programming for SVG,"** DOI [10.1145/3332165.3347925](https://doi.org/10.1145/3332165.3347925). `[verified]`
- **Brian Hempel, Justin Lubin, Grace Lu, Ravi Chugh (ICSE 2018) — "Deuce: A Lightweight User Interface for Structured Editing."** `[verified — four authors; Grace Lu is usually dropped from secondary citations]` Structure-editing affordances layered *over* text rather than replacing it.
- **Jacob Prinz, Henry Blanchette, Leonidas Lampropoulos (POPL 2025) — "Pantograph: A Fluid and Typed Structure Editor."** *PACMPL* 9(POPL), art. 28. DOI [10.1145/3704864](https://doi.org/10.1145/3704864); arXiv:[2411.16571](https://arxiv.org/abs/2411.16571). `[verified]` Cut-and-paste of *one-hole contexts*, typed by a **category of type diffs**, with a user study against a text editor. The main non-Hazel typed structure editor, the strongest recent evidence that structure editing can be fluid, and — the part arbor should care about — the type-diff category is a formal object the edit calculus in `formalism/paper/arbor-core.tex` §"Part V" could plausibly reuse.
- **Beckmann, Thiede, Lincke, Hirschfeld (2026) — "Hybrid Structured Editing: Structures for Tools, Text for Users."** *The Art, Science, and Engineering of Programming* 11(1), art. 1. arXiv:[2603.05644](https://arxiv.org/abs/2603.05644). `[verified]` Tool builders declare structural constraints the system enforces; users keep ordinary text editing. **The strongest recent published counterargument to storing only structure** — worth reading as the opposing position rather than as an ally. Discussed further in `09-scid.md`, where it sits in the substrates conversation.

## Where it plugs in

- `../design/07-hazel-substrate.md` — the citations for all ten research lines, which that doc deliberately leaves un-sourced. Research line #10 (Grove) is the only place in `docs/` with a venue and year; everything else gets its anchor here.
- `../design/03-content-addressing.md` — Grove's UID-vs-content-hash split is the concrete form of the edit-layer/committed-layer identity distinction `07-hazel-substrate.md:120` flags as "not a problem to solve today, but real."
- `../design/02-definitions-and-derived-data.md` — Live Functional Programming with Typed Holes is what an eval aspect means for a term with holes.
- `docs/prototypes/p8-holes/00-scope.md`, `docs/prototypes/p9-typed-namespaces/00-scope.md` — p8's bare `Hole` and p9's three-outcome checker are both approximations of the POPL 2024 marked lambda calculus.
- `formalism/paper/arbor-core.tex` §"Part V — The edit calculus" — Hazelnut is the calculus this part is closest to; Pantograph's type diffs are a candidate formal object.
- `docs/prototypes/p15-open-existentials/00-scope.md`, `docs/prototypes/p17-translucent-modules/00-scope.md` — the web-UI work sits in the Deuce / Hybrid Structured Editing tradition rather than the fully projectional one.

## Gaps and negative findings

- **arbor's `Ill_typed` rejects ingest**, contradicting both the POPL 2024 totality result and the repo's own commit-and-report posture (`04-naming-layer.md`'s visible-breakage stance). Filed in `../design/open-questions.md`.
- **Grove uses minted UIDs, not content hashes.** arbor has not yet articulated what hashes buy over UIDs, or conceded what they cost — a hash cannot represent a move. This is the sharpest available comparison and arbor currently ducks it.
- Four items in this theme are `[UNVERIFIED]`: Mentor, the Synthesizer Generator, the Agda/Idris/Lamdu citations, and the exact author ordering on Porter et al. (OOPSLA 2025).
- **Blockly, Lamdu, and Darklang's editor have no publications.** Darklang is covered in `09-scid.md` as a cautionary tale rather than a citation.
