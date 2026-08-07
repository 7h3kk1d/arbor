# Unison — Caching, update, merge, and minting

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope. Everything is `[source-verified @ db60ce2]` unless tagged otherwise.

## The cache

Unison's most-cited claim is that evaluation results are never invalidated. It is true, and the way it is true is more interesting than the claim.

### There is no invalidation logic

```sql
CREATE TABLE watch_result (
  hash_id INTEGER NOT NULL REFERENCES hash(id),
  component_index INTEGER NOT NULL,
  result BLOB NOT NULL,
  PRIMARY KEY (hash_id, component_index)) WITHOUT ROWID;
```
`codebase2/codebase-sqlite/sql/create.sql:121-153`.

Searching for any invalidation machinery across the storage layer, the codebase layer, and the CLI returns nothing: `rg -ni "invalidat" codebase2/codebase-sqlite parser-typechecker/src unison-cli/src` produces **zero matches** `[verified by absence @ db60ce2]`. The only paths that remove a cached result are:

- `clearWatches` (`Queries.hs:1479-1482`) — a wholesale `DELETE FROM watch_result`, reachable only from the debug command `DebugClearWatchI` (`unison-cli/src/Unison/Codebase/Editor/HandleInput.hs:338`);
- a GC query dropping watches whose hash has no `hash_object` row (`Queries.hs:1720-1723`);
- and errors are simply never written (`RuntimeUtils.hs:84-86`).

None of these is invalidation. **A stale entry is unreachable rather than wrong**, because the key is the full content hash of a closed term, which transitively includes every dependency's hash (`02-store-and-hashing.md` §"Components"). Any change anywhere below produces a different key.

**This is arbor-core's `thm:stability` and `cor:cache` — evaluation stability under store growth, and no re-evaluation — running in production and never stated.** `../../related-work/00-index.md:102` is right that the claim has never been formally established; what this read adds is that the *implementation* is exactly the one the theorem describes, down to the absence of the code that the theorem makes unnecessary.

### The key need not be a definition

`sql/create.sql:141-143` carries **Note [Watch expression identifier]**:

> The `hash_id` + `component_index` is an unevaluated term reference. We use `hash_id` instead of `object_id` because the unevaluated term may not exist in the codebase: it is not added merely by watching it without a name, e.g `> 2 + 3`.

So the cache is keyed by the hash of an arbitrary closed expression, named or not, stored or not.

**That is arbor-core's no-definition-sort stance, arrived at independently.** `formalism/decisions.md` (2026-07-30) retracted an explicit root set as "mint-flavored" and settled on: the store carries no definition sort, and naming a closed stored term is the only assertion of definition-hood. Unison's watch table is the same commitment expressed as a schema comment — the thing being cached is an expression, and being a definition is a separate, later, optional fact about it.

### Read path and the one side condition arbor has no analogue for

`Runtime.evaluateWatches` (`parser-typechecker/src/Unison/Codebase/Runtime.hs:88-136`), documented at `:93-95`:

> The definitions in the file are hashed and looked up in `evaluationCache`. If that returns a result, evaluation of that definition can be skipped.

Hash everything in the file, consult the cache, return an `IsCacheHit` flag per watch. `Runtime.noCache` opts out.

But not everything is cacheable, and the condition is a type-level one:

```haskell
-- A term's result is cacheable iff it has no arrows in its type,
-- this is sufficient since top-level definitions can't have effects without a delay.
Just typ | not (Rec.cata hasArrows typ) -> pure (r, CodeRep sg Cacheable)
_ -> pure (r, CodeRep sg Uncacheable)
```
`unison-runtime/src/Unison/Runtime/Interface.hs:463-492`. And `RunWatch` is never cached at all, so side effects re-run (`Runtime.hs:115, 120, 129-134`).

**arbor's `E` has no such condition, and should eventually say why.** In arbor-core, `E : Hash ⇀ Hash` maps a term hash to the hash of its value, and evaluation is pure CBV/WHNF over closed terms — nothing can be effectful, and a function value is just another closed term, so caching it is harmless. Unison needs the guard because a cached *function* would be a cached closure over a runtime code cache, and because abilities mean a value's type is where effectfulness is visible. The observation for the formalism is that **`E`'s totality is a consequence of the language being pure and values being terms**, and the moment either changes — effects, or a value representation distinct from the term representation — a `checkCacheability`-shaped side condition appears. Filed in `formalism/open-questions.md`.

