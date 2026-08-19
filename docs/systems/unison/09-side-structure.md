# Side-structure — the design space, and why Unison retreated from it

**Status:** Source read @ `db60ce2`, plus git archaeology. See `00-overview.md` for the pin and scope. Extends `05-deletions.md`, which records *what* was deleted; this file is about *why*, and about the design space the question sits in.

**A standing caveat, stated once and meant.** Unison's commit messages for these removals are terse — *"remove a bunch of metadata-related code"*, *"remove `Metadata.Type`"*, *"remove metadata types from the in-memory branches"*. **No design rationale was written down anywhere in the tree.** Everything in §"Why" below is inference from what was removed, what replaced it, and what the mechanism's own design document asked for. It is argued, not quoted. Treat it as a reading of the evidence, not as Unison's stated position.

## The question

Every program store has to answer: **given a definition, how do you associate something else with it?** Its documentation, its author, its type, whether it is a test, its evaluated result, who calls it.

arbor's answer is *aspects* — `../../design/02-definitions-and-derived-data.md`. Unison's answer was *metadata links*. Unison's answer shipped, was part of the namespace hash, and was removed in January 2024. Understanding why is the most directly useful thing this whole source read produces.

## The design space

Six mechanisms are available, and Unison has used five of them. They are worth separating because they have genuinely different properties, and the deletion story is a *migration between them*, not an abandonment of the goal.

| # | Mechanism | Keyed by | Who writes it | Survives rename? | In a hash? |
|---|---|---|---|---|---|
| 1 | **In the definition's own hash** | — (it *is* identity) | author | n/a | yes, the definition's |
| 2 | **A side-map in the namespace** | (definition, name) | author | no | yes, the namespace's |
| 3 | **Naming convention** (`foo.doc`) | name | author | with the name | yes, the namespace's |
| 4 | **Derived index keyed by hash** | hash | the system | yes | no (rebuildable) |
| 5 | **A column on a hash-keyed table** | hash | the system | yes | no |
| 6 | **Recomputed from the value or type** | nothing — it's a query | nobody | yes | no |

Unison uses **1** (the type is in the term hash; the `unique` GUID is in the decl hash), used and deleted **2**, and now uses **3**, **4**, **5**, and **6**. arbor's aspects are a general mechanism spanning **2**, **4**, and **5** with one API.

## What Unison built, and what it looked like to a user

`docs/metadata.markdown` `[design doc — the mechanism it proposes shipped and was later removed]` is two pages. It names six motivating categories:

> Author, copyright holder · Creation date · License · API docs · Boolean indicating whether a definition is a test, needed to support incremental test evaluation · Comments that annotate subpaths of the definition

and two desiderata — extensibility (*"We probably won't know all the kinds of metadata in advance"*) and versioning. Then the mechanism, in full:

> Metadata is just a "link", a lightweight reference to some other definition.
>
> We don't try to make `MetadataType` more strongly typed. It's just a string, its meaning determined by convention.

It shipped as mechanism **2**: a `Star2` relation on the branch associating a **(definition, name)** pair with a set of metadata values, each value an ordinary content-addressed term reference, hashed into the namespace (`05-deletions.md` §"Metadata"). The user-facing surface was `link`/`unlink`, plus a config that attached default metadata on `add`.

Here is what it actually looked like, recovered from the transcript deleted in `b7d43cf4` (`git show b7d43cf4^:unison-src/transcripts/create-author.md`):

```
.foo> add
.foo> create.author alicecoder "Alice McGee"
.foo> view 2
.foo> link metadata.authors.alicecoder def1 def2
```

And here is the same transcript today (`unison-src/transcripts/idempotent/create-author.md`):

```
> create.author alicecoder "Alice McGee"

  Added definitions:
    1. metadata.authors.alicecoder          : Author
    2. metadata.copyrightHolders.alicecoder : CopyrightHolder
    3. metadata.authors.alicecoder.guid     : GUID

  Tip: Add License values for alicecoder under metadata.
```

