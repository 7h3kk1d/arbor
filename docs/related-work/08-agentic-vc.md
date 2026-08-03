# Agentic Version Control

**Status:** Index. Sourced 2026-08-03. Annotations are notes and observations, not substrate policy — see `00-index.md` §"How to read this".

## Why this theme

This is arbor's newest framing — agents as autonomous concurrent users; immutability preventing collisions; forking as a cheap copied view — and it is the theme where the survey most changes what arbor can claim. The literature is **mature where arbor is not, and thin where arbor is aiming.**

The three claims worth stating precisely:

- **(a) Immutability prevents collisions.** Partially refuted in its strong form. CodeCRDT reports 100% convergence with zero merge failures and **5–10% semantic conflicts remaining**. Structural non-collision does not buy semantic non-conflict. arbor should state (a) as "no destructive *overwrite*" and concede the semantic residue.
- **(b) Forking is a cheap copied view.** **Already engineered, and better.** Fork/Explore/Commit gives copy-on-write branch contexts with sub-350 µs creation independent of base filesystem size, atomic commits, no root. Cheap forking is not a novel claim in 2026. arbor's differentiation has to be *what* is forked (a typed definition DAG plus namespace) and which invariants survive the fork.
- **(c) Hash-keyed incremental type-and-test caching as the substrate for agent speculation.** **Unclaimed.** Nobody has built it. Merge-Readiness Packs *name* the need; SWE-bench rebuilds it per task from Docker images; Glite ARF approximates it with scripts. The composition — definition-level immutability, plus a separate mutable namespace, plus aspects derived and cached by hash, so a speculative edit's type-correctness and test results are already computed before any name is rebound — is arbor's opening.

Also: **"agentic version control" returns zero DBLP hits**, and neither major LLM-agents-for-SE survey has a version-control subcategory. The framing is academically unclaimed. That is an opportunity and a warning — the *design* discourse here is almost entirely vendor blog posts.

## Peer-reviewed: agent platforms and repository-level work

