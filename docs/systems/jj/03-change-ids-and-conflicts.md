# Change IDs, evolution, and first-class conflicts

**Status:** Source read @ `253d3be`. See `00-overview.md` for the pin.

## Change ID vs commit ID

`docs/glossary.md` `[source-verified]`:

> **Change ID:** A unique identifier for a change. They are typically 16 bytes long and are often randomly generated.
>
> **Commit ID:** A unique identifier for a commit. They are 20 bytes long when using the Git backend.

The change ID tracks a commit **as it evolves through rewrites**; the commit ID identifies one specific version. Amend, rebase, or reword a commit and the commit ID changes while the change ID does not.

This is **arbor's mint thread** (`../../design/10-minted-identity.md`, p11), shipped and load-bearing in a system with real users. Two properties arbor's line does not have:

- **Change IDs are randomly generated, not derived.** No attempt is made to recover identity from content or from a name. Compare Unison, which *reconstructs* the `unique` GUID by name at save time and resolves ties by choosing *"an arbitrary one"* (`../unison/04-caching-update-merge.md` §"Minting"), and p11, which recovers it from an explicit *edit-of-X* gesture. jj's answer is the simplest of the three: mint once, carry it forward through every rewrite, never re-derive.
- **Divergence is a named, tolerated state.** `docs/glossary.md`: *"Divergent change: A change that has more than one visible commit. These changes are displayed with a label of 'divergent' in the log."* Not an error, not a resolution prompt — a label.

## The tax: a side table

`docs/technical/architecture.md` §GitBackend `[source-verified]`:

> Commit data that is available in Jujutsu's model but not in Git's model is stored in a `StackedTable` in `.jj/repo/store/extra/`. That is currently the change ID and the list of predecessors. For commits that don't have any data in that table, which is any commit created by `git`, we use an empty list as predecessors, and **the bit-reversed commit ID as change ID**.

Three things to take from this.

1. **Minting costs a side table, again.** Unison needed `namespace_unique_type_guid` to keep GUIDs stable across merges; jj needs `store/extra/` to keep change IDs at all. Two independent systems reached for the same structure for the same reason: *an identity axis orthogonal to content has nowhere to live inside a content-addressed object.* arbor's design puts the mint **inside** the hashed bytes (p10/p11, `../../design/10-minted-identity.md`), which avoids the side table and buys the opposite problem — the mint is then part of identity, so it cannot survive a content edit without an explicit gesture. The trade is now documented on both sides; it is a genuine fork, not an oversight.
2. **The fallback is a total function.** A Git-made commit with no entry gets *the bit-reversed commit ID* as its change ID. Rather than making change-ID lookup partial, jj defines it everywhere, degenerately. That is a design move arbor should consider wherever it is tempted to make an aspect partial.
3. **The hash is over the commit, and the change ID is not in it.** `architecture.md` notes the consequence plainly: *"Because we use the Git Object ID as commit ID, two commits that differ only in their change ID […] will get the same commit ID, so we error out when trying to write the second one of them."* An honest statement of a real defect caused by keeping the mint outside the hash — which is precisely the failure mode arbor's mint-in-the-hash choice prevents, at the cost noted above.

`lib/src/evolution.rs` (added 2025) exposes *"commit evolution history"* built over `commit_predecessors`, which the `Operation` message records (`simple_op_store.proto:120-127`) behind a `stores_commit_predecessors` flag. So the lineage of a change is reconstructed from **the operation log**, not from the commits themselves. `[skimmed — module header and types only]`

## First-class conflicts

`docs/technical/conflicts.md` `[source-verified]`. The stance:

> Most DVCSs require you to resolve those conflicts before you can finish the merge operation. Jujutsu instead **records the conflicts in the commit** and lets you resolve the conflict when you feel like it.

The representation is an **odd-length list of trees** hanging off the commit, read as an alternating sum:

> If the commit has trees A, B, C, D, and E it means that the contents should be calculated as A+(C-B)+(E-D).

Three properties worth naming, because arbor's residual could have all three:

- **Lazy.** *"The resulting tree contents is calculated on demand."* Merging descends into a subtree only when the tree IDs differ and the conflict is not trivially resolvable — content addressing is what makes the shortcut sound, since equal hashes mean equal subtrees.
- **Algebraic, with a simplification law.** `Merge::flatten()` and `Merge::simplify()` cancel terms. The documented example: a conflict `C+(B-A)` rebased onto `D` gives `D+((C+(B-A))-C)`, which simplifies to `D+(B-A)` — *"a regular 3-way merge between D and B with A as base (no trace of C)"*. Without this, repeated rebasing of an unresolved conflict would nest indefinitely.
- **Compositional with the rest of the model.** Because a conflict is a value in the commit, every operation that takes commits works on conflicted ones. Nothing needs a special case for "is currently broken."

**Relevance.** `formalism/paper/arbor-stlc.tex`'s **residual** — the set of names a migration broke — is currently a report. jj demonstrates the alternative: make it a *value in the store*, computed lazily, with laws that keep it from growing under repeated operations. `../../design/13-editing-context.md` proposes that the residual becomes the next frontier of an editing context; if it does, the simplification law is the thing that stops the frontier from accumulating stale history, and it is the part arbor would have to invent an analogue of.

Note the separation jj is careful about and arbor should copy: `conflicts.md` opens by saying it is about conflicts *between changes to files*, and points at `concurrency.md` for **conflicts between changes to bookmark targets**. Two conflict notions, two representations, two documents. arbor has the same split latent — conflicting *definitions* versus conflicting *names* — and has not distinguished them.

## Content hashing

`lib/src/content_hash.rs` `[source-verified]`. jj hashes with **BLAKE2b-512** (`blake2b_hash`), over a `ContentHash` trait with a derive macro and a stated encoding discipline: an enum hashes *"a 32-bit little-endian encoding of the ordinal number of the enum variant, then the variant's fields in lexical order."*

Same hash family as arbor (BLAKE2B via digestif) and the same discipline as arbor's tag-byte canonical encodings — a variant tag followed by fields in a fixed order. Recorded because it is a small independent confirmation that arbor's encoding convention is the ordinary one, and because "fields in *lexical* order" is a stronger canonicalization than arbor's positional one: it makes the encoding insensitive to field reordering in the source type, which is the same instinct as p17's rank rule (sorting by label hash so declaration order carries no information).
