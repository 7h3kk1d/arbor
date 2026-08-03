# Content Addressing and Merkle Structures

**Status:** Index. Sourced 2026-08-03. Annotations are notes and observations, not substrate policy — see `00-index.md` §"How to read this".

## Why this theme

This is arbor's foundation and the theme where prior art is densest, best-engineered, and — at arbor's granularity — least formalized. The recurring shape across every system below is the same two-layer split arbor calls Store and Attachment: an immutable content-addressed layer, plus a mutable naming layer above it. Nix has store paths and profiles; IPFS has CIDs and IPNS; Perkeep has blobs and signed claims; Software Heritage has SWHIDs and origins. arbor's contribution is not that split but its *granularity* (per-definition rather than per-file or per-package) and its *canonicalization* (α-equivalence, so `\x. x` and `\y. y` collide).

The one rigorous technical result for that canonicalization is Maziarz et al. below. The most important negative finding in the whole survey also lives here: **Unison, the system arbor is closest to, has no peer-reviewed publication at all.**

## Merkle structures and the store/name split

- **Ralph C. Merkle (CRYPTO '87) — "A Digital Signature Based on a Conventional Encryption Function."** LNCS 293, pp. 369–378, Springer 1988. DOI [10.1007/3-540-48184-2_32](https://doi.org/10.1007/3-540-48184-2_32). `[verified]` The hash-tree construction; the canonical citation for "a node's identity is the hash of its children's identities." Note the *tree* here is authentication machinery — cite Merkle for hash-linking and Benet for the DAG generalization arbor actually uses.
- **Juan Benet (2014) — "IPFS: Content Addressed, Versioned, P2P File System."** arXiv:[1407.3561](https://arxiv.org/abs/1407.3561). `[verified — preprint, no peer-reviewed venue]` Coins "generalized Merkle DAG," and puts a self-certifying mutable namespace (IPNS) *above* the content-addressed layer. That is `06-architecture.md`'s Store/Attachment decomposition at file granularity.
- **IPLD (InterPlanetary Linked Data)** — Protocol Labs specification suite. [ipld.io](https://ipld.io/), [github.com/ipld/ipld](https://github.com/ipld/ipld). `[not peer-reviewed — artifact]` The closest existing attempt to standardize *typed links between hash-addressed nodes*, which is what `Node.Ref` is. Worth a look if arbor ever needs an interchange format; the codec/CID/path-resolution split is the part to study.
- **Perkeep (formerly Camlistore)** and **Upspin.** [perkeep.org/doc/compare](https://perkeep.org/doc/compare). `[not peer-reviewed — artifact]` A second independent instance of the split: content-addressed blobs plus GPG-signed *claims* as a mutable overlay. Perkeep's own comparison page is a usable secondary source on the design space.
- **Wen Xia et al. (USENIX ATC 2016) — "FastCDC: A Fast and Efficient Content-Defined Chunking Approach for Data Deduplication."** [usenix.org](https://www.usenix.org/conference/atc16/technical-sessions/presentation/xia). `[verified]` Content-defined chunking is the *sub-object* half of content addressing. Marginal for arbor — our chunk boundaries are given by the AST, not discovered by a rolling hash — but the right citation for why AST-shaped chunking is a better deal for code than rolling-hash chunking.
- **Git's object model.** **Honest gap:** no good academic treatment of the object model *as such* was found. The usable citations are **Bird, Rigby, Barr, Hamilton, German, Devanbu (MSR 2009), "The Promises and Perils of Mining Git"** `[verified — secondary]`, which describes Git as "a content addressable distributed database" but is a mining-methodology paper, and **Chacon & Straub, *Pro Git*, 2nd ed., Apress 2014, ch. 10 "Git Internals"** for a reference-quality description. A lead about a "Bill Pippin" academic treatment turned up nothing — `[UNVERIFIED; probably does not exist]`.

## Deployment and package granularity — Nix and Guix

- **Eelco Dolstra, Merijn de Jonge, Eelco Visser (LISA 2004) — "Nix: A Safe and Policy-Free System for Software Deployment."** pp. 79–92. [PDF](https://edolstra.github.io/pubs/nspfssd-lisa2004-final.pdf). `[verified]`
- **Eelco Dolstra (2006) — "The Purely Functional Software Deployment Model."** PhD thesis, Utrecht University. [PDF](https://edolstra.github.io/pubs/phd-thesis.pdf). `[verified]` **[Priority read — the *intensional store* chapter specifically.]** Store paths named by the hash of the complete dependency closure; profiles (mutable symlink trees) as the naming layer. The thesis distinguishes *input-addressed* from *intensional* (truly content-addressed) stores — the latter is the design arbor implements, and the one Nix took roughly fifteen years to ship. The strongest available evidence that hash-keyed immutability plus a separate name layer scales to a real ecosystem.
- **Eelco Dolstra & Andres Löh (ICFP 2008) — "NixOS: A Purely Functional Linux Distribution."** DOI [10.1145/1411204.1411255](https://doi.org/10.1145/1411204.1411255). `[verified]`
- **Ludovic Courtès (European Lisp Symposium 2013) — "Functional Package Management with Guix."** arXiv:1305.4584; [HAL hal-00824004](https://inria.hal.science/hal-00824004/en). `[verified]` Nix's model with a Scheme EDSL instead of a bespoke language. Useful precisely because it shows the model is language-agnostic and survives embedding in a general-purpose language.

## Archival scale — Software Heritage

The only deployed content-addressed code archive at planetary scale (16B+ unique source files as a Merkle DAG). This cluster is arbor's reality check, and its identifier vocabulary is the cleanest available for arbor's core claim.

- **Roberto Di Cosmo & Stefano Zacchiroli (iPRES 2017) — "Software Heritage: Why and How to Preserve Software Source Code."** [HAL hal-01590958](https://hal.science/hal-01590958v1). `[verified]` The founding paper.
- **Di Cosmo, Morane Gruenpeter, Zacchiroli (iPRES 2018) — "Identifiers for Digital Objects: the Case of Software Source Code Preservation."** [HAL hal-01865790](https://hal.science/hal-01865790). `[verified]` **[Priority read.]** SWHIDs are *intrinsic* identifiers forming a Merkle DAG, so identity, integrity, and deduplication are one mechanism. The **intrinsic vs. extrinsic** distinction — a hash you can recompute versus a DOI someone assigns — is the vocabulary arbor should adopt.
- **Di Cosmo, Gruenpeter, Zacchiroli (2020) — "Referencing Source Code Artifacts: a Separate Concern in Software Citation."** *Computing in Science & Engineering* 22(2):33–43. arXiv:2001.08647. `[verified]` Explicitly about *why references and names are a separate concern from identity* — the paper closest in spirit to `04-naming-layer.md`, arrived at from scholarly citation rather than PL.
- **Jean-François Abramatic, Di Cosmo, Zacchiroli (2018) — "Building the Universal Archive of Source Code."** *CACM* 61(10):29–31. DOI [10.1145/3183558](https://doi.org/10.1145/3183558). `[verified]`
- **Antoine Pietri, Diomidis Spinellis, Zacchiroli (MSR 2019) — "The Software Heritage Graph Dataset: Public Software Development Under One Roof."** DOI [10.1109/MSR.2019.00030](https://doi.org/10.1109/MSR.2019.00030). `[UNVERIFIED — DOI resolves but author list and pages were not confirmed against the primary record; check before citing]`
- **Spec: SWHID.** [docs.softwareheritage.org](https://docs.softwareheritage.org/devel/swh-model/persistent-identifiers.html). `[not peer-reviewed — artifact]`

**The distinction to state explicitly when citing this cluster:** Software Heritage content-addresses *artifacts as they were written* — files, directories, commits, i.e. Git's object model generalized. arbor content-addresses *canonicalized abstract syntax*. Extrinsic-text-hash versus intrinsic-semantic-hash is a real gap in the literature and one of arbor's clearer openings.

## Hashing modulo an equivalence — the core primitive

- **Krzysztof Maziarz, Tom Ellis, Alan Lawrence, Andrew Fitzgibbon, Simon Peyton Jones (PLDI 2021) — "Hashing Modulo Alpha-Equivalence."** DOI [10.1145/3453483.3454088](https://doi.org/10.1145/3453483.3454088); arXiv:[2105.02856](https://arxiv.org/abs/2105.02856). `[verified]` **[Priority read.]** The one rigorous result for the primitive arbor depends on: hashing binding structure so α-equivalent terms collide, in O(n log²n) rather than O(n²), via a weak commutative hash combiner used at exactly one point, with a proved collision bound. **This is the citation for arbor's α-equivalence-by-canonicalization** — not de Bruijn (who gives the representation, not the hashing result) and not Unison (which has no publication).
- **Jean-Christophe Filliâtre & Sylvain Conchon (ML 2006) — "Type-Safe Modular Hash-Consing."** DOI [10.1145/1159876.1159880](https://doi.org/10.1145/1159876.1159880). `[verified]` Maximal sharing behind an abstract type, *parameterized by an arbitrary equivalence*. That last part is the direct precedent for canonicalization: hash-consing modulo α-equivalence is hash-consing modulo a user-supplied relation.
- **M.G.J. van den Brand, H.A. de Jong, P. Klint, P.A. Olivier (2000) — "Efficient Annotated Terms."** *Software: Practice and Experience* 30(3):259–291. `[verified]` ATerms: maximal subterm sharing, automatic GC, and a compact binary exchange format so shared terms move *between processes*. The nearest ancestor of `CLAUDE.md`'s "typed values at substrate APIs, serialization internal to the layer that needs it."
- **Hash-consing provenance.** A. P. Ershov, "On programming of arithmetic operations," *CACM* 1(8), 1958 (technique originates 1957); Eiichi Goto, "Monocopy and Associative Algorithms in Extended Lisp," TR-74-03, University of Tokyo, 1974. `[verified — secondary; confirmed via ACL2 documentation and survey material, not primary records]` Cite for provenance only.
- **Bowen Zhu (2025) — "Efficient Symbolic Computation via Hash Consing."** arXiv:2509.20534. `[verified as preprint; venue UNVERIFIED]` The recent end of the hash-consing line.

## Unison

- **Unison — Paul Chiusano, Rúnar Bjarnason, Unison Computing.** [The big idea](https://www.unison-lang.org/docs/the-big-idea/), [annotated bibliography](https://www.unison-lang.org/docs/usage-topics/bibliography/), [github.com/unisonweb/unison](https://github.com/unisonweb/unison). `[no publication exists]` **[Priority read — the docs, and the finding.]**

  **No peer-reviewed publication exists.** DBLP returns nothing for Unison plus content-addressing. The citable artifacts are documentation, blog posts (Chiusano, ["A new project: Unison"](https://pchiusano.github.io/2014-09-14/unison.html), 2014), a Strange Loop 2019 talk, and one piece of trade press (["Programming in Unison," LWN, 2024](https://lwn.net/Articles/978955/)).

  Unison's design claims — hashes as identity, names as metadata, never-invalidated caches, no diamond-dependency problem — have **never been formally stated, let alone proven**. `formalism/paper/arbor-core.tex`'s no-silent-breakage and eval-stability theorems appear to be the first formal treatment of this model. That is arbor's single largest citation opportunity and the strongest argument for the `formalism/` line existing at all.

  Unison's own annotated bibliography cites only *other people's* work as influence, which is worth knowing: **Dunfield & Krishnaswami (ICFP 2013), "Complete and Easy Bidirectional Typechecking for Higher-Rank Polymorphism,"** arXiv:1306.6032 `[verified]`, and **Sam Lindley, Conor McBride, Craig McLaughlin (POPL 2017), "Do be do be do,"** pp. 500–514, DOI [10.1145/3009837.3009897](https://doi.org/10.1145/3009837.3009897) `[verified]` — the Frank effect system behind Unison's abilities.

## Where it plugs in

- `../design/03-content-addressing.md` — the whole theme; in particular §"Unison as prior art — what we adopt and what we don't", which currently cites no source at all. Maziarz et al. belongs against the α-equivalence claim; the intrinsic/extrinsic vocabulary belongs in the framing.
- `../design/04-naming-layer.md` — Nix profiles, IPNS, Perkeep claims, and Di Cosmo et al. 2020 are four independent instances of "names are a separate concern from identity."
- `../design/06-architecture.md` — the Store/Attachment split is the recurring two-layer shape above; ATerms is the precedent for the typed-boundary/internal-serialization rule.
- `../design/02-definitions-and-derived-data.md` — Nix's input-addressed vs. intensional distinction is the same axis as procedure identity: what exactly goes into the key.
- `formalism/paper/arbor-core.tex` — Maziarz et al. and Dolstra's thesis are the two works `arbor-core` is closest to and cites neither.
- `../design/decisions.md` — nothing here has hardened into a decision yet; the α-equivalence citation is the most likely first graduation.

## Gaps and negative findings

- **No peer-reviewed Unison publication.** See above. Also relevant to `../design/open-questions.md` and the `formalism/` line's positioning.
- **No academic treatment of Git's object model.** Use Bird et al. plus *Pro Git*; do not imply a paper exists.
- **No prior work found on canonicalized-AST hashing as a *program-store* primitive** beyond Maziarz et al., which treats it as an algorithm rather than a storage model. This appears to be genuinely open ground — but it is a negative finding from a budget-limited search, not a proof of absence.
- **Content-defined chunking is a red herring for arbor** and should be cited only to explain why AST-shaped boundaries are preferable.
