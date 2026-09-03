# Unison versus a git + package-manager workflow

**Status:** Comparison, written 2026-09-03. Source claims pinned to `db60ce2`; behavioural claims run against `ucm 1.1.1` and tagged `[verified empirically]`. Unison's own claims are quoted verbatim from `unison-lang.org` and each is given a verdict.

This is the doc to read before repeating a Unison marketing claim in `docs/design/`. Several are true, one is stronger than Unison states it, and three are true only under conditions Unison does not mention.

## The one substitution everything follows from

Git versions **a tree of text files**. Unison versions **a map from names to hashes**, over a store of immutable definitions.

Every difference below — the wins and the losses — is a consequence of that one substitution. Git's unit of change is a line in a file; its unit of identity is a path. Unison's unit of change is a name binding; its unit of identity is a definition's hash. Once you accept that, you can predict most of the behaviour without looking it up.

| | git | Unison |
|---|---|---|
| Unit of identity | file path | definition hash |
| Unit of change | line (hunk) | name binding |
| A conflict is | two edits to nearby **lines** | two hashes for one **name** |
| Merge operates on | text, three-way | the name → hash relation |
| Merge can produce | text that does not compile | a namespace, or nothing at all |
| History records | full content of every version | which **names** appeared and disappeared |
| Working state | the tree itself | a scratch file outside the codebase |
| Formatting, comments | versioned content | not stored at all |

## Unison's claims, checked

Quotations are verbatim from `unison-lang.org` (`/`, `/docs/the-big-idea/`, `/blog/reducing-churn/`), retrieved 2026-09-03.

### 1. "This eliminates builds" — **true, with a runtime asterisk**

> "Other tools try to recover structure from text; Unison stores code in a database. This eliminates builds"
> "we can parse and typecheck definitions once, and then store the results in a cache which is never invalidated."

The typechecking claim holds and is `[verified by absence]` in `04-caching-update-merge.md`: there is no invalidation logic, not merely rare invalidation. Nothing ill-typed is ever stored, so nothing ever needs re-checking.

The asterisk is the runtime, established in `12-state-inventory.md`. `preEvalTopLevelConstants` eagerly evaluates every arrow-free constant in the transitive closure of whatever you run, on **every process start**; those values are never persisted; and the pass does not benefit from its own results — a chain of N constants costs N × base, measured 1.00 / 2.01 / 3.00 / 3.99 / 4.98. So "no build" is partly a trade: compile-time work becomes start-up work that no cache captures. Test results *are* persisted and synced (`kind=Test` rows); ordinary watch results are persisted but not synced; top-level constant values are never persisted at all.

### 2. "instant nonbreaking renames" — **true for code, false for merges**

A rename changes a name binding. The definition's hash does not move, so no dependent is touched, no text is rewritten, and nothing can break. In git a rename is an O(codebase) textual diff with a large conflict surface, and rename detection is a similarity heuristic that fails on rename-plus-edit. As a claim about *breakage*, this is unqualified and it is Unison's cleanest win.

As a claim about *cost*, it is wrong, and the cost lands on whoever else is working at the same time. Merge separates a human's edit from an auto-propagated one using the **syntactic hash**, which is computed after substituting each reference with a **name** — so a rename changes the syntactic hash of every dependent, reclassifying propagated updates as authored ones. `Merge/Synhash.hs`'s docstring: *"(Critically, this cannot handle renames very well.)"*

Measured, `[verified empirically]` — two runs identical but for one `move.term`; alice updates `dep` and lets `usesDep` propagate, main edits `usesDep`. Control merges clean; with the rename it conflicts, alice's side showing `usesDep = renamed + 100`, which is the ancestor's own code under a new name. A rename *alone* against a concurrent edit fails differently: zero conflicts, and a scratch file referencing a name the merged namespace no longer has. Full mechanism and both transcripts in `08-merge-and-branching.md` §"A rename can manufacture a conflict in a definition nobody edited".

*(Corrected 2026-09-03. This section previously rated the claim upheld with only the review caveat below.)*

