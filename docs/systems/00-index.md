# Systems — Index

**Status:** Index. A source-level read of another system is a different genre of evidence from a citation; the conventions below say how it is recorded. Annotations are notes and observations, not substrate policy — nothing here is a commitment unless `../design/decisions.md` says so.

## Purpose

`../related-work/` indexes the *literature* arbor sits next to. This directory reads the *implementations*.

The distinction is not pedantic. A paper is a fixed artifact with a DOI: it can be cited, and the citation stays true. An implementation is a moving target with a commit history: it can only be cited *as of a commit*, its documentation routinely describes a design that no longer ships, and — most usefully — it records not just what the designers chose but **what they later removed**. That last category is invisible in a bibliography and is often the most informative thing available.

The directory exists because of a specific gap. `../related-work/00-index.md` §"Negative findings" records that several of the systems arbor is closest to have **no publication at all** — Unison, Kythe, Glean, Glamorous Toolkit, Perkeep, Automerge. For those, the source *is* the citable artifact, and reading it is the only way to replace second-hand claims with evidence.

## Relation to `../related-work/` and `../design/07-hazel-substrate.md`

This directory adopts, unchanged, the contract stated at `../design/07-hazel-substrate.md:12` and re-adopted at `../related-work/00-index.md:15`: **items here are inputs.** When a finding turns into a design commitment, the commitment lands in `../design/decisions.md` and the detail lands in the relevant `../design/0N-*.md`, with a back-reference here. Reading a system is not adopting it.

Where the same system appears in both places, they do different jobs. `../related-work/01-content-addressing.md` §"Unison" says what Unison *is* in the bibliography and why it cannot carry a citation; `unison/` says what the code actually does. The theme file points here rather than duplicating.

## Evidence conventions

Publication tags (`[verified]`, `[UNVERIFIED]`, …) do not apply to source. These do:

- **`[source-verified — <path>:<line> @ <sha7>]`** — read in the tree at that commit. Paths are relative to the system's repository root, never absolute.
- **`[verified by absence — <the exact search> @ <sha7>]`** — for claims of the form "there is no X." These are frequently the *interesting* claims and cannot be cited positively, so the search that produced them is recorded verbatim and can be re-run.
- **`[historical — removed in <sha7>, <date>]`** — for mechanisms that existed and were deleted. Cite the commit, not the current tree.
- **`[design doc — <path>, describes intent, not current behavior]`** — for a system's own in-repo prose. Many such documents predate the implementation by years and are archaeology, not specification. Say so every time.
- **`[unread]`** — explicitly out of scope for this pass. Recorded so that silence is never mistaken for coverage.

**Every read is pinned to a commit**, stated in its overview file and re-checkable with `git -C <repo> rev-parse HEAD`. When upstream moves, the read does not become wrong — it becomes *dated*, which is a different and much more manageable failure. Do not silently refresh a citation; add a dated note.

**Every read states what it did not read.** A source read has no page count to bound it, so its scope is whatever the reader chose. Unbounded claims about a 160k-line codebase are not credible; bounded ones are.

**Every read keeps a followups list.** Reading source raises more questions than it answers, and the questions are the point. Each system directory carries a running list — tagged by whether more reading would settle it (`[read]`), whether it is a design question for arbor (`[arbor]`), or whether it is a claim made here on thin evidence (`[verify]`). Entries are added and closed, never silently dropped.

**Where a system's own rationale is absent, say so every time.** Implementations record decisions but rarely their reasons; commit messages are terse and design docs go stale. An argument reconstructed from what a system did is legitimate and often the only thing available, but it must be labeled as inference and its weakest points named. See `unison/09-side-structure.md` for the pattern.

## Systems read

- **`unison/`** *(read 2026-08-07, pinned to `db60ce2`)* — the system arbor most resembles and the one with no publication. Eleven files: how the codebase is organized, the store and hashing model, the namespace and history model, caching/update/merge, **what Unison deleted** and **why**, merge and branching, PL notes outside arbor's current lines, the mapping onto arbor's formal objects, and a running followups list. Start at `unison/00-overview.md`; the payload is `unison/07-arbor-mapping.md`; open threads are `unison/10-followups.md`.

## Non-goals

- **Not a tutorial or a port.** Nothing here is written to help someone use the system, and no finding implies arbor should adopt the mechanism.
- **Not comprehensive.** Each read is bounded and says where its boundary is.
- **Not a substitute for the literature index.** Where a system's technique has a published treatment, the citation belongs in `../related-work/`, and the source read should point at it.