### Test results are content-addressed and shipped

`watch_kind_description` distinguishes `0 = Regular` (*"won't be synced"*) from `1 = Test` (*"will be synced"*) — `sql/create.sql:146-153`. Test results travel with the codebase. Read at `unison-cli/.../HandleInput/Tests.hs:76`, written at `:132`.

The rationale is in `docs/testing.markdown` `[design doc — describes intent, not current behavior; the `tests/` directory it describes is now the SQLite `watch_result` table]`, and it is the clearest statement anywhere of why this works:

> We can ask if a branch is passing just by taking the intersection of the hashes in the branch with the hashes in this directory and seeing if all the `Test.Status` values for the branch are `Passed`. Notice this doesn't involve running any of the tests!

> The `tests/` directory will be versioned, so everyone collaborating on the code shares a cache of test results. As the tests are 100% deterministic, this is fine, unless of course someone manually mucks with that directory to doctor some test results…

And an epistemics note worth keeping (`:57`):

> I kinda like the "trust but occasionally reverify" model for this kind of caching. So every once in a while, pick a random test to rerun and make sure it checks out. With statistics, over time, it becomes exceedingly likely that the cache is good…

**Observation for arbor.** `../../related-work/04-incrementality.md:29` already notes that p17's `test` aspect is this feature. What the source adds is the *distribution* dimension: because the cache key is intrinsic, the cache is shareable, and a shared cache turns "did the tests pass" from a computation into a set intersection. arbor's `E` is proved stable under *store growth*; Unison relies on it being stable under *transfer between codebases*, which is the same theorem with a different quantifier and is the thing that makes the commons argument in `../../related-work/07-commons.md` concrete. Also note the honest limit the doc states itself — the guarantee is only as good as nobody having doctored the cache, which is a trust problem, not a soundness one.

## Update — printing, re-parsing, and re-typechecking

`unison-cli/src/Unison/Codebase/Editor/HandleInput/Update2.hs`, `handleUpdate2` at `:89`. The algorithm:

1. Assert the namespace has no conflicted names (`:99-102`) and no incoherent decls (`:104-108`).
2. Diff the typechecked scratch file against the namespace to find genuinely added and updated bindings; reject anything touching `lib.*` (`:110-144`).
3. Query the **dependents index** for everything transitively affected (`getNamespaceDependentsOf`, `:160-168`).
4. Render those dependents back to Unison source under a carefully-built pretty-print environment, then **re-parse and re-typecheck** them against the new definitions.

Step 4 is the striking one, and the code says so itself (`:424-427`):