One more, `[verified empirically]`: `diff.branch` does **not** report a rename as a rename. `move.term count tally` displays as `+ tally` and `- count`. The *merge* handles a lone rename correctly — but the review surface does not group the two halves, so a reviewer sees a delete and an add. Git's heuristic, for all its flakiness, at least tries.

### 3. "A Unison codebase is never in a broken state, even midway through a refactoring" — **true of the codebase, misleading about the experience**

> "Your codebase is always live and typechecks, even in the middle of a refactoring. Unison has structured refactoring sessions, not a big misleading list of type errors."

The stored codebase is always green, because ingest is gated on typechecking. But the work in progress lives in a scratch file that is exactly as broken as a git working tree, and the gate is *stricter* than a build, not looser: `update` is all-or-nothing. On failure it ingests nothing, removes every name the file bound **plus every dependent it rendered**, and hands the union back as one non-compiling file (`04-caching-update-merge.md`, `Update2.hs:219-251`).

So the comparison is not "green codebase versus red build". It is:

- **git**: the repository holds broken code; you fix it incrementally, committing as you go, with a build telling you what is left.
- **Unison**: the repository never holds broken code; the price is that a failed refactor evicts the names you had and gives you a single large file to fix before anything can land.

"Not a big misleading list of type errors" is fair — the errors you get are real, because propagation has already happened. But the blast radius on failure is larger than a build's, and there is no way to checkpoint partial progress into the codebase.

### 4. "the diamond dependency problem [is] just not a thing" — **the strongest claim, stronger than stated in one way and weaker in three**

> "In Unison, definitions are referenced by hash … so dependency conflicts and the diamond dependency problem are just not a thing."
> "Multiple library versions can coexist, so you can grab the latest version of a library … and continue using the old."

This decomposes into two questions that Unison's materials do not separate.

**Can two versions coexist?** Yes, trivially — `lib.dep_v1` and `lib.dep_v2` are just two namespaces, no resolver, no version solving, no manifest. But coexistence alone is not distinctive: cargo already permits multiple semver-major versions, npm nests `node_modules`, Go gives each major version its own import path. Maven and pip are the systems that genuinely cannot.

**Can values flow between them?** This is the real question and Unison's own materials are silent on it. Tested `[verified empirically]`:

```unison
crossing : lib.dep_v2.Color -> Text
crossing c = lib.dep_v1.describe c   -- typechecks
```

Where `dep/v2` was branched from `dep/v1` and only `describe` was changed, `Color` is **the same type** in both, so a value crosses freely. This is the substantive advantage, and it is one **no package-granular system can offer**: in cargo, npm, Go or Maven, identity is a property of the package, so `c@1::Color` and `c@2::Color` are distinct even when byte-identical. Unison shares the entire unchanged subset of a library across versions. That is a real solution to a real problem, and Unison undersells it.

Three conditions Unison does not mention:

- **It requires shared provenance.** The sharing works because v2 was derived from v1 and `update` preserved unchanged hashes. Two *independently authored* copies of the same `unique` type do not unify — tested: `has type: a.Color but I expected: b.Color`. (`structural` types do unify, since they hash by structure.) So the mechanism is the exact analogue of git's merge base: common history is what makes it work, and a fork or a reimplementation shares nothing.
- **If the boundary type is what changed, you are back where cargo is.** The shared subset is shared; the changed part is not. When the type crossing the diamond is precisely the one that was updated, nothing crosses, and Unison offers no more than anyone else.
- **It applies only under `lib`.** Established in `08-merge-and-branching.md` §"The diamond-dependency claim": `lib` is a magic name segment, special-cased in ~20 modules, and it is the only region exempt from propagation. Regular definitions *are* propagated into and *do* conflict. The diamond claim is a property of a naming convention, not of content addressing.

And coexistence defers migration rather than removing it. `upgrade old new` is a whole-codebase rewrite that must typecheck as a unit, creates a temporary branch, and drops you into a scratch file on failure — structurally the same flow as a failed merge. The claim is about being *allowed* to postpone; the migration is unchanged when you take it.

### 5. "an entire class of conflicts can be eliminated" — **true**

