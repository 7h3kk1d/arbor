# Related Work — Reading Plan

**Status:** Sequencing layer, drafted 2026-08-06. `00-index.md`'s priority list ranks items by expected impact on what arbor writes down; this file turns that ranking plus the per-theme annotations into an actual reading order. It adds no new items and changes no verification tags — every entry below is annotated in full in its theme file, which is where reading notes should land.

Tracks are keyed to the dissertation paper plan from 2026-08 planning (foundations → abstraction → evolution → multi-language, with the vision paper alongside). That plan is not yet a design doc; if it becomes one, re-key the tracks to it.

## How to use this

Three doors, use whichever fits the week:

1. **The spine** — four papers plus one afternoon of short reads. Do this first regardless of anything else.
2. **Tracks** — six sequences keyed to what each unblocks. Enter whichever matches what you're about to write or build.
3. **Jump points** — "I'm about to do X, what must I have read?" Lookup table at the bottom.

Reading discipline, so the corpus stays alive: every read ends with a delta — an upgraded annotation in the theme file, an entry in `../design/open-questions.md`, a graduation into `../design/decisions.md`, or an explicit "read; nothing changed." While reading, verify anything the theme file tags `[UNVERIFIED]` that you intend to cite.

Tags: **[deep]** read carefully for structure and proofs; **[position]** read for the claim and the vocabulary, an hour or two; **[opposed]** read adversarially — this one threatens a claim; **(short)** under an hour; **(heavy)** budget multiple sessions.

Already digested — do not re-queue: the Software Heritage CiSE 2020 article (read in full 2026-08-03; notes in `01-content-addressing.md` §"What the Software Heritage identifier papers actually argue"); propl24 (your own paper); and **Unison, read in source 2026-08-07 at commit `db60ce2` — notes in `../systems/unison/`, which is where further Unison findings belong.** PROPL 2025 has had only its first pages read — it is queued below.

## The spine — read first, in order

Four papers and an afternoon. After these you can state arbor's core claims against the two sharpest pieces of prior art, the one rigorous result under the core primitive, and the nearest formal neighbour.

1. **Leifer, Peskine, Sewell, Wansbrough (ICFP 2003) — "Global Abstraction-Safe Marshalling with Hash Types."** [deep] Shortest of the Sewell cluster and most on-point: `Opaque{mint, witness}` with the witness-in-hash soundness argument already written, twenty years early. → `03-modules-abstraction.md`
2. **Sewell (POPL 2001) — "Modules, Abstract Types, and Distributed Versioning."** [deep] Abstract-type identity across versions and independently-compiled programs — unacknowledged prior art for no-silent-breakage. Read directly after #1 while the construction is fresh. → `03-modules-abstraction.md`
3. **Maziarz, Ellis, Lawrence, Fitzgibbon, Peyton Jones (PLDI 2021) — "Hashing Modulo Alpha-Equivalence."** [deep] The one rigorous result for the primitive everything sits on, and the most likely first graduation into `../design/decisions.md`. → `01-content-addressing.md`
4. **Adams, Griffis, Porter, Satish, Zhao, Omar (POPL 2025) — "Grove."** [deep] [opposed] (heavy) Same lab, same problem, opposite identity substrate. Read to answer, in writing, what hashes buy over UIDs — and to concede what they cost (a hash cannot express a move). → `02-structure-editors.md`
5. **The short-reads afternoon** [position] (short each): ~~Unison's "The big idea" + the `unique`-types page~~ **— superseded 2026-08-07 by the source read in `../systems/unison/`, which confirmed mint-by-default and per-sort minting from the implementation and closed both open questions.** Remaining: Perkeep's permanode schema page (the mint as a separate signed identity node, shipped); Jonathan Edwards' Substrates vision statement (the named programme arbor belongs to, listing "database-based substrates" and "static typing viability" as open). → `01-content-addressing.md`, `09-scid.md`

## Track A — Foundations and caching

*Unblocks: the foundations paper; `formalism/paper/arbor-core.tex`; stating eval-stability in existing vocabulary.*

