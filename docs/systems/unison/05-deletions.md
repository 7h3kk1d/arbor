# Unison — What it deleted

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope. Everything is `[source-verified @ db60ce2]` or `[historical]` as tagged.

## Why this file exists

A bibliography records what a system's designers chose. A repository also records what they **un-chose**, and that is not available any other way.

Three large deletions, over twenty months, remove roughly 5,950 lines between them:

| Commit | Date | What went | Lines |
|---|---|---|---:|
| `b7d43cf4` | 2024-01-03 | Metadata links (`link`/`unlink`, all metadata commands) | −2,099 |
| `8ed9ab41` | 2025-05-10 | `Patch`/`propagate`/`update.old` | −2,068 |
| `c90ec8aa` | 2025-09-18 | The SQL name-lookup index | −1,785 |

They look unrelated. They are the same move three times: **a reified side-structure was replaced by derivation-on-demand or by convention.**

arbor is currently building all three categories — aspects (`../../design/02-definitions-and-derived-data.md`), mints and threads and binding history (`../../design/10-minted-identity.md`, p11), and derived indices (`Store.callers_of`, the `follow-clean:v1` cached dry run). Whether that is convergent evolution or a warning is the most consequential open question this read produces. It is not resolved here.

## Metadata — the one that should worry arbor most

### What it was

`docs/metadata.markdown` is short enough to be the entire design `[design doc — describes intent; the mechanism it proposes shipped and was later removed]`:

> The Unison codebase format needs to be able to store metadata about definitions it contains, such as: Author, copyright holder · Creation date · License · API docs · Boolean indicating whether a definition is a test, needed to support incremental test evaluation · Comments that annotate subpaths of the definition · …
>
> Some desired features: We probably won't know all the kinds of metadata in advance, so having it be extensible would be good. Metadata should probably be versioned.

> Metadata is just a "link", a lightweight reference to some other definition.
>
> We don't try to make `MetadataType` more strongly typed. It's just a string, its meaning determined by convention. For instance, the default CLI viewer can look for an "API docs" key, and use that in its display.

**Read that against `../../design/02-definitions-and-derived-data.md`.** Extensible categories of data associated with a definition; categories not knowable in advance; values that are themselves ordinary content-addressed definitions; category identity as a manually-managed tag rather than a closed type. That is arbor's aspect concept, proposed independently, five-ish years earlier.

### How it was built

Not as a side table — as part of the namespace, and therefore part of its hash.

```haskell
type Star a n = Star2 a n Value
type Value    = TermReference
```
`parser-typechecker/src/Unison/Codebase/Metadata.hs:13-24`, over `Star2` — a star-schema relation `(fact, d1, d2)` (`parser-typechecker/src/Unison/Util/Star2.hs:30-37`). So metadata attaches to a **(definition, name) pair**, not to a definition alone, and each metadata value is itself a term reference. The V2 form is `MdValues = Set MetadataValue` where `MetadataValue = Reference` (`codebase2/codebase/U/Codebase/Branch/Type.hs:30-32`), stored as `MetadataSetFormat' t h = Inline (Set (Reference' t h))` (`Sqlite/Branch/Full.hs:85-86`) — the constructor name `Inline` implies a by-reference variant was contemplated.

It was hashed: `Hashing/V2/Branch.hs:23-24, 35-36` includes metadata in the namespace tokens. Changing a definition's license changed the namespace hash.

### What happened

Commit `b7d43cf4`, 2024-01-03, "remove a bunch of metadata-related code" `[historical — removed in b7d43cf4, 2024-01-03]`. It deleted `HandleInput/MetadataUtils.hs`, the `link` and `unlink` commands, 78 lines of `InputPatterns.hs`, 150 of `OutputMessages.hs`, and the `link`, `ambiguous-metadata`, `isPropagated-exists`, `isTest-exists`, and `fix1356` transcripts wholesale.

Today: no UCM command attaches metadata `[verified by absence — rg '"link"|"unlink"' unison-cli/src/Unison/CommandLine/InputPatterns.hs, zero hits @ db60ce2]`. The types remain for format compatibility and are empty in practice. `causal_metadata` is annotated *"Currently unused"* in the schema (`sql/create.sql:112-119`). The `IsPropagated` builtin declaration survives as a fossil — still defined at `parser-typechecker/src/Unison/Builtin/Decls.hs:53-54, 274, 519, 692-697` — because it used to be attached *as metadata* to mark auto-propagated definitions. Its job is now done by synhash (`04-caching-update-merge.md` §"Merge").

### What replaced it: a naming convention

Documentation is found by appending a segment to a name.

```haskell
docSegment :: NameSegment
docSegment = NameSegment "doc"
```
`codebase2/core/Unison/NameSegment.hs:31-32`, used as `potentialDocNames = [name, name :> docSegment]` (`unison-share-api/src/Unison/Server/Backend.hs:849`). `foo`'s documentation is the term named `foo.doc`. Other reserved segments encode other conventions the same way — `lib`, `License`, `metadata`, `authors`, `copyrightHolders`, `guid`, `builtin`, `>` (`NameSegment.hs:31-80`).

