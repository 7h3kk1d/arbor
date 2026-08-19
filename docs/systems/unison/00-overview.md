# Unison, read in source — Overview

**Status:** Source read. Conducted 2026-08-07 against `github.com/unisonweb/unison` at commit **`db60ce2faa4649f97746873f7d186a6d3ebd3bb8`** (`db60ce2`, 2026-08-07, HEAD of `trunk`). Evidence conventions in `../00-index.md` §"Evidence conventions". Findings are inputs, not policy — see `../00-index.md` §"Relation to …".

Re-check the pin with `git -C <unison-repo> rev-parse HEAD`. If it has moved, every citation below is still true *at `db60ce2`*; it is dated, not wrong.

## Why read the source

`CLAUDE.md:7` describes arbor as "inspired by Unison." `docs/` invokes Unison roughly twenty-five times across `design/` and `related-work/` — as prior art for content addressing, as the model for the naming layer, as the origin of the `unique`-type minting idea, as the system whose migration UX arbor declines. Before this read, **not one of those claims was sourced to anything but documentation, blog posts, or inference from rendered output.**

The reason is recorded at `../../related-work/00-index.md:102`:

> **No peer-reviewed publication exists for Unison.** Zero DBLP hits. Its design claims — hashes as identity, names as metadata, never-invalidated caches, no diamond-dependency problem — have never been formally stated, let alone proven.

That finding stands. Its consequence is what changes: when there is no paper, **the source is the citable artifact.** This read converts second-hand claims into evidence with paths and line numbers, and — as it turns out — corrects several of them.

A second reason emerged during the read and was not anticipated. Unison's git history contains three large deletions of machinery that arbor is currently building. That is not visible in any bibliography, and it is the most consequential thing here.

## Headline findings

Nine, roughly in descending order of how much they should change what arbor writes down.

1. **Unison has spent three years deleting its reified side-structure.** Metadata links (2024-01-03, `b7d43cf4`, −2099 lines), `Patch`/`propagate` (2025-05-10, `8ed9ab41`, −2068 lines), the SQL name-lookup index (2025-09-18, `c90ec8aa`, −1785 lines). Each was replaced by derivation-on-demand or by naming convention. arbor is currently *accumulating* the same three categories — aspects, mints and threads, and derived indices. `05-deletions.md`.

2. **The metadata mechanism Unison deleted is arbor's aspect concept, near enough to be startling.** `docs/metadata.markdown` proposes extensible per-definition metadata whose values are themselves content-addressed references and whose categories are strings interpreted by convention. That is `../../design/02-definitions-and-derived-data.md` in one page. It shipped, was part of the namespace hash, and is now unreachable. `05-deletions.md` §"Metadata".

3. **The store is not `Hash ⇀ Node`.** A hash names a whole strongly-connected *component*; a definition is `(Hash, Pos)` (`codebase2/core/U/Codebase/Reference.hs:114-119`). That is a different *type* of store from arbor-core's Σ, and it is the price of mutual recursion — a bigger commitment than the "canonical ordering" `../../design/03-content-addressing.md:141` describes. `02-store-and-hashing.md` §"Components".

4. **References are hash-transparent.** A singleton-component reference hashes to *literally* the referent's hash, so `x = 1 + 1` and `y = x` share a hash (`unison-hashing-v2/src/Unison/Hashing/V2/Term.hs:135-146`). arbor's formalism deliberately went the other way and reintroduced `Ref(h)` as a distinct node. Both are defensible; the fork is real and undocumented on arbor's side. `07-arbor-mapping.md` §"Ref".

5. **The evaluation cache has no invalidation logic.** Not "invalidation is rare" — there is none. `[verified by absence]`, with the search recorded. The only eviction paths are a debug-only wholesale `DELETE FROM watch_result` and a GC of orphaned hashes. This is arbor-core's `thm:stability`/`cor:cache` running in production, and the cache key is the hash of an expression that **need not be a named definition** — arbor's no-definition-sort stance, arrived at independently. `04-caching-update-merge.md` §"The cache".

6. **`update` is implemented by printing, re-parsing, and re-typechecking.** The code's own comment calls it *"the world's weirdest implementation of AST substitution"* (`unison-cli/src/Unison/Codebase/Editor/HandleInput/Update2.hs:424-427`). Its correctness rests on print-then-parse preserving hashes — which is exactly arbor-core's `prop:print-elab`. **arbor has already proved the lemma Unison's implementation needs and never states**, and Unison tests it as a golden regression corpus. `04-caching-update-merge.md` §"Update", `07-arbor-mapping.md` §"Migration".

