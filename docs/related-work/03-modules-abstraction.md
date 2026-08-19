# Type Abstraction, Modules, Existentials, Generativity

**Status:** Index. Sourced 2026-08-03. Annotations are notes and observations, not substrate policy — see `00-index.md` §"How to read this".

## Why this theme

This is the theme where arbor's docs lean hardest on named authors with no bibliographic anchor anywhere in the repo. `../design/12-type-abstraction.md:59` invokes "the translucent-sum design of **Harper–Lillibridge and Leroy**"; `../design/open-questions.md:98` and both p14 and p15 scope docs invoke "**Montagu–Rémy** / avoidance problem"; `docs/prototypes/p17-translucent-modules/00-scope.md:4` names Harper–Lillibridge and Leroy again. None of the three appears in `formalism/paper/references.bib`. This file supplies those anchors and three more things:

1. **The `Sig`/`Struct`/`Ascribe` architecture already has a theorem.** Rossberg, Russo & Dreyer's *F-ing Modules* proves that plain `∀`, `∃`, and records suffice for full ML modules. p13's `Forall`, p14's existentials, p16's `Record`, and p17's `Sig` are precisely that target language. If arbor cites one modules paper, it is that one.
2. **`Open` and `Open_local` are the open and closed forms of one operation, and the name for that is F-zip.** Montagu & Rémy split `pack`/`unpack` so a witness can be introduced at one point and closed at another — exactly the difference between p17's scoped `Open_local` (no mint) and generative top-level `Open`.
3. **The Sewell cluster is unacknowledged prior art for p12.** Abstract-type names generated *by hashing*, so type equality suffices to protect abstraction across a distributed system, with the soundness argument for why the witness belongs in the hash already written — in 2001–2006. This is the most consequential finding in the whole survey and gets its own section below.

## The classical core