1. **Mokhov, Mitchell, Peyton Jones — "Build Systems à la Carte" (JFP 2020).** [deep] (heavy) The scheduler × rebuilder grid that locates arbor (deep constructive trace, cloud) and the minimality/correctness definitions to state eval-stability against. → `04-incrementality.md`
2. **Erdweg, Lichter, Weiel (OOPSLA 2015) — pluto.** [deep] The nearest existing proof of the theorem shape (sound + optimal, dynamic dependencies). Read for proof structure, immediately after #1. → `04-incrementality.md`
3. **Hammer et al. (OOPSLA 2015) — Nominal Adapton**, with a skim of **"Refinement Types for Precisely Named Cache Locations."** [deep] Minted mark ≈ nominal name, content hash ≈ structural key — the sharpest statement of the identity design. → `04-incrementality.md`
4. **Dolstra (2006) — PhD thesis, the intensional-store chapter only.** [position] The design arbor implements, described fifteen years before Nix shipped it. → `01-content-addressing.md`
5. **Klein et al. (POPL 2012) — "Run Your Research."** [position] Errors found in all nine re-mechanized semantics — the methodological citation for prototypes-as-evidence. → `11-benchmarks-and-evaluation.md`
6. **Erdweg, Bračevac, Kuci, Krebs, Mezini (OOPSLA 2015) — co-contextual type rules**, then **Zwaan, van Antwerpen, Visser (OOPSLA 2022).** [deep] The one item suggesting a concrete change (caching below the definition root); Zwaan's context-free/context-sensitive split is the hash/namespace boundary. → `04-incrementality.md`
7. **Porter, Kirisame, Wei, Panchekha, Omar (OOPSLA 2025) — incremental bidirectional typing.** [position] The granularity gap eval-stability does not cover — the keystroke inner loop. Know the boundary of the claim before a reviewer finds it. → `04-incrementality.md`
8. **Zhao, Maroof, Dukkipati, Blinn, Pan, Omar (POPL 2024) — total type error localization.** [deep] [opposed] The totality result that p9's `Ill_typed`-rejects-ingest contradicts — and so does arbor's own commit-and-report posture. → `02-structure-editors.md`
9. **When mechanization is scheduled, not before:** POPLmark (TPHOLs 2005, skim) then **POPLmark Reloaded (JFP 2019)** [deep] — the current test of binding-infrastructure choice. → `11-benchmarks-and-evaluation.md`

## Track B — Modules and abstraction

*Unblocks: the abstraction paper; positioning p12–p17; judging the rank rule.*

1. **Rossberg, Russo, Dreyer — "F-ing Modules" (JFP 2014).** [deep] (heavy) The p13–p17 architecture stated as a theorem: `∀`, `∃`, records suffice. The frame for judging everything else in this track. → `03-modules-abstraction.md`
2. **Montagu & Rémy (POPL 2009) — F-zip.** [deep] `Open` and `Open_local` are the open and closed forms of one operation; this names the avoidance condition p14–p17 enforce by hand. → `03-modules-abstraction.md`
3. **Cardelli & Leroy (1990) — "Abstract Types and the Dot Notation."** [deep] Under-cited and directly on the p15→p17 axis (`M.t` vs. unpack). → `03-modules-abstraction.md`
4. **Leroy (JFP 1996) — "A Syntactic Theory of Type Generativity and Sharing."** [deep] The applicative/generative distinction that decides whether `Open` must mint — which p17 answers without formal justification. → `03-modules-abstraction.md`
5. **Crary (POPL 2017) — "Modules, Abstraction, and Parametric Polymorphism."** [deep] The theorem the opacity-parameterized checker needs if "opacity lives in the editing layer" is to be sound rather than convenient. → `03-modules-abstraction.md`
6. **Grossman, Morrisett, Zdancewic (TOPLAS 2000) — "Syntactic Type Abstraction."** [position] `Seal` nodes as coercions, the open set as the principal — very close to p12's actual design. → `03-modules-abstraction.md`
7. **MacQueen (LFP 1984 / 1988).** [position] (short) Skim for the type-stamp tradition — the ancestor of mints, with the difference (session-local counters vs. persisted-in-hash) worth a paragraph in `../design/10-minted-identity.md`. → `03-modules-abstraction.md`
8. **Neis, Dreyer, Rossberg — "Non-Parametric Parametricity."** [position] The sleeper: what abstraction guarantees survive dynamic type generation — which is what a runtime mint is. → `03-modules-abstraction.md`

