# The scratch file — Unison's editing context, read in source

**Status:** Source read @ `db60ce2` (see `00-overview.md` for the pin and the evidence conventions). This file discharges the standing note at `06-pl-notes.md` §"The scratch file and slurping", which read the mechanism at notes-depth and said *"worth reading properly if arbor's editing layer grows a multi-definition commit."* That is now the live question — `../../design/13-editing-context.md`.

## The question this read was asked

arbor wants an **editing context**: a named, persistent collection of definitions a user (or agent) is currently looking at and editing, which the system can (a) optimize for and (b) use to scope migration. Unison's scratch file is the nearest shipped thing. What is it actually made of, and how much of the above does it do?

Short answer: the scratch file is a **text file plus two in-memory fields**, it is **not an object in the codebase**, there is **exactly one of it**, and it does (b) — scoping migration — surprisingly aggressively, while doing essentially none of (a).

## What the context *is*

The entire persistent-across-commands state is two fields of `LoopState` (`unison-cli/src/Unison/Cli/Monad.hs:192-216`):

```haskell
    -- The file name last modified, and whether to skip the next file
    -- change event for that path (we skip file changes if the file has
    -- just been modified programmatically)
    latestFile :: Maybe (FilePath, Bool),
    -- Nothing means the file didn't parse
    -- Just (Left) means the file parsed but didn't typecheck
    -- Just (Right) means the file parsed and typechecked
    latestTypecheckedFile :: Maybe (Either (UF.UnisonFile Symbol Ann) (UF.TypecheckedUnisonFile Symbol Ann)),
```

Five observations, each load-bearing for arbor.

1. **It is `Maybe`, not a set.** One file. `LatestFileLocation` defaults to `"scratch.u"` when nothing has been touched (`.../HandleInput/ShowDefinition.hs:174-178`). There is no way to have two working sets open, or to name one.
2. **It is in memory and dies with the process.** `loopState0` initializes both to `Nothing` (`Cli/Monad.hs:220-229`). Nothing about the working set is written to the codebase. `[verified by absence — no `workspace`/`scratch`/`checkout`/`working_set` table among the 19 in `codebase2/codebase-sqlite/sql/create.sql`]`
3. **The reified version was contemplated and never built.** Immediately above `latestFile`:
   ```haskell
     -- TBD
     -- , _activeEdits :: Set Branch.EditGuid
   ```
   `Cli/Monad.hs:195-196`. `EditGuid` was the identifier of a `Patch` — the reified-edit machinery deleted wholesale in 2025 (`05-deletions.md` §"Patches and propagate"). So the *stored, named, multiple* editing context is a road Unison started down and abandoned along with patches.
4. **The `Bool` is a debounce flag, not semantics.** It means "I wrote this file myself, skip the next filesystem event" (`.../HandleInput.hs:196-200`). Worth naming only because it is the one place the file-watcher loop and the command loop have to agree.
5. **The LSP has a second, incompatible one.** `checkedFilesVar :: TVar (Map Uri (TMVar FileAnalysis))` plus `dirtyFilesVar :: TVar (Set Uri)` (`unison-cli/src/Unison/LSP/Types.hs:87-88`), with a rich per-file `FileAnalysis` (`:141-157`: lexed source, parsed file, typechecked file, notes, diagnostics, code actions, symbols). That *is* an n-buffer working set with a dirty set — but it is a different data structure in a different subsystem, and it is not what `add`/`update` commit from. **Unison has two editing contexts that share no representation.**

## What is *in* the context

`UnisonFile` (`parser-typechecker/src/Unison/UnisonFile/Type.hs:18-26`) is a map from **variable** to term:

```haskell
data UnisonFile v a = UnisonFileId
  { fileNamespace :: Maybe (a, Name),
    dataDeclarationsId :: Map v (TypeReferenceId, DataDeclaration v a),
    effectDeclarationsId :: Map v (TypeReferenceId, EffectDeclaration v a),
    terms :: Map v (a, Term v a),
    watches :: Map WatchKind [(v, a, Term v a)]
  }
```