**The `link` line is gone and nothing replaced it.** `create.author` still creates the definitions; there is no longer any way to attach them to anything. The authorship record is now three named definitions sitting under `metadata.*` with no edge to the code they describe.

That is the honest bottom of this story, and it is worth stating before the analysis: **Unison did not migrate this category. It dropped it,** and papered over the gap with a tip telling you to put things under a name.

## Where each motivating category ended up

The most informative thing available: trace `metadata.markdown`'s own six bullets to their fate.

| Motivating category | Where it went | Mechanism |
|---|---|---|
| **API docs** | `foo`'s documentation is the term named `foo.doc` (`codebase2/core/Unison/NameSegment.hs:31-32`; `potentialDocNames = [name, name :> docSegment]`, `unison-share-api/src/Unison/Server/Backend.hs:849`) | **3** — naming convention |
| **Is-this-a-test** | A **type query**. `handleTest` calls `findTermsOfTypes … (DD.testResultListType mempty)` — a test is any term of type `[Test.Result]` (`unison-cli/src/Unison/Codebase/Editor/HandleInput/Tests.hs:71`). Cached results are keyed by watch kind (`sql/create.sql:146-153`) | **6** + **4** + **5** |
| **Author, copyright holder** | **Nowhere.** Definitions under `metadata.*`, unattached | — |
| **License** | **Nowhere.** *"Tip: Add License values for alicecoder under metadata."* | — |
| **Creation date** | Never built | — |
| **Comments annotating subpaths** | Never built | — |

And the two categories that *were not* in the doc but exist today are the ones that thrive:

| Not in the doc | Mechanism |
|---|---|
| A term's **type**, queryable in reverse (`find_type_index`, populated at save) | **4** — derived index keyed by hash |
| A term's **evaluated result** (`watch_result`) | **5** — hash-keyed table |
| A term's **dependents** (`dependents_index`) | **4** |

**The pattern is legible.** Everything that survived is keyed by **hash** and written by **the system**. Everything that died was keyed by **name** and written by **the author**. Not one author-asserted, name-shaped category made it through in the general mechanism; two of them made it through as naming conventions and two were simply lost.

## Why — the forces, argued from the evidence

Four, in descending order of how well the evidence supports them.

**1. Metadata was already name-coupled, so it inherited the namespace's problems without the namespace's machinery.** The relation is `Star2 Referent NameSegment Value` — the association is to a **(definition, name)** pair, not to a definition. So metadata already had to be carried through rename, alias, move, merge, and diff. But the namespace *already implements* all of those for name→hash bindings. A parallel structure with the same key shape and none of the same tooling is pure duplicated cost, and every namespace operation had to grow a metadata case. The diff of `b7d43cf4` shows exactly this: `Output/BranchDiff.hs` lost 206 lines and `OutputMessages.hs` lost 150, almost all of it metadata cases in namespace-diff rendering.

**2. It was in the namespace hash, which makes an annotation a structural change.** `Hashing/V2/Branch.hs:23-24, 35-36` includes metadata in the namespace tokens. Changing a definition's license changes the namespace hash, which produces a causal entry, which shows up in `diff.namespace`, which participates in merge. An editorial act became a versioned structural change to the tree. Compare mechanism **3**: `foo.doc` also changes the namespace hash — but it changes it *by adding a definition*, which is a thing the whole system already knows how to diff, merge, and propagate.

**3. It was optional everywhere, so it was never depended on — and was a source of bugs.** Commit `9882d5445` / `77a690fa7`, "Don't completely crash mid-update on missing metadata" (2023), is the tell: metadata could be absent, code had to tolerate its absence, and therefore nothing could rely on its presence. A mechanism that must degrade gracefully to nothing is a mechanism whose consumers must have a fallback — and once you have the fallback, you can delete the mechanism. Note that the branch that removed it was named `24-01-04-delete-default-metadata`: the first thing to go was the config that *automatically* attached metadata on `add`, i.e. the feature that existed to make the store non-empty.

