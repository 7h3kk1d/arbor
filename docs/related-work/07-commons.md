# The Computational Commons and the Social Dimension

**Status:** Index. Sourced 2026-08-03. Annotations are notes and observations, not substrate policy — see `00-index.md` §"How to read this".

## Why this theme

`../design/00-overview.md:10` names the north star: "the *computational commons* sketched in Hazel's propl24 paper." That reference appears four times across the repo as the bare token `propl24`, with no authors, title, year, or venue anywhere. This file supplies the citation — Bandukwala, Blinn & Omar, PROPL 2024 — and adds the **second, later** commons paper the repo does not mention at all: Omar, Coblenz & Madhavapeddy, PROPL 2025, which is where the FAIR framing and the Fairground design live. Two papers, both relevant, cited separately below.

**The lineage arbor cannot inherit.** The PROPL 2025 paper's bibliography runs to 53 items and contains **no Ostrom, no Nelson, no Engelbart, no Unison, and no content-addressing citation of any kind.** Its framing is the FAIR principles (findable, accessible, interoperable, reusable), and its stated inspirations are Wikipedia, GitHub, and Google Earth Engine. Identity in Fairground comes from DOIs, ORCIDs, and a version-control platform — extrinsic identifiers, in the vocabulary of `01-content-addressing.md`. **The commons argument as currently published has no content-addressed-substrate component.** That is precisely the gap arbor fills, and it should be claimed rather than assumed.

## The commons papers