## Track C — Naming, versioning, evolution

*Unblocks: the evolution paper; pitching no-silent-breakage; update strategies.*

1. **Lam, Dietrich, Pearce (Onward! 2020) — "Putting the Semantics into Semantic Versioning."** [position] The venue and vocabulary to pitch no-silent-breakage into. → `05-naming-versioning.md`
2. **Ochoa et al. (EMSE 2022) — "Breaking Bad?"** [opposed] Read immediately after #1, adversarially: most breaking changes never reach clients, so the pitch must rest on certainty and verification cost, not breakage frequency. → `05-naming-versioning.md`
3. **Jayasuriya et al. (ISSTA 2023) — breaking changes in the wild.** [position] The source/binary/behavioural taxonomy: which classes arbor eliminates by construction, and that behavioural is the class it must actually earn. → `05-naming-versioning.md`
4. **MolhadoRef (ICSE 2007) + Ellis, Nadi, Dig (TSE 2023).** [position] Read as a pair — the aspiration and its sixteen-years-later evaluation. The argument for recording intent at authoring time, i.e. for mints. → `05-naming-versioning.md`
5. **Kleppmann, Mulligan, Gomes, Beresford (TPDS) — the move operation**, plus **PaPoC 2019 interleaving anomalies** (short). [deep] The namespace under concurrent agents is a replicated tree with moves; and convergence ≠ correctness, a distinction the theorems must make. → `05-naming-versioning.md`
6. **Edwards & Petricek — "Baseline: Operation-Based Evolution and Versioning of Data."** [deep] The closest existing work to the edit calculus (`arbor-core.tex` Part V). → `09-scid.md`
7. Skims as needed: Dietrich et al. (MSR 2019) for the pin-vs-float distribution; REPENT for the brittle-names evidence; Schesch & Ernst (ASE 2024) before any comparative merge claim. → `05-naming-versioning.md`

## Track D — Agentic concurrency

*Unblocks: any further agentic positioning — do not write more of it before the three-preprint set.*

1. **The set, together: STORM + Fork/Explore/Commit + CodeCRDT.** [opposed] Direct competitor with benchmark wins; cheap forking already engineered one layer down; 100% convergence still leaves 5–10% semantic conflicts. These respectively force restating claims (a), (b), (c) in `08-agentic-vc.md` §"Why this theme". → `08-agentic-vc.md`
2. **CodePlan (FSE 2024).** [position] A hand-built approximation of `Store.callers_of` — the framing line "that planning problem is a query in a content-addressed store." → `08-agentic-vc.md`
3. Skims: the Liu et al. TOSEM survey (only to cite the absence of a version-control subcategory); the AIDev rejection study (46.41% of agent PRs rejected — the motivation number); Khan's isolation-anomaly preprint if `formalism/` wants the vocabulary. → `08-agentic-vc.md`

## Track E — Multi-language and the benchmark framing

