# Unison — Merge, branching, and how they interact with update

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope. Everything is `[source-verified @ db60ce2]` unless tagged otherwise. Extends `04-caching-update-merge.md` §"Update" and §"Merge", which summarize; this file is the detail.

## The one-sentence version

**Unison's merge does not merge code. It merges *namespaces* — name-to-hash bindings — and hands back, as a text file, everything it could not decide.** Resolving that file is an ordinary `update`, which is why merge and update are the same machinery seen twice.

*(Corrected 2026-09-03: this sentence said "a text file **with conflict markers**". There are none on the default path. Both versions are written out as complete top-level definitions, each preceded by a comment naming its branch — see §"What the conflict file actually looks like". Git-style markers exist only in the `$MERGED` seed file handed to an external `UCM_MERGETOOL`, and even there the delimiters differ from git's diff3 syntax.)*

That framing is the whole design. Every property below follows from it.

## What a branch is

Not a namespace path. Since the project model landed, a branch is a row in `project_branch` with its own causal head; `merge`, `update`, and `upgrade` each operate on branch roots. Three boolean flags mark a branch's provenance (`codebase2/codebase-sqlite/U/Codebase/Sqlite/ProjectBranch.hs:19-21`):

```haskell
isMerge  :: !Bool,
isUpdate :: !Bool,
isUpgrade :: !Bool
```

backed by three tables — `merge_branch` (`sql/016`), `update_branch` (`sql/017`), `upgrade_branch` (`sql/019`) — recording what the branch is *for* and what it came from. `sql/017`'s comment is the clearest statement of the pattern: *"We put you on a new 'update branch' whenever an `update` fails (from a non-update branch)."*

**A failed operation is not rolled back. It becomes a branch.** That is the same commit-and-report posture arbor takes at `feedback_visible_breakage` and `arbor-stlc.tex`'s residual, expressed through the branch model rather than through a report.

## The merge algorithm

`unison-cli/src/Unison/Codebase/Editor/HandleInput/Merge2.hs`, `doMerge`. The pure algorithm is `unison-merge/` (3,323 lines, no IO); the driver is the CLI.

**1. Fast-forward checks, before loading anything** (`Merge2.hs:184-192`).
- `alice == bob` or `lca == bob` → already up to date, done.
- `lca == alice` → Alice is strictly behind; set Alice's root to Bob's branch and stop. A **pure pointer move, no diffing at all.**

Both are hash comparisons on causal hashes. This is the first dividend of `03-namespace-and-history.md`'s hash-consed history: the common cases cost one equality test.

**2. Load Alice, Bob, and the LCA** (`:197-206`). The LCA comes from the bidirectional BFS over `causal_parent` described in `03-namespace-and-history.md` §"Causal".

**3. Preconditions, both of which abort** (`:208-222`).
- **Neither side may have definitions in `lib`.** `lib` is for dependencies; defining your own code there is refused (`Output.MergeDefnsInLib`).
- **No side may have a conflicted name** — `Branch.asUnconflicted` must succeed for Alice, Bob, *and the LCA*. Note the FIXME at `:215-216` conceding that the error does not say which side the conflict came from, *"even though you can't do anything about conflicted names in the LCA."*

That second precondition is worth pausing on. `03-namespace-and-history.md` §"Names map to sets" establishes that conflicted names are a legal, representable state. Merge is one of the operations that **demands unconflictedness up front** rather than coping. Conflict is representable but not universally workable — a distinction arbor's exact-function `N` cannot even express.

**4. Diff each side against the LCA, and classify by synhash.** Two diffs, LCA→Alice and LCA→Bob, each entry a `DiffOp'Add | DiffOp'Delete | DiffOp'Update` (`Unison/Merge/DiffOp.hs:19-23`). Every updated definition is additionally tagged with whether the change was authored or induced, by comparing **syntactic hashes** — hashes taken after substituting references with names from a combined pretty-print environment (`04-caching-update-merge.md` §"Merge"). The result type carries the bit explicitly: `DiffOp2'Update !(Updated a) !Bool {- is propagated? -}`.

This is where auto-propagated changes get filtered out of the merge. Without it, every definition downstream of an edit would look like a competing change.

**4a. What "propagated" means, precisely, and what it buys.** A definition's *syntactic hash* is taken after substituting every reference with a **name** drawn from a PPE combining Alice's, Bob's and the LCA's namespaces (`Diffblob.hs:418-422`, unsuffixified, Alice first). Variable names become de Bruijn indices, source annotations vanish, and `Ref`/`Constructor`/`Request` all collapse to the same tag plus a name (`Synhash.hs:340-343`). So two definitions that would *render the same to a human* synhash equal.

The diff then does something stronger than tagging (`Diff.hs:69-84`): an update whose synhash is unchanged is **removed from the diff entirely**, into a `propagatedUpdates` map that nothing ever reads. An auto-propagated update is not a weak change — it is *no change at all*.

Verified live: Alice edits `foo`, which propagates to `bar`; main edits `bar` by hand. Both sides changed `bar`, and the merge is clean, keeping `foo = 2` and main's authored `bar`. Without this the two would collide on every downstream definition.

Note the hash class: `Unison.Hashable`, **not** the content-addressing hash, and never persisted. It only has to be self-consistent within one `merge` process, where all three sides are hashed against the same PPE — so its token encoding carries no compatibility obligation.

**5. Combine the two diffs and partition** (`Unison/Merge/CombineDiffs.hs`, `PartitionCombinedDiffs.hs`). The output splits into:

```haskell
data Unconflicts v = Unconflicts
  { adds    :: !(TwoWayI (Map Name v)),
    deletes :: !(TwoWayI (Map Name v)),
    updates :: !(TwoWayI (Map Name v)) }
```
`Unison/Merge/Unconflicts.hs:20-25`. `TwoWayI` distinguishes "Alice only," "Bob only," and "both agreed" for each.

**Conflicts are keyed by *name*, not by definition.** A conflict is one name that Alice and Bob bound to different, non-propagation-related hashes.

**6. Pull the dependents of conflicts into the file too.** `Unison/Merge/Mergeblob.hs:120-201` computes `dependentsIds` from the conflict set and folds them into what gets rendered. The comment at `:172` says it plainly — the job is *"identifying the unconflicted dependents we need to pull into the Unison file."*

This is the merge/update interaction, and it is the crux. A conflicted definition cannot be resolved in isolation: whatever the user picks, everything downstream must be re-typechecked against it. So merge computes the same transitive-dependents closure `update` does (§"How update picks dependents" below) and puts those definitions into the scratch file alongside the conflicts, unresolved but present.

**7. Merge `lib` separately** (`Unison/Merge/Libdeps.hs`). Dependencies are not ordinary definitions; a `LibdepDiffOp` merges the dependency *sets* rather than their contents.

**8. Produce the outcome** (`Unison/Merge/Mergeblob.hs:68-77`):

```haskell
data Mergeblob libdep = Mergeblob
  { conflicts         :: TwoWay (DefnsF (Map Name) TermReferenceId TypeReferenceId),
    typecheckedFile   :: Maybe (TypecheckedUnisonFile Symbol Ann),
    unconflictedDefns :: DefnsF (Map Name) Referent TypeReference,
    uniqueTypeGuids   :: TwoWay (Map Name Text),
    unparsedFile      :: Pretty ColorText,
    unparsedSoloFiles :: ThreeWay (Pretty ColorText) }
```

Note `typecheckedFile :: Maybe`. **If the combined result typechecks, the merge completes.** If it does not, `unparsedFile` — the conflict-marker scratch file — is what the user gets, and they land on a merge branch. `unparsedSoloFiles` is the three-way variant for an external mergetool.

Note also `uniqueTypeGuids`: the mint-stability machinery from `04-caching-update-merge.md` §"Minting" is threaded through merge specifically, because this is the operation that would otherwise let two branches mint independently for the same type name.

## How update picks dependents — and whether it maps to arbor's strategies

`unison-cli/src/Unison/Codebase/Editor/HandleInput/Update2.hs`, `handleUpdate2` at `:89`.

The selection is one call (`Update2.hs:163-171` via `unison-cli/src/Unison/Cli/UpdateUtils.hs:73-89`):

```haskell
getNamespaceDependentsOf defns dependencies =
  Operations.transitiveDependentsWithinScope (Names.unconflictedReferenceIds defns) dependencies
```

and the operation it calls is documented at `codebase2/codebase-sqlite/U/Codebase/Sqlite/Operations.hs:1125-1126`:

> `transitiveDependentsWithinScope scope query` returns all transitive dependents of `query` that are in `scope` (excluding self-references).

So, precisely:

- **Depth: transitive, not a frontier.** Everything downstream, to closure.
- **Scope: every reference with an unconflicted name in the current branch.** Naming is what puts a definition in scope — an unnamed definition is not a dependent to be updated. That is `formalism/`'s no-definition-sort stance doing load-bearing work in a second place.
- **Then subtract** what the file itself shadows (`Update2.hs:173-179`) and reject anything in `lib.*` outright (`:139-144`).
- **No type-based gating whatsoever.** Every selected dependent is re-rendered, re-parsed, and re-typechecked together.

That last point is the interesting one, because **Unison used to gate on types and stopped.** The deleted `Patch` model classified each edit as `Same | Subtype | Different` and propagated only the first two automatically (`05-deletions.md` §"Patches and propagate"). The current model computes the closure and lets the typechecker decide, reporting failure rather than predicting it.

### Mapped onto arbor's update strategies

`../../design/04-naming-layer.md` §"Update strategies" and p11 give three points in a two-axis space (scope × user-in-loop): **pin**, **follow**, **explicit**.

| arbor | Unison's `update` |
|---|---|
| **Follow** | This, and only this. Every transitive dependent in the namespace is rebound to the new hash. |
| **Pin** | Not offered. There is no way to say "leave my callers on the old hash." The nearest thing is to not run `update` — bind a new name with `add` instead, which is pin-by-abstention, not pin-as-an-operation. |
| **Explicit / per-unit choice** | Not offered *at selection time*. It reappears at *failure* time: what fails to typecheck comes back as source, and the user edits it by hand. |

Two conclusions, and they cut in different directions.

**Unison's update is arbor's Follow with scope fixed at "the whole namespace," and no other strategy exists.** arbor's 2-axis framing is strictly richer than what the closest comparable system ships. That is worth knowing before treating the framing as descriptive of prior art — it is not; it is arbor's own.

**But Unison has something arbor's Follow does not: a defined behavior when Follow fails.** arbor-stlc proves *type-preserving migration is total* (same type at the seed ⟹ empty residual) and otherwise commits-and-reports a residual set of names. Unison's answer to the non-type-preserving case is more than a report — it **evicts the broken dependents from the namespace and hands them back as editable source** on a dedicated branch (`Update2.hs:219-258`). The namespace stays coherent; the broken work becomes a scratch file.

That is arbor's residual, rendered. `formalism/open-questions.md` §"Modeling questions from `arbor-stlc`" already asks for *"richer residual reporting … per-name witnesses (expected vs. actual type at the failing position)"* and asks whether that is editing-layer elaboration or part of the report. Unison's answer is the strongest version available: the residual is **the code itself**, and the editing layer's job is to hand it to you. Recorded in `10-followups.md`.

### How update and branching compose

`Update2.hs:253`:

```haskell
if pp.branch.isUpdate || pp.branch.isUpgrade || pp.branch.isMerge
```

If `update` fails while you are *already* on an update, upgrade, or merge branch, it does **not** create another branch — it updates that branch in place. Only a failure from an ordinary branch spawns a new one.

So the three operations compose into one loop:

1. `merge` (or `update`, or `upgrade`) computes a closure and re-typechecks.
2. Success → the branch advances.
3. Failure → you are on a `*_branch` with a scratch file holding the conflicts and their dependents.
4. You edit and `update` again. Because you are already on such a branch, step 3 does not nest.
5. Repeat until it typechecks, then merge the branch back.

**The merge conflict and the failed update are the same object**, reached by different routes and resolved by the same gesture. That is a real design economy, and it is only possible because the resolution medium is *text that must re-typecheck* rather than a structured patch.

## The conflict taxonomy — what actually conflicts

A conflict is produced in **exactly two places** (`CombineDiffs.hs:56-66`): both sides added the same
name with different synhashes, or both sides updated it with different synhashes. Everything else is
resolved or refused. The seed set is then closed under a fixpoint (`PartitionCombinedDiffs.hs:65-141`)
that drags in the *other* party's constructor names when a type conflicts, and the enclosing decl when
a constructor conflicts.

| situation | outcome |
|---|---|
| both add different names | merge |
| both add the same name, same synhash | merge |
| both add the same name, different bodies | **conflict** |
| both update the same name, different bodies | **conflict** |
| one updates, other propagates | merge — the propagated side is not a change |
| one updates, other **deletes** | **merge, delete silently dropped** |
| both delete | merge |
| any two explicit changes to the same *type* | **conflict**, always — even disjoint constructor edits |
| constructor **rename** vs. any type update | **conflict** — a rename is an update to the decl |
| two independently-created `unique type`s that look identical | **conflict** — the GUID is hashed |
| term on one side, constructor on the other | **conflict** |
| conflicted name resolves to a builtin | **refused** — cannot render a builtin in a scratch file |
| `lib.*` differs | never conflicts — unioned, with `__0`/`__1` suffixes |
| you touched nothing but depend on a conflict | your definition is **evicted into the file** |

Two of these are worth dwelling on because they are silent.

**Update + delete is not a conflict.** `CombineDiffs.hs:60-63`, verbatim: *"These two are not a
conflicts, perhaps only temporarily, because it's easier to implement. We just ignore these deletes
and keep the updates."* Verified live — Alice deleted `gone`, main edited it to `42`, merge reported
success and `gone = 42` survives. Git stops and asks; Unison discards the delete and says nothing.
The upstream transcript concedes *"In a future version, we'd like to give the user a warning at
least."*

**Renames are not detected at all.** `Unison.Merge.Rename` computes `simpleRenames`, and
`Diffblob.simpleRenames` is **read by nothing in the repository** — a whole-tree grep finds hits only
inside `Diffblob.hs` itself. In the core diff, keyed by `Name`, a rename is a delete at the old name
plus an add at the new one. (Constructor renames are the exception, and only because they change the
enclosing decl's synhash.)

## What the conflict file actually looks like

No markers. `Render.hs:191-235` emits both versions as **complete top-level definitions**, each
preceded by one comment naming its branch. Verified by running it:

```unison
-- scratch/main
foo : Nat
foo = 3

-- scratch/alice
foo : Nat
foo = 2

-- The definitions below are not conflicted, but they each depend on one or more
-- conflicted definitions above.

bar : Nat
bar =
  use Nat +
  foo + 10
```

**The file deliberately does not parse** — two `foo : Nat` bindings at top level is an error, and
`Mergeblob.hs:225-227` tries to parse it and gives up. Resolution is "edit until it parses", not
"delete the marker lines". You then run `update`, which merges the temporary branch into its parent
and deletes it; or `cancel`, which deletes the branch and prints nothing.

Two sharp edges. The file is **prepended to**, not replaced, so whatever you were working on is
pushed below `---- Anything below this line is ignored by Unison.` and silently stops compiling. And
the banner above is **not always true**: `identifyCoreDependencies` also pulls in dependents of your
*own unconflicted* updates (`Mergeblob.hs:271-274`), which the upstream transcript flags as a bug
(`merge.md:2696`, "This test demonstrates a bug").

Git-style markers exist only on the `UCM_MERGETOOL` path (`Merge2.hs:562-604`), and they are
**diff3-shaped but not git-diff3 syntax** — `<<<<<<< / ======= / ======= / >>>>>>>`, where git writes
`|||||||` for the middle delimiter. A tool configured for git's diff3 will mis-parse it.

## `lib` is a magic name segment, and it is the only pin Unison has

Two things about `lib` that are easy to state wrongly, both checked.

**You cannot write to `lib` by hand.** Editing a definition there is refused — *"Modifying definitions
in `lib` is not allowed"* — and so is adding one from a scratch file. It is populated only by
`lib.install`, `pull` and `clone`. So every rule about `lib` concerns installed dependency subtrees,
never anything a user typed.

**It is a distinguished `NameSegment`, not a sort.** `deleteLibdeps = over children_ (Map.delete
NameSegment.libSegment)` (`Branch/Type.hs:364-366`). Around twenty modules special-case that one
string. `Definition` has no notion of "is a dependency"; the store cannot tell a library from
anything else.

### The diamond-dependency claim, checked `[verified empirically]`

The usual framing is that content addressing dissolves the diamond: two versions of a library are two
hashes, both persist, nothing breaks. That is true **at the store level, universally** — and it is
not the whole claim, because for ordinary definitions nothing is left pointing at the old hash.

Measured: `f = 1`, `usesF = f + 10`, `add`, then `update f = 2`. `usesF` evaluates to **12**, not 11.
Its reference was rewritten. The old `f` is still in the store, still correct, and now reachable by
nothing — `update` propagates over every named transitive dependent and there is no parameter to
decline (`prop:follow-only`).

The one exemption is `lib`, which `update` subtracts from the dependent scope and refuses as a
target. **So library pins survive because Unison declines to rebuild them, not because libraries are
special objects.** Diamond tolerance is a consequence of non-propagation, and `lib` is simply the
region where propagation is switched off.

**For arbor this is the sharpest available datum on the update-strategy axis.**
`../../design/04-naming-layer.md` frames pin / follow / explicit as points in a scope × user-in-loop
space. Unison ships Follow with scope fixed, plus exactly one hard-coded scope exclusion keyed off a
magic name. Its "pin" is not a strategy; it is a directory. That both confirms the framing is richer
than prior art and suggests where the pressure comes from: the thing a real system needed first was
*a region that does not get updated*, which is the scope axis pinned at a constant.

## What this means for arbor

**On the namespace-as-store question.** `01-organization.md` §"The layer graph" notes that arbor's Attachment layer has no Unison counterpart because the namespace lives inside the store. This is where that pays off: because a namespace is a hashed value and history is a causal DAG over namespace values, `merge` gets fast-forward-by-hash-equality, LCA-by-graph-search, and *"is this the same namespace"* as free operations. arbor's `N` as a partial function beside `Σ` gets none of them. `formalism/open-questions.md` §"The 'work up' ladder" lists branching as an unattacked rung; the shape of the answer is that `N` has to become a hashed object before `merge` is expressible at all.

**On conflicts.** arbor's `N : Name ⇀ Hash` cannot represent a conflicted name, so it cannot represent a merge in progress. Unison represents it, then requires most operations to reject it. The lesson is not "make `N` a relation" — it is that **the intermediate state of a merge is a namespace that does not satisfy the invariants the steady state does**, and a model where `N` is a function has nowhere to put that state.

**On update strategies.** See the table above. arbor's framing is richer; Unison's failure handling is better. Neither system has the other half.

**On what merge does *not* do.** It does not merge two versions of a definition. Structural or semantic three-way merge of code never happens — the definitions are opaque, the merge is over names, and anything ambiguous becomes text. `../../related-work/05-naming-versioning.md` covers structured diff and merge, and `../../related-work/08-agentic-vc.md` records that 100% structural convergence still leaves 5–10% semantic conflicts. Unison sidesteps that entire literature by never attempting it. Whether arbor should is an open question and a genuine fork: p11's `multi_rebind` is namespace-level like Unison's, but arbor's stored ASTs would *permit* structural merge in a way Unison's opaque components do not. Recorded in `10-followups.md`.