> "Many dependency conflicts arise from definitions competing for the same names. Since Unison references code by hash instead of by name, an entire class of conflicts can be eliminated."

Confirmed across the conflict taxonomy in `08-merge-and-branching.md`. Things that conflict in git and cannot in Unison: formatting differences, comment edits, import/`use` line churn, moving a definition between files, two definitions edited near each other in one file, and — the sharpest case, `[verified empirically]` — **two people independently writing the identical definition**. Unison merges to `scratch/main was already up-to-date with scratch/alice`, because both authored the same hash. The same pair with different bodies conflicts as expected. *(Git conflicts here only when the text differs: git resolves add/add cleanly on identical blobs, so a canonicalizing formatter recovers this case without any hashing. See §"Could you just do this in git?". What a formatter cannot recover is α-equivalence — Unison also merges two definitions that differ only in a bound variable name.)*

Add to that the synhash rule: an update that only *propagated* into a definition is removed from the diff entirely, so a downstream definition that both sides inherited does not collide.

### 6. "no need to rerun a deterministic test if none of its dependencies have changed" — **true, and persisted, and synced**

The strongest uncontested claim. Test results are stored by hash in SQLite and travel with the codebase, so a test that passed on your machine does not re-run on someone else's. Compare CI, which re-runs everything on every push and where incremental test selection is a whole product category. Caveat from `12-state-inventory.md`: this holds for tests specifically; ordinary watch expressions are cached locally but not synced.

## What git does that Unison gives up

These are mostly not about merging, which is why they are easy to miss when comparing merge models.

1. **History has no content.** `history` lists which names appeared and disappeared per step, never what changed inside a definition. There is no `log -p`, no blame, no content bisect. You cannot see past a merge node in either direction, and `undo` refuses there. Repoint operations — `reset`, `fork`, `branch.squash`, a fast-forward merge — leave no parent edge at all, so the reflog is the only record. `13-branching.md`.

2. **The review surface is names, not code.** `[verified empirically]` `diff.branch` prints `+ added`, `~ greet`, `- doomed` and stops. To see what changed inside `greet` you `view` it on each branch and compare by eye, or configure `UCM_DIFFTOOL`, which hands you two whole-file renderings and no diff at all. For a semantic change this is arguably better than a hunk; for reviewing a one-character fix inside a large function it is much worse.

3. **Comments and formatting do not survive storage.** `[verified empirically]` — a definition written with a leading comment, an inner comment, a trailing comment and deliberate spacing comes back from `view` with all three comments gone and the spacing normalised, plus a synthesised `use Nat +`. Only `{{ }}` doc-blocks survive, because they are separate definitions. In a text VCS the comment *is* the code and is versioned, blamed and reviewed like everything else. This is the exact flip side of "no formatting conflicts", and it is a much larger cost than that win.

4. **The codebase is a database, so the tool ecosystem does not apply.** No `grep`, no `sed`, no ripgrep across your own code, no arbitrary linter or formatter, no `git add -p`, no staging area, no partial commit, no cherry-picking a hunk. Every operation must exist as a UCM command. Git's real advantage here is not the algorithm; it is that a working tree is *just files*, so forty years of text tooling applies unchanged.

5. **You cannot save broken work.** Committing something that does not compile is a feature in git — checkpointing, handing work over, bisecting to find where it broke. Unison's gate means half-finished work has nowhere to live but an untracked scratch file.

6. **Delete-versus-edit is resolved silently in favour of the edit.** The delete is discarded and nothing says so; the source calls it *"perhaps only temporarily, because it's easier to implement."* Git conflicts here, correctly.

7. **A merge can fail with zero conflicts,** if propagating an unconflicted update breaks typechecking downstream — and there is no partial application. One broken dependent blocks the entire merge. Git would let the merge land and the build fail, which is worse in one way (you have broken main) and better in another (the merge is not hostage to unrelated breakage).

## Dependency management specifically

Four regimes, on the axes that matter:

| | Resolver | Two versions at once | Unchanged types shared | Upgrade cost |
|---|---|---|---|---|
| **Manifest + solver** (npm, cargo, pip, maven) | global constraint solve | cargo/npm yes, pip/maven no | **never** — identity is the package | bump + fix, all at once |
| **Vendoring** | none | yes, by copying | never | manual textual re-merge |
| **Go modules (MVS)** | deterministic selection | across majors only | never | bump + fix |
| **Unison `lib`** | **none at all** | yes, freely | **yes**, on the shared-provenance subset | `upgrade`, all at once |

The useful framing: **`lib` is vendoring with content-addressed deduplication.** `lib.install` grafts a namespace subtree into your project, exactly as vendoring copies source in — your project is self-contained and there is no fetch-and-resolve step at build time. What vendoring cannot do, and Unison can, is share the parts that are actually the same. Two vendored copies of a library share nothing; two Unison versions of a library share every definition that did not change.

What Unison gives up on this axis:

- **No version constraints.** There is no way to express "I need base ≥ 2.1", and nothing checks compatibility. You install a namespace and find out by typechecking.
- **No lockfile and no manifest**, so no dependency audit surface: no way to ask "which of my dependencies has a known vulnerability" in the terms the CVE ecosystem uses (package + version). Hashes are in principle a *better* identifier for this, but no tooling consumes them that way.
- **Duplication is permanent and unsignalled.** If two libraries pull in versions of a third whose types diverged, both live in your codebase forever, and nothing tells you it happened.

## Could you just do this in git?

A natural challenge to everything above: suppose you keep git, but put **every top-level definition in its own file** with the path mirroring the namespace, and add a **pre-commit hook that pretty-prints canonically and strips comments** — Unison's round-trip, imposed on a text repo. How much of the advantage does that recover?

Answer, from running it (`[verified empirically]`, git 2.x, `ort` strategy): **splitting files buys conflict granularity; hashing buys identity.** The proposal is the first without the second, and the line between them is sharp.

### What it recovers — nearly the whole conflict story

Formatting, comment edits, import churn, moving a definition, two people editing "adjacent" code: all gone, because none of them are objects any more.

Including one this document previously credited to hashing. Two people independently authoring the identical definition merges cleanly in git — `Merge made by the 'ort' strategy` — because git resolves add/add when the blobs match. So §5's sharpest example is really *canonicalization* doing the work, not content addressing. The hook recovers it. **Corrected 2026-09-03.**

And the proposal gets one property Unison has to work for: **propagation is free and invisible.** Change `add`, and `double.u`'s bytes do not move — no diff, no churn, and history stays legible. Unison re-hashes every dependent and then needs the syntactic hash to *reconstruct* the same effect. Name-based reference has it natively; the cost is that it is unverified.

### What it cannot reach

The crux: the proposal makes the **layout** match the namespace, but the file **contents** still reference by name.

- **Rename is still O(dependents), and still conflicts.** Renaming `add` → `plus` in a four-definition repo touched four files. Then a *pure rename* on one branch against an *unrelated edit to a dependent* on the other:

  ```
  CONFLICT (content): Merge conflict in math/double.u
  <<<<<<< HEAD
  double x = x * 2
  =======
  double x = plus x x
  >>>>>>> alice
  ```

  Both sides did something legitimate and disjoint. Unison merges this clean — the rename touches a binding, the edit touches a definition, and they do not intersect. This is §2's win, and the file split cannot touch it.

- **Cross-version type sharing is entirely lost.** `lib/dep_v1/Color.u` and `lib/dep_v2/Color.u` holding identical bytes are still two declarations at two paths, hence two types in any nominally-typed language. §4's result needs identity *derived from content*, which is the one thing the proposal does not add. Note the symmetry with the `structural`/`unique` finding: structural types unify across independent authorship precisely because their identity already is content.

- **α-equivalence marks where canonical text stops being canonical program.** `other y = y + 1` versus `other z = z + 1` is an add/add conflict. A formatter will not α-normalize, and should not — binder names are for humans. Small in practice; it is the boundary.

- **Propagation is unverified.** Git does not know the dependents still typecheck. You keep git's behaviour, which §3 argues is *better* for incremental work, but it is not the "always green" property.