So: the general, extensible, hashed, first-class mechanism was replaced by **strings in the namespace**.

The one aspect that survived as a real index is the one with a real query behind it: a term's type, via `find_type_index`/`find_type_mentions_index`, populated from `HashHandle.toReference` during save (`Queries.hs:2616-2621`). Note what distinguishes it — it exists to answer "which definitions have type T?", a question the namespace cannot answer by naming convention.

### The reading for arbor, stated carefully

The honest version is not "aspects are a mistake." It is narrower and more useful:

**A general aspect mechanism has to earn its generality against naming convention, and Unison's did not.** Almost every metadata category it was built for — docs, authors, license, copyright — is a *name-shaped* fact: there is exactly one per definition, humans want to read and edit it directly, and it wants to be renamed, moved, and merged alongside the definition. For those, `foo.doc` is not a degradation; it is strictly better, because the whole existing namespace machinery (aliasing, merge, history, suffix resolution) applies to it for free, and a hashed-in metadata set gets none of that.

The categories where the general mechanism does earn its keep are the ones that are *not* name-shaped: **derived, machine-produced, keyed by hash rather than by name, and queried in reverse.** In Unison that is exactly the two that survived — the evaluation/test cache (`04-caching-update-merge.md`) and the type index. Both are keyed by hash, neither is user-editable, and both answer questions naming cannot.

arbor's aspects are, so far, largely of the second kind: `Type_of`, `has-holes:v1`, `follow-clean:v1`, translation targets, the eval cache. `../../design/02-definitions-and-derived-data.md`'s asserted/derived split already names the distinction. What this finding suggests is that **the split is load-bearing rather than descriptive** — derived-and-hash-keyed is the case the mechanism exists for, and asserted-and-name-shaped may be better served by the namespace. Worth stating in that document rather than leaving both halves equally weighted. Filed for `../../design/open-questions.md`.

## Patches and propagate

### What it was

Reified edits, hash-addressed like everything else, hanging off namespaces:

```haskell
data Patch = Patch { _termEdits :: Relation Reference TermEdit
                   , _typeEdits :: Relation Reference TypeEdit }
data TermEdit = Replace Reference Typing | Deprecate
data Typing = Same | Subtype | Different
```
`parser-typechecker/src/Unison/Codebase/Patch.hs:9-13`, `TermEdit.hs:5-16`, with the propagation semantics documented at `TermEdit.hs:12-14`:

> Replacements with the `Same` type can be automatically propagated. Replacements with a `Subtype` can be automatically propagated but may result in dependents getting more general types, so requires re-inference. Replacements of a `Different` type need to be manually propagated by the programmer.

Patches were a first-class object type (`object_type_description` id 3), stored in the `object` table, part of the namespace hash (`Hashing/V2/Branch.hs:25, 38`), and had their own diff format. `Unison/Codebase/Editor/Propagate.hs` was 703 lines.

**Note the three-way `Typing` classification.** It is arbor's update-strategy question with a different vocabulary: same-type changes propagate silently, subtype changes propagate with re-inference, different-type changes need a human. `../../design/04-naming-layer.md` §"Update strategies" and p11's pin/follow/explicit occupy the same space, and arbor-stlc's *type-preserving migration is total* theorem is the formal statement of the first row.

### What happened

Commit `8ed9ab41`, 2025-05-10, "delete update.old and update.old.preview commands" `[historical — removed in 8ed9ab41, 2025-05-10]`. `Propagate.hs` deleted entire; `Update.hs` shrank by 639 lines; 94 lines of `InputPatterns.hs` and the `ability-term-conflicts-on-update` transcript went with them.

Today there is no `patch`, `replace`, `propagate`, or `edit.resolve` command `[verified by absence — rg over InputPatterns.hs, zero hits @ db60ce2]`, and `unison-cli/src/Unison/Codebase/Editor/Input.hs` contains zero occurrences of "patch" `[verified by absence @ db60ce2]`. Patches are read-only legacy, reachable through old causal history, never created.

### What replaced it

`04-caching-update-merge.md` §"Update" and §"Merge", in one line: **from reified edits to recomputed diffs.** Compare namespaces at their LCA rather than storing what changed; query the dependents index rather than replaying substitutions; recover propagation-provenance by hashing modulo names rather than by marking it with metadata.

### Correction owed to arbor

`../../design/03-content-addressing.md:82` currently lists under "Do not adopt":

> **Automatic migration UX (`update`, `patch`, etc.).** When a definition changes, Unison helps users systematically move callers to the new hash. We have no equivalent.

The stance is fine. The description is out of date by more than a year: `patch` does not exist, and the mechanism `update` now uses is a different one with different properties. Corrected in place; see `07-arbor-mapping.md` §"Corrections owed".

The more interesting point for arbor is that **Unison tried the reified-edit design and abandoned it.** p11's binding history and `multi_rebind`, and arbor-core's `H`, are on the reified side of that line. They are not the same thing as a `Patch` — `H` records what bindings *were*, not a replayable edit program — but the distinction is worth being explicit about, because `Patch` is what `H` becomes if it ever grows the ability to be applied.