7. **Unison is unique-by-default for types, and this is now source-verified rather than inferred.** `parser-typechecker/src/Unison/Syntax/DeclParser.hs:230` — a declaration with *no* modifier resolves to a minted GUID. This closes the gap recorded at `../../related-work/01-content-addressing.md` §"Gaps and negative findings" and answers `../../design/open-questions.md:77`. `04-caching-update-merge.md` §"Minting".

8. **Minting has a recurring cost, and Unison pays it with a side table and an arbitrary choice.** Because the GUID must survive re-saving a type, it is recovered *by name* from the namespace — and `parser-typechecker/src/Unison/Codebase/UniqueTypeGuidLookup.hs:20` says plainly: *"If there are multiple such types, an arbitrary one is chosen."* Plus a `namespace_unique_type_guid` table to keep GUIDs stable across merges. p11's mint threads have the same problem and no answer. `04-caching-update-merge.md` §"Minting".

9. **The type is inside the term hash** (`Hashing/V2/Term.hs:92-109`). In arbor-stlc the type is a derived aspect Θ *about* a hash, and typing is observational. In Unison, changing a signature changes the definition's identity. `07-arbor-mapping.md` §"Types".

## What was read, skimmed, and not read

The tree is ~161,000 lines of Haskell across 39 packages. Stating the boundary honestly matters more here than in a literature survey, because there is no page count to bound the claim.

**Read closely** — hashing (`unison-hashing-v2/`, ~1.3k lines) in full; the SQLite schema and its 22 migrations; `LocalIds`/localization; `Reference`/`Referent`; `Causal` and `Branch` at all three representations; the watch-cache read and write paths; `Update2.hs`'s algorithm and PPE construction; `Merge/Synhash.hs`; the `unique`-GUID path end to end; name suffixification in `unison-core/src/Unison/Name.hs`; every package's `package.yaml` dependency block.

**Skimmed at notes-depth** (per scope decision — see `06-pl-notes.md`) — the ability/effect system, the ANF→MCode runtime, pattern-match coverage, kind inference, doc literals, the transcript runner, LSP and MCP. Module headers and Unison's own design docs, not implementation bodies. Everything in `06-pl-notes.md` carries this caveat.

**`[unread]`** — the typechecker's implementation (`Typechecker/Context.hs`, 3786 lines) beyond its ability-row interface; the runtime interpreter loop and calling conventions beyond the design doc; the FFI/foreign-function surface (`Runtime/Foreign/Function.hs`, 3685 lines); the Share sync protocol and auth; the LSP implementation; pretty-printer internals; the ~700 test transcripts as a corpus (structure examined, contents not).

**Unison's own design docs are archaeology.** Several describe designs that never shipped or shipped and were removed. `docs/data-types.markdown` still proposes `nominal type` as hypothetical syntax and says structural is "the current default" — both false at `db60ce2`. Each citation to one of these carries `[design doc — describes intent, not current behavior]`, and the gap between intent and outcome is itself a finding.

## Document map

- **`01-organization.md`** — how the codebase is organized. The two parallel type universes, the frozen hashing package and the `HashHandle` seam, the package graph against arbor's four layers, and where the weight sits.
- **`02-store-and-hashing.md`** — the content-addressing model. Algorithm and token format, hash versioning, component hashing and its known failure mode, de Bruijn-at-hash-time, `Reference`, ref-transparency, type-in-the-hash, `LocalIds`.
- **`03-namespace-and-history.md`** — `Branch` as a Merkle tree with per-subtree history, names as a relation, `Referent` vs `Reference`, suffix resolution and the `lib`-depth rule, `Causal`, primary vs. derived tables.
- **`04-caching-update-merge.md`** — the watch cache and no-invalidation, `update`, `merge` and synhash, minting.
- **`05-deletions.md`** — what Unison removed and what replaced it. The most important file here for arbor.
- **`06-pl-notes.md`** — conceptually interesting, outside arbor's current lines. Notes-depth.
- **`07-arbor-mapping.md`** — the payload: the design space mapped onto arbor's formal objects, and the corrections this read forces.
- **`08-merge-and-branching.md`** — the merge algorithm step by step, how `update` selects dependents, and how update/merge/upgrade branches compose. Includes the mapping onto arbor's pin/follow/explicit strategies.
- **`09-side-structure.md`** — *why* the deletions happened. The six-mechanism design space for associating data with a definition, each of `metadata.markdown`'s motivating categories traced to where it ended up, and the forces argued from evidence. Read with `05`.
- **`10-followups.md`** — running list of threads this read opened and did not close. Add to it.