- **Test caching is recoverable, but not by git.** It needs a content-addressed cache over the transitive closure of inputs, which is Bazel. Proposal + Bazel ≈ §6, at definition granularity since the files *are* definitions. One gap: the input hash includes names, so renaming a local in a dependency busts the cache where Unison's would not.

### The proposal's own costs

The file-per-definition half has been shipped and withdrawn. Pharo's **FileTree** is a file-per-*method* format built so git can version Smalltalk at method granularity; the community moved to **Tonel**, file-per-*class*, because file-per-method meant too many files, wasted space (most methods are short, against a 4 KiB minimum block), and Windows path-depth trouble. That is the field verdict on file count, from the one community that ran it for years. `[not peer-reviewed — project documentation]`

Two more. A pre-commit hook that rewrites what you staged means committing something you did not review — this wants a clean/smudge filter or format-on-save. And stripping comments is more destructive than Unison is: Unison at least keeps doc-blocks as separate definitions, so the honest version is a sidecar (`foo.u` plus `foo.doc`), which is arbor's aspect store rediscovered from the other end.

One place the proposal beats Unison outright: delete-versus-modify becomes a real git conflict rather than §"gives up" item 6's silent discard.

### Where it turns into Unison

Push it one step. Make the **stored** form reference by hash and render names on checkout — clean filter on `git add`, smudge on checkout. Renaming becomes an edit to the name map alone, and the cross-version sharing falls out because identity is content.

The conflict does not disappear; it **moves** to two hashes competing for one name in that map — which is exactly Unison's conflict, in exactly the right place. At that point the working tree is a rendering and the repository is a store, and you have built Unison with git's history underneath it.

That last sentence is the interesting one for arbor, and it is the converse of the finding above. Unison's costs cluster in observation; git-with-one-file-per-definition gets the observation for free and misses the identity. The target is the union, and the smudge/clean sketch is evidence that the union is a build order rather than a contradiction.

## Could you just use GUIDs?

The follow-up challenge to the previous section, and a sharper one: keep the file-per-definition git repo, but have definitions reference each other by **GUID**, with one table mapping GUID → human-readable name. Renames become a single row edit. What does content addressing buy over that?

Less than the marketing implies. Two of the arguments made earlier in this document do not survive it.

### What the GUID system gets outright

Free renames, exactly as cheap as Unison's — one row, no dependents touched, nothing to conflict. Free moves between namespaces. No import churn. Plus everything the file split already bought in the previous section.

### Withdrawn: cross-version type sharing is not unique to content addressing

§4's result — `dep_v1.Color` and `dep_v2.Color` being the same type — is reachable here. Have the compiler content-hash the AST and use *that* for nominal type identity, and identical content unifies regardless of GUID.

One sharpening, which is a concession rather than a rescue. The compiler must hash **transitively through** the GUIDs — replacing each reference with the hash of what it points at, recursively — because hashing a GUID only hashes an assignment, and two independently forked libraries with different GUIDs would give different hashes for identical content. That transitive hash *is* a content address; the design rebuilds it inside the compiler rather than avoiding it. But it works, and the property is not an argument for exposing the hash as the stored identity.

### Withdrawn: verification and shareable caches are not unique either

Nix is the existence proof: named source files, a content-addressed store, and a remote cache that is safely shareable between mutually distrusting machines precisely because the key is a content hash. Add lockfiles carrying integrity hashes and artifact-level verification follows. The residual difference is *granularity* — definition-level versus artifact-level reference — and who maintains the manifest. Real, but small.

### The strongest argument is on the GUID system's side, and it is not one its proposer made

**In the GUID system there is no propagation.** `usesDep.u` holds `ref(g_dep)`; change `dep` and `usesDep.u`'s bytes do not move. Nothing cascades, nothing is re-hashed, so **there is nothing to classify** — no syntactic hash, no propagated-versus-authored distinction, and therefore none of the rename fragility documented in `08-merge-and-branching.md`.