- **Cyrus Omar, Michael Coblenz, Anil Madhavapeddy (PROPL 2025) — "A FAIR Case for a Live Computational Commons."** 2nd ACM SIGPLAN International Workshop on Programming for the Planet, Singapore, October 2025, pp. 8–13. DOI [10.1145/3759536.3763802](https://doi.org/10.1145/3759536.3763802); [PDF](https://hazel.org/papers/fairground-propl25.pdf). `[verified — first pages read directly]` **[Priority read.]** Proposes **Fairground**: "a computational commons designed as a collaborative notebook system where thousands of scientific artifacts are authored, collected, and maintained together in executable form in a manner that is FAIR, reproducible, and live by default." Unlike JupyterLab or Google Earth Engine, Fairground notebooks reference each other as libraries, "forming a single planetary-scale live program executed by a distributed scheduler." Also describes **Fair Python**, a purely functional immutable-dataflow subset, and an FFI.

  The FAIR framing is worth taking seriously as an evaluation rubric: findable / accessible / interoperable / reusable maps onto hash-addressing, the naming layer, and translation aspects almost line for line. If arbor wants a requirements checklist for `../design/07-hazel-substrate.md` §"Requirements checklist (populated as claims land)", this is the source to distill it from.
- **Alexander Bandukwala, Andrew Blinn, Cyrus Omar (PROPL 2024) — "Toward a Live, Rich, Composable, and Collaborative Planetary Compute Engine."** Programming for the Planet, co-located with POPL 2024, London, January 2024. [Video](https://watch.eeg.cl.cam.ac.uk/w/3nGExywoVm6XFRBA2zYxSL). `[verified — read directly]` This project's own position statement, and the direct ancestor of arbor's framing. The four adjectives — live, rich, composable, collaborative — are what arbor is building substrate for. Cite it as the position arbor operationalizes; it currently appears nowhere in `docs/` under its own title.

## Hypertext, augmentation, and the personal-computing lineage

- **Ted Nelson (ACM '65) — "Complex Information Processing: A File Structure for the Complex, the Changing and the Indeterminate."** 20th National Conference, August 1965. DOI [10.1145/800197.806036](https://doi.org/10.1145/800197.806036). `[verified]` Where "hypertext" is coined. **Caveat: "transclusion" is *not* in this paper** — it comes later, via Xanadu and *Literary Machines* (1981, edition details `[UNVERIFIED]`). Cite 1965 for the file-structure argument, not for transclusion.
- **Douglas C. Engelbart (1962) — "Augmenting Human Intellect: A Conceptual Framework."** Summary Report AFOSR-3223, SRI Project 3578, Stanford Research Institute, October 1962. [dougengelbart.org](https://dougengelbart.org/pubs/augment-3906.html). `[verified — technical report]`
- **Alan Kay & Adele Goldberg (1977) — "Personal Dynamic Media."** *Computer* 10(3):31–41. DOI [10.1109/C-M.1977.217672](https://doi.org/10.1109/C-M.1977.217672). `[verified]`
- **David A. Smith, Alan Kay, Andreas Raab, David P. Reed (C5 2003) — "Croquet — A Collaboration System Architecture."** pp. 2–9. DOI [10.1109/C5.2003.1222325](https://doi.org/10.1109/C5.2003.1222325). `[verified]`
- **Bret Victor** — "Learnable Programming" (essay, Sept 2012), "Inventing on Principle" (talk, 2012), "The Future of Programming" (talk + notes, 2013), "Magic Ink" (essay, 2005), plus Dynamicland documentation (Intro/FAQ 2024, progress reports 2019–2022, Zine 2017). [worrydream.com](https://worrydream.com/). `[not peer-reviewed — essays and talks]` All present and citable as what they are. Widely influential in this line and never peer-reviewed; do not dress them up.

## Literate and notebook computing

- **Donald E. Knuth (1984) — "Literate Programming."** *The Computer Journal* 27(2):97–111. DOI [10.1093/comjnl/27.2.97](https://doi.org/10.1093/comjnl/27.2.97). `[verified]`
- **Thomas Kluyver et al. (ELPUB 2016) — "Jupyter Notebooks — A Publishing Format for Reproducible Computational Workflows."** *Positioning and Power in Academic Publishing*, Göttingen, pp. 87–90, IOS Press. ISBN 978-1-61499-649-1. `[verified]`
- **João Felipe Pimentel, Leonardo Murta, Vanessa Braganholo, Juliana Freire (MSR 2019) — "A Large-Scale Study About Quality and Reproducibility of Jupyter Notebooks."** pp. 507–517. DOI [10.1109/MSR.2019.00077](https://doi.org/10.1109/MSR.2019.00077). Extended in *EMSE* 2021, DOI [10.1007/s10664-021-09961-9](https://doi.org/10.1007/s10664-021-09961-9). `[verified]` 1.4M notebooks from GitHub — the empirical reproducibility crisis the PROPL 2025 paper argues against.
- **Adam Rule, Aurélien Tabard, James D. Hollan (CHI 2018) — "Exploration and Explanation in Computational Notebooks."** DOI [10.1145/3173574.3173606](https://doi.org/10.1145/3173574.3173606). `[verified]`
- **Souti Chattopadhyay, Ishita Prasad, Austin Z. Henley, Anita Sarma, Titus Barik (CHI 2020) — "What's Wrong with Computational Notebooks? Pain Points, Needs, and Design Opportunities."** DOI [10.1145/3313831.3376729](https://doi.org/10.1145/3313831.3376729). `[verified]` Together with Rule et al. and Pimentel et al., these give the commons argument *empirical* rather than purely aspirational grounds — useful if arbor ever needs to motivate the vision to a skeptical reader.
- **Observable notebooks** and **Wolfram "computational essays."** `[UNVERIFIED by fetch — the Wolfram URL failed on a TLS certificate error; non-peer-reviewed in any case. Link-check before use.]`

## Commons as institutional economics — an editorial addition

- **Elinor Ostrom (1990) — *Governing the Commons: The Evolution of Institutions for Collective Action.*** Cambridge University Press. DOI [10.1017/CBO9780511807763](https://doi.org/10.1017/CBO9780511807763). `[book verified]` **Flagged honestly: this is an editorial addition, not lineage.** Ostrom does not appear in the PROPL 2025 bibliography, and the survey found no evidence that the PL/HCI computational-commons line draws on her institutional-economics work. If arbor uses her — governance of a shared definition store is a genuinely Ostromian problem — it should be flagged as arbor's own framing move rather than presented as inherited.

## Where it plugs in

- `../design/00-overview.md:10` — "Hazel's propl24 paper" now has a full citation.
- `../design/07-hazel-substrate.md:9`, `:16`, and §"Research lines worth pulling from" #7 — the same, plus a new `propl25` bullet. The placeholder "_TBD: further requirements extracted from propl24_" is what the FAIR rubric could fill.
- `../design/01-language-model.md` — Fair Python (a purely functional immutable-dataflow subset with an FFI) is a data point on what a commons-oriented language looks like when someone else designs it.
- `../design/03-content-addressing.md` — the negative finding that the published commons argument has no content-addressing component is the clearest statement of what arbor contributes to it.
- `MEMORY.md` / `project_agentic_version_control` — the commons framing and the agentic framing (`08-agentic-vc.md`) are two different audiences for the same substrate; they have not been reconciled anywhere in `docs/`.

## Gaps and negative findings

- **The published commons vision has no content-addressed-substrate component.** No Ostrom, Nelson, Engelbart, Unison, or content-addressing citation in 53 references. Identity there is extrinsic (DOIs, ORCIDs). This is arbor's contribution to make, not to inherit.
- **No NSF award is titled or abstracted around a "computational commons."** NSF CAREER 2238744, "CAREER: Live and Direct Programming Environments" (PI Omar, Michigan, 2023) is verified as to number and title, but its abstract text could not be read (the award page rendered empty); NSF 2422028 is verified and unrelated. **Cite the PROPL papers, not grant text.**
- **Ostrom is not part of this line's lineage.** Use her deliberately or not at all.
- **`propl24` had no citation in the repo** — four bare mentions of the token, no authors or venue. Now cited, and PROPL 2025 added alongside it as a distinct source.
- **Bret Victor, Dynamicland, Observable, and Wolfram are all non-peer-reviewed.** Influential, but they cannot carry a claim.