## The name index

### What it was

Denormalized name lookup in SQL: `scoped_term_name_lookup` and `scoped_type_name_lookup`, storing per (root branch hash, name) a `reversed_name`, a `last_name_segment`, a `namespace`, and a flattened referent (`sql/create.sql:236-328`). The schema comments are a small tutorial in query-planner-driven design (`:243-281`) — names are stored reversed with trailing dots so that suffix queries become prefix `GLOB`s, the trailing dot makes `base.` match `base.List` but not `base1` without an `OR` that would defeat the planner, and the composite index exists because *"SQLite will only optimize for a single prefix-glob at once."* `name_lookup_mounts` (`sql/007`) even let one index be mounted inside another at a path, so a dependency's name index could be shared rather than copied.

### What happened

Commit `c90ec8aa`, 2025-09-18, "Delete names perspective things instead of deprecating it" `[historical — removed in c90ec8aa, 2025-09-18]`. Deleted `NameLookups.hs`, `NamedRef.hs`, `NamesPerspectives/{Operations,Queries}.hs` (882 lines of SQL alone), `Server/NameSearch/Sqlite.hs`, and `PrettyPrintEnvDecl/Sqlite.hs`.

The tables remain in `create.sql` but are vestigial: grepping the tree for `scoped_term_name_lookup` hits only migration files `[verified by absence — rg scoped_term_name_lookup --type hs @ db60ce2]`. Names are now derived in memory from the branch, via `Branch0._deepTerms`/`_deepTypes` — derived cache fields recomputed by the lens setters (`parser-typechecker/src/Unison/Codebase/Branch/Type.hs:94-98, 225-333`).

### The reading

This one is the least alarming and the most instructive. The index was not wrong; it was *redundant*, because the namespace is a Merkle tree and traversal can memoize on subtree hashes — `deepChildrenHelper` skips a subtree it has already seen by hash (`Branch/Type.hs:343-361`). Content addressing made the derived index cheap enough to recompute that maintaining it in SQL stopped paying.

**For arbor this cuts toward the design rather than against it.** `../../related-work/01-content-addressing.md` §"Gaps" notes that *"nobody has published the primary-store-vs-derived-index argument"*, and that Kythe, Glean, CodeQL, and SCIP are all derived indices over files while arbor sits on the unclaimed side. Unison is a data point on arbor's side, and a sharp one: a system that started with a persisted derived index over a content-addressed store **deleted the index and kept the store**, because hash-keyed memoization made recomputation competitive. That is a concrete instance of the general argument, and it is citable now.

The caution attached to it: what made recomputation viable is that a namespace is small and traversal is memoizable by hash. arbor's `Store.callers_of` is a *reverse* index, which is the case where recomputation is not cheap — you cannot find callers by walking down from a root. Unison keeps its dependents index in SQL for exactly that reason (`03-namespace-and-history.md` §"Primary state vs. derived indices"). So the finding is not "derived indices can be deleted"; it is "**forward-derivable indices can be deleted; reverse ones cannot**," and Unison's own choices split precisely along that line.

## Summary table

| Deleted | What replaced it | arbor's counterpart | Reading |
|---|---|---|---|
| Metadata links (`Star2`, hashed into the namespace) | Naming convention (`foo.doc`), plus a type index for the one reverse query | Aspects (`../../design/02`) | Generality must beat naming convention; derived-and-hash-keyed is where it does |
| `Patch` / `propagate` (reified, hash-addressed edits) | LCA diff + dependents index + re-typecheck + synhash | Binding history `H`, update strategies, mint threads | The reified-edit design was tried and abandoned; provenance can be recovered by hashing modulo names instead of by minting |
| SQL name-lookup index | In-memory derivation, memoized on subtree hashes | `Store.callers_of`, `follow-clean:v1` | Forward-derivable indices can go; reverse ones stay — Unison kept its dependents index |

## What was *not* deleted, which is equally informative

- **The content-addressed store itself**, and the frozen hashing format. Never revisited.
- **The evaluation cache**, unchanged and uninvalidated (`04-caching-update-merge.md`).
- **The dependents index** — the one reverse-derived index, still in SQL.
- **Causal history**, which grew rather than shrank (projects, branches, reflog, squash memoization, history comments).
- **Minting**, which is still the default for types and acquired *more* supporting machinery over time (`namespace_unique_type_guid` arrived in `sql/016`, after the metadata deletion).

The through-line is that the primitives survived and the *bookkeeping layers built on top of them* did not.

**For *why*, and for the design space this sits in, see `09-side-structure.md`** — the six mechanisms available for associating data with a definition, each of `docs/metadata.markdown`'s motivating categories traced to where it actually ended up, and the four forces argued from evidence. Two findings from that file are worth carrying back here: what survived is keyed by **hash** and written by **the system**, while what died was keyed by **name** and written by **the author**; and Unison did not migrate authorship and licensing at all — it dropped them.