That apparatus exists *only because content addressing forces the cascade*. A dependent's hash contains its dependency's hash, so an edit at the bottom of a graph rewrites the identity of everything above it, so merge must reconstruct which of those rewrites a human meant. The GUID system never incurs the debt, so it never pays the interest.

| | Unison | GUID indirection |
|---|---|---|
| Dependent's identity when a dependency changes | forced to change | unchanged |
| Therefore | must classify propagated vs. authored | nothing to classify |
| Cost of that | synhash; renames break it | — |
| What it buys | dependents are known to typecheck | the build finds out |

The residual trade is exactly the one from §3: *the codebase always typechecks* versus *you find out at build time*. Which this document has already argued is not obviously a win for Unison.

### What survives

**1. Convergence of independent authorship — genuinely unique, and narrow.** Two people who write the same definition in different repositories get the same hash. GUIDs cannot ever unify them, and no amount of merging will.

The bound matters more than the property. Measured `[verified empirically]` — seven ways to write "add one", added together, then `names a1`:

```
#t7g50rohbm   Term   a1, a2, a4, a5, a6
```

| variant | | |
|---|---|---|
| `a1 x = x + 1` | baseline | — |
| `a2 y = y + 1` | α-renamed | **same** |
| `a4 x = (Nat.+) x 1` | operator desugared to prefix | **same** |
| `a5 = x -> x + 1` | lambda instead of equation | **same** |
| `a6 : Nat -> Nat` + same body | signature written vs. inferred | **same** |
| `a3 x = 1 + x` | arguments commuted | **different** |
| `a7 x = (y = x + 1; y)` | intermediate named | **different** |

The equivalence is **syntactic identity modulo desugaring and binder names**. Commuting a commutative operator breaks it; naming an intermediate breaks it. Two people converge only if they wrote literally the same expression tree.

A second bound is harsher: two independently declared `unique type`s never converge, and Unison is unique-by-default for types. Any definition mentioning a locally declared type is permanently disqualified. Convergence is therefore confined to code written over *already shared* vocabulary — builtins and library types both parties imported.

So the dividend is real and concentrated in the small-combinator tail. It is a nice property, not a foundation. Convergence at a *semantic* rather than syntactic level is a normalization problem (canonical forms, e-graphs); content addressing offers no help with it, only a cheap equality test once normalization has happened.

Note also that Unison does not market this. The FAQ presents it as a consequence — *"Definitions never change, but the names we give them do"* — and it is absent from the six advertised claims audited above.

**2. No preservation obligation.** Someone must mint a GUID and every tool must keep it, through copy-paste, forks, and refactors. "Does a fork keep upstream's GUIDs?" has no principled answer: keep them and the fork claims to *be* upstream; drop them and nothing is shared. Hashes never ask, because the answer is derived from the bytes.

The evidence is inside Unison. It *has* GUIDs — `unique type` mints one — and that is the part needing scaffolding. To survive a re-save the GUID is recovered **by name** (`UniqueTypeGuidLookup.loadUniqueTypeGuid`), whose docstring says *"If there are multiple such types, an arbitrary one is chosen."* Plus a side table, also keyed by name:

```sql
create table namespace_unique_type_guid (
  namespace_hash_id integer not null references hash (id) on delete cascade,
  type_name         jsonb   not null,
  type_guid         text    not null
);
```

In the one place Unison uses assigned identity it needs a side table, a name-based recovery path, and an arbitrary tie-break. That is the GUID design's operational cost, measured inside the system being compared to.

**3. Durable reference.** A hash written down means the same thing forever, to anyone, offline. A GUID means "whatever that slot holds now," so any durable reference needs a GUID *plus* a version — which is a manifest, which is another thing to maintain, resolve, and merge. This is probably the strongest remaining argument, and it is not one Unison makes.

### They are duals, and neither is a superset

| | identity derived from | unifies independent equals | survives an edit |
|---|---|---|---|
| **Hash** | content | **yes** | no |
| **GUID** | assignment | no | **yes** |

"Which definition is this the successor of?" is unanswerable from hashes — an edit produces an unrelated hash. Unison answers it *by name*, which is precisely why its merge classifier breaks on renames. A GUID answers it directly, and a GUID-based synhash would be rename-proof.