**A definition in the scratch file has no hash.** Hashes appear only in `TypecheckedUnisonFile` (`:45-56`), in `hashTermsId` and in `topLevelComponents' :: [[(v, a, Term v a, Type v a)]]` — grouped by SCC, because Unison hashes components rather than definitions (`02-store-and-hashing.md` §"Components"). Identity is *conferred by typechecking*: until the buffer typechecks, its contents are name-keyed syntax and nothing else.

For arbor this is the single sharpest contrast. arbor's store ingests unconditionally and types observationally (`formalism/paper/arbor-stlc.tex`); a definition has a hash the moment it is a term. So arbor could give a scratch definition a hash *before* it typechecks, which is precisely what would let an editing context be a stored object rather than a text file.

## How the context resolves names

One function, and its doc comment says exactly what it is for (`unison-core/src/Unison/Names.hs:348-353`):

```haskell
-- This is appropriate for shadowing names in the codebase with names in a Unison file, for instance:
-- @shadowing scratchFileNames codebaseNames@
shadowing :: Names -> Names -> Names
```

left-biased union on the *domain* (name), so a name bound in the buffer wins over the same name in the namespace. The editing context is a **namespace overlay**, nothing more. Everything the user is editing is visible to everything else they are editing, automatically, with no gesture — which is where co-dependent edits (`f` and `g` changing together) come from for free.

### Known problem: the overlay reaches names, not stored references `[verified empirically]`

Because `shadowing` is keyed by **name**, the buffer can only shadow something for a lookup that is
happening *now*. A definition already in the codebase resolved its dependencies once, at ingest, and
holds them as hashes — there is no name left in it for the overlay to reach. So a buffer edit is
invisible to every committed dependent, and the buffer will happily compute with the old one.

Commit `scale` and a `double` that calls it:

```unison
scale : Nat -> Nat
scale = n -> n * 2

double : Nat -> Nat
double = n -> scale n
```

then reset the scratch file to a new `scale` and a watch on `double`:

```unison
scale : Nat -> Nat
scale = n -> n * 5

> double 3
```

The watch prints **6**, not 15. After `update` it prints 15, and `double`'s hash moves
`#ro9625q6ee` → `#gi3up8f7ep` although nobody edited it. Nothing is stale in the cache sense — the two
watches are *different expressions*, because `double 3` names a different `double` before and after —
but the number on screen is the one the user did not ask for.

**Unison knows about this, and guards one of the two ways in.** [PR #5964][pr5964] (merged 2025-10-27,
so it is in `db60ce2`) added a staleness check to `run`, and its rationale states the confusion
exactly:

> A `run x` is considered "stale" if `x` … depends (transitively) on some uncommitted update in the
> latest typechecked scratch file, **via some intermediate dependency in the codebase**. The intention
> is to prevent the user from accidentally running old stuff, as they are likely to feel like their
> uncommitted updates in the latest typechecked scratch file are properly shadowing codebase
> definitions.

`checkStale` (`unison-cli/src/Unison/Codebase/Editor/HandleInput/Run.hs:235-290`) walks the transitive
dependents graph and refuses, printing the path:

```
scratch/main> run report

  Sorry, I don't want to run report because it depends on
  something that hasn't been committed to the codebase yet:

    report (in codebase)
    └ double (in codebase)
      └ scale (in file)

  You can `update` to save and propagate these changes into your branch.
```