> We are updating old references to new references by rendering old references as names that are then parsed back to resolve to new references (**the world's weirdest implementation of AST substitution**).

The PPE construction is where the difficulty lives, and the comment continues (`:429-443`) with the reason: names for definitions *in the file* must be suffixified by name, while names for things *in the codebase* must be suffixified by hash, because aliases sharing a suffix are usable in the codebase but ambiguous in a file:

```
one.foo = 10
two.foo = 10
hey = foo + foo   -- "Which foo do you mean? There are two."
```

On failure you land on a dedicated *update branch* (`update_branch` table, `sql/017`: *"We put you on a new 'update branch' whenever an `update` fails (from a non-update branch)"*).

### Why this matters to arbor specifically

**Unison's `update` is correct only if printing and re-parsing preserves hashes.** That property is not stated anywhere in the implementation. It is *tested*: `unison-src/transcripts-round-trip/` prints an entire regression corpus with `edit.new`, re-adds it into a fresh namespace, and asserts `diff.namespace /a1: /a2:` is empty — the transcript says *"This diff should be empty if the two namespaces are equivalent. If it's nonempty, the diff will show us the hashes that differ."* The corpus file is literally named `reparses-with-same-hash.u`, and its sibling `reparses.u` holds the weaker cases that need only parse.

**arbor already proves this.** `formalism/paper/arbor-core.tex` Part III makes printing a judgment (`P-Var`/`P-Ref`/`P-Lam`/`P-App`) and establishes `prop:print-elab` — print-then-elaborate is exact for every printer choice — and `prop:elab-print` up to the kernel of elaboration. `formalism/decisions.md` (2026-07-30) records that this replaced a vaguer "identity up to name choice" statement.

So: **the closest comparable system has a load-bearing correctness property that it maintains with a golden-file regression corpus, and arbor has the theorem.** That is a concrete, defensible contribution claim, and a better one than the general "first formal treatment" framing, because it points at a specific implementation that would benefit. Filed in `formalism/open-questions.md`; see `07-arbor-mapping.md` §"Migration".

It also frames the design fork honestly. arbor's `Migrate` is a structural rewrite `ρ` over stored nodes — substitution done directly, no surface syntax involved. Unison round-trips through text. arbor's approach needs no round-trip lemma at all for migration to be correct; Unison's gets, in exchange, the property that whatever the user is shown *is* what was re-typechecked. Neither dominates.

## Merge, and synhash — provenance without mints

`unison-merge/` is a pure library, 3,323 lines, no IO, no storage dependency; its driver is `unison-cli/src/Unison/Codebase/Editor/HandleInput/Merge2.hs`. Three-way diff of namespaces (LCA / Alice / Bob), classified as `DiffOp'Add | DiffOp'Delete | DiffOp'Update` (`Unison/Merge/DiffOp.hs:19-23`), combined, partitioned into conflicted and unconflicted, with `lib.*` dependency sets merged specially (`Libdeps.hs`).

The piece worth arbor's attention is **synhash**. `unison-merge/src/Unison/Merge/Synhash.hs:1-26` explains it in full:

> Utilities for computing the "syntactic hash" of a decl or term, which is a hash that is computed **after substituting references to other terms and decls with names from a pretty-print environment**. Thus, syntactic hashes can be compared for equality to answer questions like "would these definitions look the same when rendered for a human (even if their underlying references are different)?".
>
> The merge algorithm currently uses syntactic hashes for determining whether an update was performed **by a human, or was the result of auto-propagation**. (Critically, this cannot handle renames very well).

With the worked example: `foo = #bar + 3` and `foo = #bar2 + 3` both render as `foo = helper + 3` under a combined PPE, so they share a synhash, so the change was induced rather than authored. Carried as `Synhashed a = Synhashed { hash :: Hash, value :: a }` and surfaced as `DiffOp2'Update !(Updated a) !Bool {- is propagated? -}` (`DiffOp.hs:60-64`). It uses `Unison.Hashable`, the *non*-content-addressing hash (`02-store-and-hashing.md` §"the two-hash-scheme wrinkle").

**This is a genuine alternative to arbor's mint threads.** `../../design/10-minted-identity.md` and p11's threads answer "is this new definition the same thing as that old one?" by *carrying a mark forward through edits*. Unison answers a closely related question — "did a human do this, or did it fall out of propagation?" — by **hashing modulo names**: quotient out the identity of the dependencies, and see whether what remains is the same. No extra identity axis, no side table, nothing to keep stable.

Its stated limit is exactly where a mint would win: *"critically, this cannot handle renames very well"* — because the quotient is *by name*, a rename destroys it. p11's threads survive renames by construction. So the two mechanisms fail in complementary places, which is the useful shape of the finding. Filed for `../../design/open-questions.md` §"Minted identity" and `formalism/open-questions.md`.

Supporting state: `merge_branch` (`sql/016`) records an in-progress merge's source and target causals so it can be resumed.

## Minting — `unique` types, and what they cost

### Unison is unique-by-default, source-verified

`parser-typechecker/src/Unison/Syntax/DeclParser.hs:225-230`:

```haskell
resolveModifier name modifier =
  case L.payload <$> modifier of
    Just UnresolvedModifier'Structural        -> pure DataDeclaration.Structural
    Just (UnresolvedModifier'UniqueWithGuid g) -> pure (DataDeclaration.Unique g)
    Just UnresolvedModifier'UniqueWithoutGuid -> resolveUniqueTypeGuid name.payload
    Nothing                                   -> resolveUniqueTypeGuid name.payload
```

The last line is the answer: **a declaration written with no modifier at all mints a GUID.** `type Foo = ...` is unique; `structural type Foo = ...` is the marked opt-out. The GUID enters the hash as `tokens (Unique txt) = [Tag 1, Text txt]` (`Hashing/V2/DataDeclaration.hs:130-132`).

The pretty-printer then drops it: `prettyModifier RenderUniqueTypeGuids'No (DD.Unique _guid) = mempty` (`parser-typechecker/src/Unison/Syntax/DeclPrinter.hs:304`), which is why round-tripped output shows a bare `type` — and which is what `../../related-work/01-content-addressing.md` had inferred from rendered output and tagged `[inference from rendered output, not stated on the page]`. **The inference was correct; it is now primary evidence.** This closes the gap recorded in that file's §"Gaps and negative findings" ("Unison's unadorned `type` default is undocumented") and answers `../../design/open-questions.md:77`.

Note also that Unison's *own* design doc gets this backwards. `docs/data-types.markdown:11-12` says structural *"is the current default if you just write `type Blah = ...`"* and proposes `nominal type` as hypothetical future syntax `[design doc — describes intent, not current behavior]`. The intent inverted somewhere between the doc and the implementation, and nobody updated the doc. A good reminder that a system's prose is evidence about its designers, not about its behavior.

### And it is per-sort, which is where arbor also landed

Terms are never minted; only decls carry a `Modifier`. `../../design/10-minted-identity.md:93` already records this observation and its consequence — that per-sort minting is a third option beside per-definition and per-language, and that arbor converged on it independently (p16/p17 mint only labels, p12 mints `Opaque` types, terms stay structural). That entry can now drop its hedging.

### The recurring cost, which arbor's docs do not record

A mint must survive being re-saved. If you edit `type Foo` and `update`, the new declaration must carry the *same* GUID, or it becomes a different type and every dependent breaks. Unison recovers the GUID **by name**:

```haskell
-- | @loadUniqueTypeGuid loadNamespaceAtPath path name@ looks up the GUID associated with the unique type named @name@
-- at child namespace @path@ in the root namespace. If there are multiple such types, an arbitrary one is chosen.
```
`parser-typechecker/src/Unison/Codebase/UniqueTypeGuidLookup.hs:19-20`, implementation at `:29-46` — find the name in the branch, load each decl behind it, take the first `Unique guid`.

Two things follow, and both matter for arbor:

1. **The mint is recovered from the name, so it is only as stable as the name.** Rename the type and re-save, and you have minted a new one. This is the exact dual of synhash's limitation above, in the same system.
2. **Ties are broken arbitrarily.** *"If there are multiple such types, an arbitrary one is chosen."* When a name is conflicted — a legal state, per `03-namespace-and-history.md` §"Names map to sets" — the identity of the thing you just saved depends on an unspecified choice.

And merging needs its own machinery to keep GUIDs from diverging: `namespace_unique_type_guid(namespace_hash_id, type_name, type_guid)` (`sql/016`), plus `makeUniqueTypeGuids` (`unison-cli/src/Unison/Cli/UpdateUtils.hs:189-211`), which walks the affected types, loads each decl, and extracts its GUID so the same one can be re-used.

**Observation for arbor.** `../../design/10-minted-identity.md` and p11's mint threads treat mint stability as something the substrate provides via an explicit *edit-of-X* gesture — the editor says "this is a new version of that," and the mark is inherited. Unison has no such gesture, so it reconstructs the intent heuristically from the name at save time, and pays for it with a side table, a rename hazard, and an arbitrary tiebreak. **That is an argument in favour of arbor's explicit-gesture design**, and it is the first piece of external evidence for it. It is also a warning: p11's threads are stable under rename but the substrate still owes an answer for what happens to a thread under *merge*, which is where Unison needed `namespace_unique_type_guid`. `formalism/open-questions.md` §"The 'work up' ladder" lists mint/thread identity as an unattacked rung; this is what it will have to handle.

## What replaced patches

The old model — `Patch` as a hash-addressed set of `Reference → TermEdit` mappings, replayed onto dependents by `propagate`, with `IsPropagated` metadata marking machine-made changes — is gone. The types survive because old history must remain readable, but nothing creates them. See `05-deletions.md` §"Patches and propagate" for the full account and the commit.

The shift, in one line: **from reified edits to recomputed diffs.** Instead of storing what changed, compare namespaces at their LCA; instead of replaying substitutions, query the dependents index and re-typecheck; instead of marking auto-propagated definitions with metadata, recover that fact by hashing modulo names.