Every system that needs both ends up carrying both: Unison has hashes plus unique-type GUIDs, Nix has a content-addressed store plus attribute names, and arbor's own line arrived at the same place deliberately — p10 minted everything, p11 layered mint *threads* over hashes.

## What arbor should take from this

Recorded as findings, not decisions — see `feedback_findings_not_decisions`. **Revised 2026-09-03**: the two hypotheticals above withdrew two arguments this list previously rested on, so items 1–3 replace a single earlier claim that cross-version sharing was the headline dividend of `Σ`.

1. **The case for a content-addressed `Σ` is durable reference and the absence of a preservation obligation — not the advertised claims.** A hash written down means the same thing forever, to anyone, offline; a GUID means "whatever that slot holds now," and every durable reference to one needs an accompanying version, i.e. a manifest to maintain and merge. And nobody has to *keep* a hash through copy-paste, forks, or refactors. Unison's own unique-type GUIDs show what that obligation costs: a side table, a name-based recovery path, and an arbitrary tie-break. These two arguments survive every hypothetical in this document; the flashier ones do not.

2. **Cross-version type sharing is real but is not an argument for `Σ`.** It is reachable in a GUID system whose compiler hashes transitively through the references (§"Could you just use GUIDs?"). It is also conditional on shared provenance — the mechanism is a merge base by another name — and `docs/design/` should not repeat "no diamond problem" without that condition. Still worth having; just not load-bearing for the identity model.

3. **Convergence of independent authorship is the one genuinely unique property, and it is narrow.** Measured: the equivalence is syntactic identity modulo desugaring and binder names. Commuting a commutative operator breaks it; naming an intermediate breaks it; and two independently declared `unique` types never converge at all, which disqualifies anything mentioning a locally declared type. The dividend is concentrated in the small-combinator tail. Treat it as a pleasant consequence, not a foundation — and note that Unison itself frames it that way. Semantic convergence is a normalization problem that content addressing does not address.

4. **Content addressing *creates* the propagation cascade, and the cascade is what needs the syntactic hash.** A dependent's hash contains its dependency's hash, so an edit rewrites the identity of everything above it, so merge must reconstruct which rewrites a human meant — via a name-substituted hash that then breaks on renames. A GUID design never incurs the debt. Budget for this: any arbor design with content-addressed references inherits the classification problem, and needs a better answer than names. p11's mint threads are one such answer.

5. **Mints are not redundant with hashes; they answer the question hashes cannot.** "Which definition is this the successor of?" is unanswerable from a hash — an edit produces an unrelated one. Unison answers it *by name*, which is exactly why its merge classifier is rename-fragile. p10/p11's minted identity and `project_cross_edit_identity`'s lean toward mint-survives-edits are doing genuine, non-overlapping work: hash for identity, mint for continuity, kept separate.

6. **`lib` is a warning.** Unison's exemption from propagation is a magic name segment special-cased in ~20 modules. arbor's update strategies (`project_update_strategies`, p11's pin/follow/explicit) are the same distinction made first-class. That is the right shape and it should stay first-class.

7. **The costs cluster in *observation*, not in merging.** No content history, no blame, no line diff, no grep, no comments. arbor's `Θ` aspect store can hold what Unison discards — comments and authorial formatting as aspects on a hash rather than bytes in it — which keeps "no formatting conflicts" without losing the comment. Worth an entry in `docs/design/open-questions.md`.

8. **The commit gate's real cost is visible here.** "Never broken" and "cannot checkpoint broken work" are the same property. `13-editing-context.md`'s position — ingest everything, uncommitted is *names* not definitions — buys back git's ability to save a half-finished thought, and p21 exists to make that trade concrete.

9. **The union is a build order, not a contradiction.** The two hypotheticals isolate which half of Unison's advantage comes from *file granularity* (recoverable in git today), which from *assigned identity* (recoverable with GUIDs), and which from *content identity* (items 1 and 3). The halves are independent, so observation-layer work — content history, per-definition diff, comments as aspects — does not have to trade against `Σ`.