The `(in file)` / `(in codebase)` annotations were added a month later by [PR #6005][pr6005], because
the first version left ambiguous which link was which — the very distinction that causes the
misunderstanding. [PR #5466][pr5466] added `edit.dependents`, the command the message recommends.

**Only `run` calls it.** Watch expressions go through the save path (`evalUnisonFile`), which never
asks. The same buffer therefore gets a hard refusal from `run` and a silent wrong number from `> `.
No issue is filed for the watch case; the nearest relative is [#1051][i1051], silent use of stale
documentation, which is the same family. (Note that [#5484][i5484] "Update ignores watch expressions"
turns up in searches but is unrelated — that is about definitions written *inside* a watch.) The
[scratch-file documentation][docs] says a watch runs code from "your scratch file and your project"
without saying what happens when the two disagree.

**The shape of the defect, which is what matters for arbor.** The condition is computable, is
computed, and each caller decides separately what to do about it — so the system has as many
behaviours as it has call sites. That is the *second* instance of this shape in this read: the first is
`doHashCycle`, which reports an ordering ambiguity that the top-level caller turns into an abort and
the internal `let rec` caller discards (`02-store-and-hashing.md` §"One caller does not abort"). Both
times a substrate-level predicate was left as per-caller policy, and both times the callers diverged.
`../../design/13-editing-context.md` §"Propagation is not commit" is arbor's response — and locates the
cause in the commit gate rather than in content addressing, since building the rebuilt `double` is only
expensive if building it means committing it.

[pr5964]: https://github.com/unisonweb/unison/pull/5964
[pr6005]: https://github.com/unisonweb/unison/pull/6005
[pr5466]: https://github.com/unisonweb/unison/pull/5466
[i1051]: https://github.com/unisonweb/unison/issues/1051
[i5484]: https://github.com/unisonweb/unison/issues/5484
[docs]: https://www.unison-lang.org/docs/tour/_scratch-files/

## The fold — the working-set boundary, written in text

The `.u` lexer treats `---` as a comment that consumes the rest of the input (`unison-syntax/src/Unison/Syntax/Lexer.hs:60`):

```haskell
    fold = P.try $ lit "---" *> P.takeRest *> pure ()
```

and definitions are **prepended** to the file, optionally followed by a fresh fold line (`unison-cli/src/Unison/CommandLine.hs:61-67`):

```haskell
defaultWriteSourceFile fp contents addFold = do
  path <- Directory.canonicalizePath (Text.unpack fp)
  prependUtf8 path
    if addFold
      then contents <> "\n\n---- Anything below this line is ignored by Unison.\n\n"
      else contents <> "\n\n"
```

So the buffer is split into a **live region** (above the topmost fold, parsed and typechecked) and an **inert region** (below it, invisible to the compiler but still on screen). `RelativeToFold = AboveFold | WithinFold` (`.../Editor/Input.hs:292-303`) chooses which, and the two `edit` commands differ *only* in that flag (`.../CommandLine/InputPatterns.hs:2669-2707`):

| command | flag | effect |
| --- | --- | --- |
| `edit foo` | `WithinFold` | prepend `foo` into the existing live region; names already bound in the file are **excluded**, so re-`edit`ing is idempotent (`ShowDefinition.hs:350-370`) |
| `edit.new foo` | `AboveFold` | prepend `foo` **and write a new fold line below it**, demoting the entire previous buffer to inert |

`edit.new` is therefore the *eviction* gesture: "clear my working set, keep the text as scrollback." This is the whole of Unison's working-set lifecycle, and it is implemented by a string in a text file. It is also why `edit` includes cycles and `view` does not (`ShowDefinition.hs:131-136`) — you cannot edit half an SCC.

## Populating the context

`edit`, `edit.new`, `edit.namespace`, and `edit.dependents`. The last is the one worth reading (`.../HandleInput/EditDependents.hs:33-104`): resolve the name, then

```haskell
          Operations.transitiveDependentsWithinScope
            (Branch.deepDefnsIds branchWithoutLibdeps)
            refs
```

— transitive dependents **restricted to the current project with `lib` deleted** — and render every one of them into the scratch file. That is "check out the blast radius, bounded by the project," and it is the closest thing in the system to a user-chosen migration scope.

## Committing, and what the context does to migration

This is where the scratch file stops being a text buffer and starts being load-bearing.

**Slurping** classifies the buffer against the namespace (`.../Editor/SlurpResult.hs:64-88`): `adds`, `duplicates`, `collisions`, `termExistingConstructorCollisions`, `termAlias`/`typeAlias`, `defsWithBlockedDependencies`, and `extraDefinitions` ("extra definitions that were added to satisfy transitive closure, beyond what the user specified"). It is a typed diff between editing context and namespace.

**`update`** (`.../HandleInput/Update2.hs:89` onward) runs:

1. Take `latestTypecheckedFile`. Drop bindings byte-identical to the namespace's — i.e. things that were `edit`-ed and left alone (`:110-135`). Refuse if anything touches `lib.*`.
2. Compute the namespace's dependents of what actually changed (`getNamespaceDependentsOf`, `:160-170`), **then subtract the ones the file already shadows** (`:173-179`). *The editing context narrows the migration scope: anything you have checked out and fixed yourself is not something the system will try to fix.*
3. **Render** the file *and* those dependents as source text, re-parse, re-typecheck (`:196-215`). The code's own comment for this technique is *"the world's weirdest implementation of AST substitution"* (`:427`).
4. If it typechecks — save everything. If it does not:
   - write the combined text **into the user's scratch file**, under the banner (`:332-338`)
     ```
     -- The definitions below no longer typecheck with the changes above.
     -- Please fix the errors and try `update` again.
     ```
   - and, under `update` v2, create a temporary `update-<branch>` branch whose namespace has those dependents **removed** (`:262-285`).

Step 4 is the finding. **On a failed update, the definitions leave the namespace and the scratch file becomes their only home.** `Load.hs:105-109` says so directly:

> on these branches, dependents have been pulled out of the underlying namespace and put into the scratch file (so that we can support deleting things)

Merge does the same thing (`Merge2.hs:375-399`): conflicts go to `scratch.u`, or to `$UCM_MERGETOOL` with base/local/remote temp files, and the merge branch's namespace is missing them until you commit.

So Unison's answer to *"how should migration work, given what the user is currently interacting with?"* is: **the working set is a staging area that temporarily owns definitions**, and the residual of a migration is delivered *as the next working set*. That is a genuinely good idea and it is not written down anywhere in Unison's documentation.

## What is *not* optimized for the checked-out set

The user-facing motivation "the system could optimize for what is checked out" has, as far as this read goes, no counterpart in Unison.

- The watch/evaluation cache is keyed by expression hash, globally, with no invalidation and no notion of a working set (`04-caching-update-merge.md` §"The cache").
- Typechecking loads dependencies on demand per file (`Cli/TypeCheck.hs:24-34` → `Codebase.typeLookupForDependencies`), which is demand-scoping by the file's own dependency set, not by a persistent working set.
- The name-lookup index that *was* scoped (`scoped_term_name_lookup`) was scoped by *namespace path*, not by what anyone was editing, and was deleted in 2025 (`05-deletions.md`).
- No table, no cache, and no index in the codebase is keyed by anything resembling a workspace. `[verified by absence — see above]`

The honest reading: Unison's codebase is fast enough (SQLite, hash-keyed, one machine) that a working-set-scoped optimization has never been worth building. arbor should treat "optimize for the checked-out set" as a **hypothesis it has to justify**, not as an established requirement — the closest comparable system does none of it and does not appear to suffer.

## Summary for arbor

| Property arbor wants | Unison | Where arbor could differ |
| --- | --- | --- |
| A collection of in-flight definitions | text file + `Maybe` in memory | a stored, hash-addressed object |
| More than one at a time | no (LSP has n buffers, cannot commit) | named contexts as a first-class sort |
| Survives restart / is shareable | no | falls out of storing it |
| Members have identity before they typecheck | **no** — hashes come from typechecking | **yes** — arbor ingests unconditionally |
| Members resolve against each other | yes — `Names.shadowing` | same overlay, but over hashes |
| Boundary of the working set | a `---` line in a text file | an explicit member set |
| Scopes migration | **yes** — dependents already shadowed are subtracted | same idea, made explicit |
| Receives the migration residual | **yes** — failed updates land in the buffer | `arbor-stlc`'s residual, delivered as a context |
| Is optimized for | **no** | unjustified; needs an argument |
| Tells you a result is stale | **`run` only** — a watch silently uses the committed dependent | a property of the result, not of the command |