*Unblocks: the multi-language paper (last in publication order — this track can wait, except #1–2 which protect against misreading).*

1. **Matthews & Findler (POPL 2007 / TOPLAS 2009).** [deep] The boundary model reviewers will assume arbor uses unless told otherwise. → `06-multi-language.md`
2. **Patterson & Ahmed (ICFP 2019) — "The Next 700 Compiler Correctness Theorems."** [deep] The theorem shape in which a partial translator (`Translation_untypable`) is legitimate. → `06-multi-language.md`
3. **Patterson & Ahmed (SNAPL 2017) — linking types** (short), then **Patterson et al. (PLDI 2022) — semantic soundness for interop.** [position] The obligation the store invariant discharges by fiat — "we forbid it" should be recognized as an answer to this question. → `06-multi-language.md`
4. **Hofmann, Pierce, Wagner (POPL 2012) — "Edit Lenses" + the Cambria essay.** [position] The pair behind every "Cambria-style" mention in the design docs; note the divergence (bidirectional edit-granular lenses vs. one-directional definition-granular translators). → `06-multi-language.md`
5. **If the benchmark/toolkit framing proceeds:** the LWC COMLAN 2015 paper [deep], Are We Fast Yet (DLS 2016) [position], and a skim of the lambda-n-ways repo — the four-genre option space and the "challenges outlive task suites" lesson. → `11-benchmarks-and-evaluation.md`

## Track F — Vision, substrates, interface

*Unblocks: the vision paper; every paper's introduction; a future interface prototype.*

1. **Omar, Coblenz, Madhavapeddy (PROPL 2025) — the FAIR paper.** [deep] Finish it (only first pages read). Distill the FAIR rubric into `../design/07-hazel-substrate.md`'s requirements checklist while it's fresh. → `07-commons.md`
2. **Petricek & Edwards (HATRA 2021) — "Typed Image-based Programming with Structure Editing."** [deep] (short) The closest published statement of arbor's exact problem. → `09-scid.md`
3. **Jakubovic, Edwards, Petricek — "Technical Dimensions of Programming Systems."** [position] (heavy) The comparison instrument for arbor vs. Unison, Hazel, MPS, Smalltalk; pairs with Green & Petre's Cognitive Dimensions. → `09-scid.md`, `11-benchmarks-and-evaluation.md`
4. **Linton (SDE 1984) — relational views of programs.** [position] The one peer-reviewed citation for the code-in-database thesis; Skarupke's blog post (short) alongside as the best statement of primary-store-vs-derived-index. → `09-scid.md`
5. **Moldable line, when interface work resumes:** Nierstrasz & Gîrba (EuroPLoP 2024) patterns [deep]; the Moldable Inspector (Onward! 2015) [position]; Webstrates (UIST 2015) [position]; the Ink & Switch malleable-software essay (short). The candidate claim to keep in view: a moldable view keyed by content hash inherits eval-stability. → `10-malleable-tooling.md`

## Jump points

| About to… | Read first |
|---|---|
| Draft the formalism paper's intro / theorem statements | Spine 1–3, then A1–A2 (+ the SWH notes already in `01`) |
| Answer "why hashes rather than UIDs" in writing | Spine 4 (Grove) + A3 (Nominal Adapton) + `01` §"Why content-address at all" |
| Write the abstraction paper's related work | Spine 1–2, then B1–B5 |
| Decide whether the rank rule is a claim | B1 (F-ing Modules), then `03` §"Gaps" |
| Pitch no-silent-breakage anywhere | C1 + C2 together |
| Write anything agentic | D1 (the set), no exceptions |
| Reopen the ingest-policy question (`Ill_typed`) | A8 (Zhao et al.) |
| Choose mechanization infrastructure | A9 (POPLmark pair) |
| Start the multi-language paper | E1–E3 |
| Decide the benchmark/toolkit framing | E5 + `11` §"Why this theme" |
| Draft the vision paper / any intro's commons paragraph | F1 + F2, then F4 |
| Design the next interface prototype | F5 + Min et al. (CHI 2025) in `10` |

## One queue, if you want it linear

Spine (1–5) → A1–A3 → B1–B2 → C1–C2 → alternate B3–B8 with C3–C6 → D1 before any agentic writing → F1–F2 early, F3–F4 while drafting intros → A6–A8 when the formalism paper is live → E and the rest of F when their papers or prototypes spin up. A9 and E5 are conditional — triggered by decisions, not by the calendar.

## Not yet in the corpus

Two pending additions will slot into existing tracks when written: the Smalltalk/GemStone/ENVY deep-dive for `09-scid.md` (Copeland & Maier 1984, Robbes & Lanza, CoExist — joins Track F after F4), and anything recovered from Substrates 2025's unpublished submissions (worth the email to the organizers; joins Track F). Neither blocks any track.