**4. The one category with a real query behind it got a real index instead.** "Is this a test" is not a fact you assert; it is a fact you *derive*, and the derivation is `type = [Test.Result]`. Once the type index exists — and it must, for `find` by type — the metadata flag is redundant *and less correct*, because a metadata flag can disagree with the term while a type query cannot. This is the cleanest case in the whole story: **mechanism 6 strictly dominated mechanism 2** for that category.

**What the evidence does not support.** There is no sign anyone concluded "associated data is a bad idea." Mechanisms 4, 5, and 6 all *grew* over the same period. The retreat is specifically from **author-asserted, name-keyed, hashed-into-the-namespace** association — one cell of the table, not the table.

## The reading for arbor

`../../design/02-definitions-and-derived-data.md` already draws the asserted/derived line. This is evidence that the line is **load-bearing rather than descriptive**, and that the two halves want different mechanisms.

**arbor's aspects are almost entirely the surviving kind.** `Type_of` / `Type_with_holes`, `has-holes:v1`, `follow-clean:v1`, the eval cache, translation targets, p17's `test` — every one is derived, hash-keyed, and system-written. That is mechanisms **4** and **5**, which are exactly what survived in Unison and grew. **This finding is not a threat to arbor's aspect design as actually built.**

**The exposure is the asserted half**, which `02` treats as coequal. The specific claim worth testing against this evidence: *an author-asserted, name-shaped fact should probably be a definition with a name, not an aspect entry.* Under arbor's own model that is nearly free — `../../design/03-content-addressing.md` §"The type-aliasing reading" already argues that when names bind hashes, a whole class of features collapses into "bind a name" — and it means such a fact gets aliasing, history, merge, and suffix resolution for free instead of needing a parallel implementation.

**Three sharper points, and one caution.**

- **Key shape is the discriminator, not assertedness per se.** What died was keyed by (definition, *name*). What lived is keyed by *hash*. A hash-keyed asserted fact — say, a human-supplied review or annotation on a specific immutable version — does not obviously suffer any of the four forces above, and arbor has no reason to rule it out. The rule is about **name-keyed side-structure**, which duplicates the namespace.
- **Whether the aspect store is in the hash matters.** Force 2 bites only because metadata was hashed into the namespace. arbor's aspects are *outside* Σ's hash — `formalism/paper/arbor-core.tex` models `E` (and arbor-stlc's `Θ`) as separate derived stores keyed by hash, not as part of any node's content. **arbor already avoids the second force by construction**, which is worth writing down rather than leaving as an accident of the model.
- **Beware the free-lunch reading of naming conventions.** `foo.doc` is not obviously better than a link; it is better *given a namespace with aliasing, merge, history, and suffix resolution already built*. It also has real costs Unison absorbed silently: the convention is unenforceable, the association is untyped, `foo.doc` occupies namespace real estate, and there is no way to attach anything to a definition that has no name. That last one is exactly the case `formalism/decisions.md`'s no-definition-sort stance makes interesting — arbor deliberately allows unnamed stored terms, and a naming convention cannot annotate one.
- **The caution.** Unison lost authorship and licensing entirely. If arbor concludes "name-shaped facts should be names," it should say what happens to the categories that then have nowhere to live, rather than repeating the same silent drop.

Filed as a question, not a decision, at `../../design/open-questions.md` §"Associated data" and in `10-followups.md`.

## Where the argument is weakest

Stated plainly so the next reader can attack it:

- **No stated rationale exists.** Four terse commit messages and a branch name. Everything above is inference.
- **Unison's usage data is invisible from here.** "Nobody used it" is inferred from the removal and from the crash-on-missing-metadata bug, not observed. If metadata was heavily used and removed anyway, force 3 is wrong.
- **The `Doc` counterexample is real and cuts the other way.** Documentation in Unison is a *first-class parsed, typed, content-addressed value* (`06-pl-notes.md` §"Doc literals"). What changed is only how it is **found**. So the retreat is specifically about the association mechanism, not about "everything is a definition" — and a reader who takes this file as an argument against rich associated data has over-read it.
- **The counterfactual is untested.** Perhaps metadata would have survived had it been hash-keyed and outside the namespace hash — which is to say, had it been arbor's design rather than Unison's.