- **John C. Mitchell & Gordon D. Plotkin (TOPLAS 1988) — "Abstract Types Have Existential Type."** 10(3):470–502; POPL 1985 original. DOI [10.1145/44501.45065](https://doi.org/10.1145/44501.45065). `[verified]` The identification p14 implements directly.
- **Luca Cardelli & Peter Wegner (1985) — "On Understanding Types, Data Abstraction, and Polymorphism."** *ACM Computing Surveys* 17(4):471–523. DOI [10.1145/6041.6042](https://doi.org/10.1145/6041.6042). `[verified]`
- **John C. Reynolds (IFIP Congress 1983) — "Types, Abstraction and Parametric Polymorphism."** `[UNVERIFIED — the 1983 paper certainly exists but the exact record was not confirmed; the follow-on **QingMing Ma & Reynolds (MFPS 1992), "Types, abstraction, and parametric polymorphism, part 2,"** pp. 1–40, DOI [10.1007/3-540-55511-0_1](https://doi.org/10.1007/3-540-55511-0_1) is verified]`
- **Luca Cardelli & Xavier Leroy (IFIP TC2 1990) — "Abstract Types and the Dot Notation."** pp. 479–504; also DEC SRC Technical Report 56. `[verified — via Leroy's own publication list]` **[Priority read — under-cited and directly on point.]** The dot notation `M.t` versus existential `unpack`: exactly the design axis p15 and p17 traverse when `Open` and `Open_local` make `M.t` and `M#f` resolvable through the resolver's `tctx`. Cite this for why the dot notation is not merely sugar.

## The ML module tradition

- **David MacQueen (LFP 1984) — "Modules for Standard ML."** pp. 198–207. DOI [10.1145/800055.802036](https://doi.org/10.1145/800055.802036). `[verified]` And **MacQueen (LFP 1988) — "An Implementation of Standard ML Modules,"** pp. 212–223, DOI [10.1145/62678.62704](https://doi.org/10.1145/62678.62704) `[verified]` — the closest citable account of the **type-stamp** implementation tradition. Plus **Milner, Tofte, Harper, MacQueen (1997), *The Definition of Standard ML (Revised)*,** MIT Press, DOI [10.7551/mitpress/2319.001.0001](https://doi.org/10.7551/mitpress/2319.001.0001). `[verified]`

  **Stamps are the direct ancestor of arbor's mints**, and the parallel deserves an explicit paragraph somewhere in `../design/10-minted-identity.md`: SML/NJ stamps are compilation-session-local counters; arbor's mints are persisted *in hashed bytes* and therefore globally meaningful. That is a real difference, not a re-implementation.
- **Mark Lillibridge & Robert Harper (POPL 1994) — "A Type-Theoretic Approach to Higher-Order Modules with Sharing."** `[verified via Harper's own publication list; DOI unconfirmed]` The translucent-sum calculus p17 is named after. **Author order is Lillibridge first**, contrary to the "Harper–Lillibridge" shorthand at `../design/12-type-abstraction.md:59` and `docs/prototypes/p17-translucent-modules/00-scope.md:4`.
- **Xavier Leroy** — all four verified against his own publication list:
  - **(POPL 1994) — "Manifest Types, Modules, and Separate Compilation."** pp. 109–122. `[verified]` p17's `Smanifest` in its original form.
  - **(POPL 1995) — "Applicative Functors and Fully Transparent Higher-Order Modules."** pp. 142–153. `[verified]`
  - **(JFP 1996) — "A Syntactic Theory of Type Generativity and Sharing."** 6(5):667–698. `[verified]` **[Priority read.]** The syntactic account of exactly the applicative/generative distinction that decides whether arbor's `Open` must mint — which p17 answers "yes, top-level; no, local" without a formal justification.
  - **(JFP 2000) — "A Modular Module System."** 10(3):269–303. `[verified]`
- **Derek Dreyer, Karl Crary, Robert Harper (POPL 2003) — "A Type System for Higher-Order Modules."** pp. 236–249. `[verified — title confirmed on Crary's page; Harper's page informally lists it as "A Type *Theory* for…", use the POPL title]` Singleton kinds plus a static/dynamic phase distinction; the reference account of higher-order module type theory.
- **Claudio V. Russo (2004) — "Types for Modules."** *ENTCS* 60:3–421 (published version of his 1998 Edinburgh PhD thesis). DOI [10.1016/S1571-0661(05)82621-0](https://doi.org/10.1016/S1571-0661(05)82621-0). `[verified]` First-class modules in Moscow ML; the origin of "modules are just terms of existential type, and you can pack them at runtime" — the position p17's `Ascribe` takes.
- **Andreas Rossberg, Claudio V. Russo, Derek Dreyer — "F-ing Modules."** TLDI 2010, pp. 89–102, DOI [10.1145/1708016.1708028](https://doi.org/10.1145/1708016.1708028); journal version *JFP* 24(5):529–607, 2014, DOI [10.1017/S0956796814000264](https://doi.org/10.1017/S0956796814000264). `[verified]` **[Priority read — the JFP version.]** The full elaboration of ML modules into plain System-Fω: no singleton kinds, no module calculus, just `∀`, `∃`, and records. **This is arbor's actual p13–p17 architecture stated as a theorem.** If arbor cites one modules paper, this is it.
- **Andreas Rossberg (ICFP 2015) — "1ML — Core and Modules United (F-ing First-Class Modules)."** pp. 35–47. DOI [10.1145/2784731.2784738](https://doi.org/10.1145/2784731.2784738). `[verified]` (A JFP 2018 extended version exists — `[UNVERIFIED]`.) Collapses the core/module stratification entirely. Relevant because arbor already has no stratification: `Struct`, `Sig`, and `Ascribe` are ordinary `Node`/`Tnode` constructors.
- **Zhong Shao (ICFP 1999) — "Transparent Modules with Fully Syntactic Signatures."** pp. 220–232. DOI [10.1145/317636.317801](https://doi.org/10.1145/317636.317801). `[verified]`
- **Robert Harper & Benjamin C. Pierce (2005) — "Design Considerations for ML-Style Module Systems,"** ch. 8 of *Advanced Topics in Types and Programming Languages*, MIT Press. `[UNVERIFIED — almost certainly correct, and probably the best single tutorial entry point; confirm before citing]`
- **Frisch & Garrigue (ML 2010)** on first-class modules in OCaml 3.12. `[UNVERIFIED — check before citing]`

## Open existentials and avoidance

- **Benoît Montagu & Didier Rémy (POPL 2009) — "Modeling Abstract Types in Modules with Open Existential Types."** pp. 354–365. DOI [10.1145/1480881.1480926](https://doi.org/10.1145/1480881.1480926); [PDF](http://gallium.inria.fr/~remy/modules/Montagu-Remy@popl09:fzip.pdf). `[verified]` **[Priority read.]** **The direct theoretical basis for p15's `open` and p17's `Open`/`Open_local`.** F-zip splits `pack`/`unpack` so an existential's witness can be *introduced* at one point and *closed* at another, rather than being scoped to an `unpack` body. That is exactly the difference between `Open_local` (scoped, no mint, α-equivalence preserved) and top-level `Open` (generative, mints a witness-less `Abstract`). **Your two constructors are, in F-zip terms, the closed and open forms of one operation** — worth saying explicitly, and F-zip gives you a name for the avoidance condition p14/p15/p17 enforce by hand.
- **Montagu (2010) — PhD dissertation.** [HAL tel-00550331](http://hal.inria.fr/tel-00550331_v1/). `[verified]` Also "A Logical Account of Type Generativity: Abstract Types Have Open Existential Types" (draft/MSR 2008) `[venue UNVERIFIED]`.
- **Konstantin Läufer & Martin Odersky (TOPLAS 1994) — "Polymorphic Type Inference and Abstract Data Types."** 16(5):1411–1430. DOI [10.1145/186025.186031](https://doi.org/10.1145/186025.186031). `[verified]`
- **Richard A. Eisenberg et al. (ICFP 2021) — "An Existential Crisis Resolved: Type Inference for First-Class Existential Types."** *PACMPL* 5(ICFP). DOI [10.1145/3473569](https://doi.org/10.1145/3473569). `[verified as a record; full author list UNVERIFIED — Duboc and Weirich appear in secondary sources]`
- **Benjamin C. Pierce & David N. Turner (TOPLAS 2000) — "Local Type Inference."** 22(1):1–44; POPL 1998 original. DOI [10.1145/345099.345100](https://doi.org/10.1145/345099.345100). `[verified]`

## Abstraction, parametricity, and generativity

- **Karl Crary (POPL 2017) — "Modules, Abstraction, and Parametric Polymorphism."** pp. 100–113. DOI [10.1145/3009837.3009892](https://doi.org/10.1145/3009837.3009892). `[verified]` Proves that ML-style sealing really does deliver parametricity-style abstraction — **the theorem arbor's opacity-parameterized checker needs** if p12's "opacity lives in the editing layer, not at ingest" is to be sound rather than merely convenient. Also **Crary (POPL 2019) — "Fully Abstract Module Compilation"** `[verified]` and **Crary, Harper, Puri (PLDI 1999) — "What is a Recursive Module?"** `[verified]`.
- **Derek Dreyer** — all verified via his research page:
  - **(2005) — "Understanding and Evolving the ML Module System."** PhD thesis, CMU-CS-05-131.
  - **(ICFP 2007) — "A Type System for Recursive Modules."**
  - **Dreyer & Rossberg (ICFP 2008) — "Mixin' up the ML Module System"**; *TOPLAS* 35(1), art. 2, 2013.
  - **Neis, Dreyer, Rossberg (ICFP 2009) — "Non-Parametric Parametricity"**; *JFP* 21(4–5):497–562, 2011. **The sleeper in this list:** what abstraction guarantees survive when a language has *dynamic type generation* — which is what arbor's runtime mint is.
  - **Dreyer, Harper, Chakravarty (POPL 2007) — "Modular Type Classes."**
- **Dan Grossman, Greg Morrisett, Steve Zdancewic (TOPLAS 2000) — "Syntactic Type Abstraction."** 22(6):1037–1080. DOI [10.1145/371880.371887](https://doi.org/10.1145/371880.371887). `[verified]` Also **Zdancewic, Grossman, Morrisett (ICFP 1999) — "Principals in Programming Languages,"** pp. 197–207, DOI [10.1145/317636.317799](https://doi.org/10.1145/317636.317799) `[verified]`. Abstraction as a *syntactic*, principal-relative property enforced by term-level coercions rather than a semantic parametricity argument. **Very close to p12's actual design**: `Seal` nodes are the coercions and the open set is the principal.
- **Donna Malayeri & Jonathan Aldrich (ECOOP 2008) — "Integrating Nominal and Structural Subtyping."** LNCS 5142, pp. 260–284. DOI [10.1007/978-3-540-70592-5_12](https://doi.org/10.1007/978-3-540-70592-5_12). `[verified]` The nominal/structural axis is arbor's central design tension: `Opaque{mint, witness}` is nominal, `Record`/`Sig` canonical-by-sorted-label-hash is structural, and `Ascribe` deliberately has no mint. This is the paper arguing a language can have both coherently.

## Records and labels

- **Luca Cardelli & John C. Mitchell (1991) — "Operations on Records."** *Mathematical Structures in Computer Science* 1(1):3–48. DOI [10.1017/S0960129500000049](https://doi.org/10.1017/S0960129500000049). `[verified]`
- **Daan Leijen (TFP 2005) — "Extensible Records with Scoped Labels."** [PDF](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/scopedlabels.pdf). `[verified — no DOI]` Directly relevant to p16's decision that field identity *is* the label hash and names never appear in stored bytes: Leijen allows **duplicate** labels with scoping, which is the opposite trade. Worth reading against `../design/11-label-sort.md`.
- **Rémy's and Gaster & Jones' record-calculus papers.** `[UNVERIFIED — check before citing]`

## Hashing abstract types across a distributed codebase — the Sewell cluster

**This is the most important find in the survey.** It is a well-developed prior literature for p12's central move that appears nowhere in arbor's docs.

- **Peter Sewell (POPL 2001) — "Modules, Abstract Types, and Distributed Versioning."** pp. 236–247; also Cambridge Computer Laboratory TR 506. [PDF](https://www.cl.cam.ac.uk/~pes20/versions-popl.pdf), [TR](https://www.cl.cam.ac.uk/~pes20/versions-tr.pdf). `[verified]` **[Priority read — second.]**
- **James J. Leifer, Gilles Peskine, Peter Sewell, Keith Wansbrough (ICFP 2003) — "Global Abstraction-Safe Marshalling with Hash Types."** DOI [10.1145/944705.944714](https://doi.org/10.1145/944705.944714). `[verified]` **[Priority read — first. Shortest and most on-point.]**
- **Sewell, Leifer, Wansbrough, Zappa Nardelli, Allen-Williams, Habouzit, Vafeiadis (ICFP 2005) — "Acute: High-Level Programming Language Design for Distributed Computation."** Journal version *JFP* 17(4–5):547–612, 2007. [Project page](https://www.cl.cam.ac.uk/~pes20/acute/). `[verified]`
- **John Billings, Peter Sewell, Mark Shinwell, Rok Strniša (ML 2006) — "Type-Safe Distributed Programming for OCaml"** (HashCaml). `[verified — via Sewell's publication list]`

**Why this cluster matters.** Acute generates type names for abstract types "freshly *and by hashing*, to ensure that type equality tests suffice to protect the invariants of abstract types across the entire distributed system." That is `Opaque{mint, witness}` and `Abstract(mint)` — including the soundness argument for why the witness must be in the hash — twenty years earlier, motivated by marshalling across a network rather than by a content-addressed store. The POPL 2001 paper is explicitly about *versioning* abstract types across independently-compiled programs, which is arbor's no-silent-breakage property in a different setting.

p12–p17 independently rediscovered a design that already has a worked-out formal treatment. That is good news for soundness and bad news for novelty claims; either way arbor must engage it. Filed in `../design/open-questions.md`.

## Where it plugs in

- `../design/12-type-abstraction.md:59` — Lillibridge & Harper and Leroy, with the author-order correction. §"Unbundled abstract types and the opacity trilemma" is where Grossman/Morrisett/Zdancewic and Crary belong.
- `../design/open-questions.md:98` and `:301` of `12-type-abstraction.md` — Montagu–Rémy now has a citation and, more usefully, a vocabulary (F-zip; open vs. closed forms).
- `../design/10-minted-identity.md` — MacQueen's type stamps are the ancestor of mints; Neis/Dreyer/Rossberg is what parametricity means under dynamic type generation.
- `../design/11-label-sort.md` — Cardelli & Mitchell, and Leijen's scoped labels as the opposite trade to p16's label-hash identity.
- `docs/prototypes/p12-abstract-types/00-scope.md` through `p17-translucent-modules/00-scope.md` — the whole line. p17's rank rule is a `Sig`-specific choice with no direct precedent found; F-ing Modules is the closest frame for judging it.
- `formalism/paper/arbor-stlc.tex` — the natural next rung past STLC is this material; F-ing Modules is the target language.

## Gaps and negative findings

- **The Sewell cluster is unacknowledged prior art.** See above; filed as an open question.
- **p17's rank rule** — binding opaques by label-hash sort order so component order carries no information — has **no precedent found** in this literature. Sigs in the ML tradition are order-sensitive telescopes. This may be genuinely novel or may just be under-searched; treat as open.
- **Sig-vs-Record unification** (flagged in p17's scope doc) has no obvious answer here either. Cardelli & Mitchell and Leijen are about records; F-ing Modules elaborates sigs *to* records, which is suggestive but not the same question.
- Six items are `[UNVERIFIED]`: Reynolds 1983, the Lillibridge & Harper DOI, Harper & Pierce's ATTaPL chapter, Frisch & Garrigue, the Eisenberg et al. author list, and Rémy/Gaster–Jones on records.