- **Carlos E. Jimenez, John Yang, Alexander Wettig, Shunyu Yao, Kexin Pei, Ofir Press, Karthik Narasimhan (ICLR 2024) — "SWE-bench: Can Language Models Resolve Real-World GitHub Issues?"** [OpenReview](https://openreview.net/pdf?id=VTF8yNQM66). `[verified]` The snapshot-plus-test-oracle loop is arbor's "materialize a speculative edit, consult its cached test aspect" — but reconstructed per task from Docker images rather than derived from content hashes.
- **John Yang, Carlos E. Jimenez, Alexander Wettig, Kilian Lieret, Shunyu Yao, Karthik Narasimhan, Ofir Press (NeurIPS 2024) — "SWE-agent: Agent-Computer Interfaces Enable Automated Software Engineering."** [Proceedings](https://proceedings.neurips.cc/paper_files/paper/2024/hash/5a7c947568c1b1328ccc5230172e1e7c-Abstract-Conference.html). `[verified]` The citable evidence that **the interface the agent sees is the dominant performance variable** — which matters because arbor proposes a radically different agent-facing interface: hash-addressed definitions plus namespace operations, rather than files plus a shell.
- **Xingyao Wang et al. (ICLR 2025) — "OpenHands: An Open Platform for AI Software Developers as Generalist Agents."** arXiv:2407.16741. `[verified]` Sandboxed execution plus explicit multi-agent coordination; the closest peer-reviewed reference platform for arbor's concurrent-agent setting.
- **Ramakrishna Bairi, Atharv Sonwane, Aditya Kanade, Vageesh D C, Arun Iyer, Suresh Parthasarathy, Sriram Rajamani, B. Ashok, Shashank Shet (FSE 2024) — "CodePlan: Repository-Level Coding using LLMs and Planning."** *Proc. ACM Softw. Eng.* 1(FSE), art. 31. DOI [10.1145/3643757](https://doi.org/10.1145/3643757). `[verified]` **[Priority read.]** Incremental dependency analysis plus change-may-impact analysis to sequence edits across 2–97 interdependent files. **A hand-built approximation of what `Store.callers_of` answers exactly.** The usable framing line: the planning problem CodePlan solves symbolically is a *query* in a content-addressed store.

## Peer-reviewed: merge conflicts and ML

This is a decade-deep, well-benchmarked program whose own ceiling numbers are arbor's strongest single motivation.

- **Alexey Svyatkovskiy, Sarah Fakhoury, Negar Ghorbani, Todd Mytkowicz, Elizabeth Dinella, Christian Bird, Jinu Jang, Neel Sundaresan, Shuvendu Lahiri (ESEC/FSE 2022) — "Program Merge Conflict Resolution via Neural Transformers"** (MergeBERT). DOI [10.1145/3540250.3549163](https://doi.org/10.1145/3540250.3549163); arXiv:2109.00084. `[verified]` The incumbent research program arbor implicitly declares unnecessary; cite it to show you know what you are displacing.
- **Elizabeth Dinella, Todd Mytkowicz, Alexey Svyatkovskiy, Christian Bird, Mayur Naik, Shuvendu Lahiri (IEEE TSE 2022) — "DeepMerge: Learning to Merge Programs."** DOI [10.1109/TSE.2022.3183955](https://doi.org/10.1109/TSE.2022.3183955); arXiv:2105.07569. `[verified]` **37% of non-trivial merges resolved correctly.** The clearest single number in arbor's favour.
- **Jialu Zhang, Todd Mytkowicz, Mike Kaufman, Ruzica Piskac, Shuvendu Lahiri (ISSTA 2022) — "Using Pre-Trained Language Models to Resolve Textual and Semantic Merge Conflicts (Experience Paper)."** DOI [10.1145/3533767.3534396](https://doi.org/10.1145/3533767.3534396). `[verified — from search metadata only]` Notable for tackling **semantic** conflicts — the residue content addressing does *not* eliminate.
- **Mori & Hashimoto (ASE 2025) — "On the Correctness of Software Merge."** pp. 2338–2349. DOI [10.1109/ASE63991.2025.00193](https://doi.org/10.1109/ASE63991.2025.00193); arXiv:2607.07987. `[verified — from the arXiv copyright header, not a primary fetch]` A formal treatment of what merge correctness means — the best formal-methods anchor if `formalism/` wants a no-destructive-collision theorem.
- See also `05-naming-versioning.md` §"Structured diff and merge" for the non-ML half, including Schesch & Ernst on *incorrect clean merges*, which any comparative claim has to reckon with.

## Peer-reviewed: surveys and empirical agent studies

- **Junwei Liu, Kaixin Wang, Yixuan Chen, Xin Peng, Zhenpeng Chen, Lingming Zhang, Yiling Lou (ACM TOSEM) — "Large Language Model-Based Agents for Software Engineering: A Survey."** DOI [10.1145/3796507](https://doi.org/10.1145/3796507); arXiv:2409.02977. `[verified]` 106 papers; use it to establish that **no surveyed subcategory is version control.**
- **Junda He, Christoph Treude, David Lo (ACM TOSEM, v4 July 2025) — "LLM-Based Multi-Agent Systems for Software Engineering: Literature Review, Vision and the Road Ahead."** arXiv:2404.04834. `[verified]`
- **Ahmed E. Hassan, Gustavo Oliva, Dayi Lin, Boyuan Chen (ACM TOSEM) — "Towards AI-Native Software Engineering (SE 3.0): A Vision and a Challenge Roadmap."** DOI [10.1145/3807901](https://doi.org/10.1145/3807901); arXiv:2410.06107. `[verified]`
- **Chen Qian et al. (ACL 2024) — "ChatDev: Communicative Agents for Software Development."** [ACL Anthology](https://aclanthology.org/2024.acl-long.810/). `[verified]` And **Sirui Hong et al. (ICLR 2024) — "MetaGPT: Meta Programming for a Multi-Agent Collaborative Framework,"** arXiv:2308.00352 `[venue verified only via secondary sources]`. Both are *sequential* pipelines, so neither actually exercises arbor's concurrency claims — worth noting when they get cited as multi-agent prior art.
- **Abujadallah, Arabat, Sayagh (MSR 2026) — "Understanding the Rejection of Fixes Generated by Agentic Pull Requests — Insights from the AIDev Dataset."** arXiv:[2606.13468](https://arxiv.org/abs/2606.13468). `[verified]` **46.41% of agent-proposed fixes are rejected**, across 14 failure reasons. Highly usable motivation: nearly half of agent output is waste that a substrate could evaluate before a name is rebound. Companion: **Rahman, Rabbi, Zibran (MSR Mining Challenge 2026) — "A Task-Level Evaluation of AI Agents in Open-Source Projects,"** arXiv:2602.02345 `[verified]`.
- **Casserini, Facchini, Ferrario (HCXAI @ CHI 2026) — "Beyond the 'Diff': Addressing Agentic Entropy in Agentic Software Development."** arXiv:[2604.16323](https://arxiv.org/abs/2604.16323). `[verified]` The most explicit peer-reviewed statement that **the diff is the wrong unit** for agent work — a natural ally for the separate-mutable-name-layer framing.

## The three preprints that matter

Read these as a set before writing another word of agentic positioning.

- **Wang & Zheng (2026) — "Fork, Explore, Commit: OS Primitives for Agentic Exploration."** arXiv:[2602.08199](https://arxiv.org/abs/2602.08199). `[verified as preprint]` **[Priority read.]** Introduces the **branch context**: copy-on-write state isolation with independent filesystem views and process groups. BranchFS (FUSE) achieves **sub-350 µs branch creation independent of base filesystem size**, with atomic commits and no root; plus a proposed `branch()` syscall with kernel-enforced sibling isolation and first-commit-wins coordination. **This is arbor's claim (b), already built, one layer down.**
- **Liu, Chen, Xu, Jiang, Dong (2026) — "Multi-agent Collaboration with State Management"** (STORM). arXiv:[2605.20563](https://arxiv.org/abs/2605.20563). `[verified as preprint]` **[Priority read — the most directly competitive paper in the survey.]** STORM mediates agents' interactions with a shared workspace so each agent has a consistent view and conflicting edits are detected **at write time**. It frames itself explicitly *against* git-worktree-per-agent, whose defect is that it "defers conflict resolution to a post-hoc merge step where recovery is expensive." Beats worktree baselines by **+18.7 on Commit0-Lite**. arbor and STORM answer the same question with different mechanisms — content-addressed immutability versus mediated write-time consistency — and arbor must cite and distinguish it. Filed in `../design/open-questions.md`.
- **Pugachev (2025) — "CodeCRDT: Observation-Driven Coordination for Multi-Agent LLM Code Generation."** arXiv:[2510.18893](https://arxiv.org/abs/2510.18893). `[verified as preprint]` **[Priority read.]** 600 trials: up to 21.1% speedup on some tasks, up to **39.4% slowdown** on others; **100% convergence, zero merge failures — but 5–10% semantic conflicts remain.** **The honest counterexample to claim (a).**

## Other preprints

- **Lindenbauer, Bogomolov, Zharov — "GitGoodBench: A Novel Benchmark For Evaluating Agentic Performance On Git."** arXiv:2505.22583. `[verified as preprint]` GPT-4o with custom tools achieves **21.11% success** — the empirical floor for "can agents even drive git."
- **Li, Zhang, Hassan — "AIDev: Studying AI Coding Agents on GitHub."** arXiv:2602.09185. `[verified as preprint]` **932,791 agent-authored PRs** across 116,211 repos; the dataset behind the MSR 2026 studies above.
- **Hassan et al. — "Agentic Software Engineering: Foundational Pillars and a Research Roadmap."** arXiv:2509.06216. `[verified as preprint]` Proposes Agent Command / Execution Environments and **Merge-Readiness Packs** — the nearest published concept to arbor's claim (c). Use for vocabulary to position against.
- **Philippov et al. — "Glite ARF: Verifier-Driven Research with Parallel LLM Coding Agents."** arXiv:2606.27416. `[verified as preprint]` **Twelve parallel agents, 146 runs, ~$450, no merge conflict ever reached main.** A mild counterweight to "git is inadequate" that arbor should engage rather than ignore.
- **Schesch & Ernst — "Merge-Bench."** arXiv:2605.25890. `[verified as preprint]` 7,938 real conflict hunks; **best models resolve under 60%.** The best single "textual merge remains unsolved in 2026" figure.
- **Khan — "Verified Detection and Prevention of Concurrency Anomalies in Multi-Agent Large Language Model Systems."** arXiv:2606.17182. `[verified as preprint]` Formalizes four anomalies as classical **isolation** violations, with 274 verified Rust implementations (Verus/TLA+). **Directly usable by `formalism/` as a vocabulary for stating claim (a) rigorously** — isolation levels are a better-understood frame than "collisions."
- **Rashidi — "The Balkanization of Execution-Security Research for AI Coding Agents."** arXiv:2607.05743. `[verified as preprint]` Systematizes 39 isolation/sandboxing papers.
- **Mazloomzadeh, Morovati, Khomh.** arXiv:2607.21832. `[verified as preprint]` Longitudinal AIDev analysis.
- **Campos Junior & Murta.** arXiv:2605.16646, LLM vs. search-based conflict resolution. `[verified — metadata only]` And **"SpecBox,"** arXiv:2607.23933 `[verified — metadata only; authors unconfirmed]`.

## Industry and engineering (non-peer-reviewed)

- **Agent Trace** — Cognition AI, announced January 2026, published as an RFC by Cursor. [contextgraph.tech/learn/agent-trace](https://www.contextgraph.tech/learn/agent-trace). `[not peer-reviewed — spec]` A vendor-neutral JSON spec linking code ranges to the conversations that produced them (attribution type, models used, conversation refs, line/file granularity); backed by Cursor, Cognition, Google Jules, Amp, Cloudflare, Vercel, OpenCode, git-ai. Explicitly framed as "complements git by capturing *why*, not just *who*." **arbor's aspect store is a strictly more general mechanism for the same need** — an asserted aspect keyed by definition hash, rather than a side file keyed by line range.
- **Freestyle (May 2026) — "Version Control for AI Agents."** [freestyle.sh](https://www.freestyle.sh/blog/engineering/version-control-for-ai-agents). `[not peer-reviewed — blog]` Important because it is the **opposite thesis**: git is not inadequate, it just needs to be infrastructure rather than a manual tool. No content-addressing or immutability innovations. Cite as the honest opposing position arbor has to beat.
- **Git-worktree-per-agent as the de facto pattern** — multiple 2026 writeups. `[not peer-reviewed — blogs]` Consistent claim: worktrees are the dominant isolation primitive (one working directory per agent over one shared `.git` object store), natively supported by Claude Code, Codex, and Cursor. Consistent caveat: worktrees give "only low-level workspace isolation; they do not solve task decomposition, dependency tracking, semantic conflicts, or merge selection." **That sentence is arbor's opening.**
- **Lower-value, noted for completeness.** [Agent-Git](https://github.com/MAS-Infra-Layer/Agent-Git) (versions *agent* state, not code; no evaluation); [re_gent](https://www.re-gent.dev/) (blame/log/rewind over agent activity). **Atomic** and **DeltaDB** are named in secondary coverage as ground-up agent-era VCS designs — DeltaDB reportedly gives "stable, addressable identity" per worktree operation, which sounds close to arbor's edit calculus. `[UNVERIFIED — worth ten minutes as possible prior art]`

## Where it plugs in

- `MEMORY.md` / `project_agentic_version_control` — this whole file is the reality check on that framing.
- `../design/04-naming-layer.md` §"Update strategies" — the pin/follow/explicit axis under concurrent agentic writers is where STORM's write-time mediation and arbor's immutability actually compete.
- `docs/prototypes/p11-mint-threads/00-scope.md` — `Store.callers_of` is what CodePlan reconstructs by analysis; `multi_rebind`'s atomicity is the primitive STORM provides by mediation instead.
- `../design/02-definitions-and-derived-data.md` — claim (c) lives here: type-check and test aspects, cached by hash, available before any rebinding. Agent Trace is the same shape as an asserted provenance aspect.
- `formalism/paper/arbor-core.tex` §"Metatheory" — Khan's isolation-violation vocabulary and Mori & Hashimoto's merge-correctness treatment are the two formal anchors for a no-destructive-collision claim.
- `../design/00-overview.md:39` — branching and merging are a stated non-goal. Agentic concurrency arrives before branching does, which is a tension worth naming.

## Gaps and negative findings

- **"Agentic version control" has zero DBLP hits** and no subcategory in either major SE-agent survey. Academically unclaimed; correspondingly, almost all design discourse is vendor blogging.
- **Claim (b) is retired as novel.** Cheap forking is a solved OS primitive with microsecond numbers.
- **Claim (a) needs restating** as "no destructive overwrite," conceding the 5–10% semantic residue CodeCRDT measures.
- **STORM is a direct competitor with benchmark wins and no content addressing.** Filed in `../design/open-questions.md`.
- **Claim (c) is genuinely unclaimed** — and secondarily, arbor's *typed-substrate* framing has no peer-reviewed competitor at all, because Unison was never published (see `01-content-addressing.md`).
- **Glite ARF is a counterweight worth engaging:** twelve parallel agents, no merge conflict reached main. "Git is inadequate" is not self-evident.
- **DeltaDB** may be genuine prior art for the edit calculus. Unverified.
