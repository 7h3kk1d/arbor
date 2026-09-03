# p21 — Decisions (ADR-lite)

Append-only. Reversals get new dated entries. Substrate-design commitments that generalize beyond
this prototype migrate to `docs/design/`; entries here are prototype-scoped.

Note that p21 is a *model of another system*. Most decisions below are "be faithful", and the ones
worth recording are the places where being faithful required a choice the source does not force.

---

### 2026-09-02 — Faithful to Unison, including where arbor disagrees

**Decision.** Implement Unison's model as `mini-unison.tex` states it, including the parts arbor has
recorded the opposite position on: the commit gate, the relation-valued namespace, the type inside
the hash, name-dependent component identity, render-and-reparse migration, and evict-on-failure.

**Why.** The alternative — take the component hashing and keep arbor's permissive ingest and
single-valued `N` — produces a nicer editor and a much weaker experiment. The point is to find out
what Unison's choices cost by paying them, and each divergence is a row in the findings table. A
hybrid would have no control group.

---

### 2026-09-02 — Store binder names; do not use de Bruijn

**Decision.** Terms are a named ABT. Binder names are stored and erased only inside `Abt.hash`.

**Why.** Forced by `update`. Step 2 renders every dependent back to source that a human reads and
edits, so the names in it have to be the author's. Every arbor prototype from p4 on stores de Bruijn
indices and invents display names; such a system could state its round trip only up to a choice of
names, and would hand the user back their own code with someone else's variables in it
(`rem:names`). This is the clearest case in the series of a *surface* requirement dictating a
*storage* representation.

---

### 2026-09-02 — SHA3-512, not BLAKE2B

**Decision.** Use SHA3-512 over a tagged, length-prefixed token stream with a leading version token,
via `digestif.ocaml`.

**Why.** It is Unison's, and for once nothing stops us: the pure-OCaml backend has SHA3-512 and works
under js_of_ocaml, which is the constraint that forced `digestif.ocaml` in the first place. Using the
modelled system's own hash function removes a whole class of "is this a real difference or an
artifact of our encoding?" question from the findings. Byte-compatibility with real Unison remains a
non-goal — the surface language's encoding is not modelled — so the hashes still will not match
`ucm`.

---

### 2026-09-02 — Split stanzas by layout in the driver, not in the grammar

**Decision.** `Parse.file` chunks the buffer into stanzas before Menhir sees them: a top-level stanza
begins at column 0, continuation lines are indented. The grammar parses one stanza and stays
newline-insensitive.

**Why.** A newline-insensitive file grammar is genuinely ambiguous — after `> f`, an `IDENT` may
continue the application or begin the next stanza, and Menhir reports the shift/reduce conflict.
Unison solves this with layout, and doing the same keeps the grammar conflict-free (it is) and gives
per-stanza error reporting, which the buffer's live feedback wants anyway. The alternative — a
layout-aware lexer emitting virtual separators — is more faithful in mechanism and buys nothing here.

---

### 2026-09-02 — Every binding needs a signature

**Decision.** A buffer stanza `name = e` without a matching `name : T` is a problem, reported and
not ingested.

**Why.** The language has no inference, so a recursive occurrence's type has to come from somewhere,
and the declared signature is the only candidate: `Ingest.typecheck_group` seeds the context with
every member's declared type before checking any of them. It also keeps `prop:print-parse` honest —
a definition rendered without its signature would not re-typecheck, and the round trip would fail for
a reason that has nothing to do with references. Unison infers, and would not need this.

---

### 2026-09-02 — Live feedback simulates ingest rather than querying it

**Decision.** `Ops.feedback` re-runs parse → resolve → typecheck over the whole buffer on every
keystroke, against a context seeded with the group's declared types. It stores nothing and caches
nothing.

**Why.** Under a commit gate there is nothing else it *can* do: a buffer definition has no hash, so
there is no object to attach feedback to and no key to cache it under. This is worth recording
because it is the concrete cost of the gate, and it is the strongest evidence the prototype produces
for `13-editing-context.md`'s claim that permissive ingest is what makes live editing cheap. The
first version of this function checked each binding in an *empty* context and reported every
co-dependent edit as an unbound variable — a bug the browser pass caught and the native tests did
not, since the tests went through `Ingest` and never through this path.

---

### 2026-09-02 — One commit gesture

**Decision.** The buffer has a single `commit` button, wired to `Update.run`.

**Why.** At `db60ce2`, `add` is an alias for `update` (`InputPatterns.hs:882-896`); `Input.AddI` no
longer exists, merged in 2025. Two buttons would model a distinction the system stopped making. This
also corrects `prop:follow-only`'s description of `add` as the abstention gesture — abstention is
choosing a name nothing depends on, not choosing a different command.

---

### 2026-09-03 — Namespace gestures, with the precondition the equations do not show

**Decision.** `Names.move`, `Names.alias` and `Names.delete` (`def:gestures`), the three operations
that change `N` and nothing else. `move` and `alias` return a `result` and refuse three cases:
source unbound, source conflicted, **target already bound**. `delete` is total.

**Why the third case.** The paper writes `move(x,y)` as `N' = (N \ {(x,r)}) ∪ {(y,r)}`, and a
relation would hold that result perfectly well when `y` is already bound — the union just makes `y`
conflicted. `ucm` 1.1.1 refuses: *"A term named bar already exists."* So conflict stays
**representable** while no gesture here can produce one, which is a third instance of `rem:conflict`'s
pattern after `def:unconf`'s all-or-nothing view and `update`'s precondition. Modelling the set
equation literally would have made the prototype able to reach a state the real system cannot.
`def:gestures` in the paper now carries the same note.

**What the pair of tests is for.** `test_gestures` pins the preconditions. `test_renamefree` pins
`rem:renamefree`, which is the reason renaming is usually offered as the payoff of content addressing:
rename a callee out from under its caller and the caller's referent, the caller's hash, the store and
the cached result of evaluating it are all bit-for-bit what they were. Nothing below the interface can
observe that a rename happened. That test exists to be *contradicted* by the merge section, which is
the one operation that reads names as though they were content.

**Not reachable here: `rem:innercycle`.** That finding is about `hash_component` having two callers
who treat the ordering warning differently. p21 has one, because `def:urterm` has no `let`, so no
inner cycle is expressible. Declining to add let-rec purely to acquire a second call site; noted in
`open-questions.md`.

---

### 2026-09-03 — Committable from `Typechecked` onwards, not *at* `Typechecked`

**Decision.** `Scratch.is_committable` compares stage *rank* (`stage_index(s.stage) >=
stage_index(Typechecked)`) rather than testing `s.stage == Typechecked`.

**Why.** The equality test was a regression introduced by the save/commit split of 2026-09-02, and it
disabled the commit button in exactly the state where committing is possible. `analyze_only` stops at
`Typechecked`; `save` carries a well-typed buffer one step further to `Hashed`; and the editor runs
`save` on every keystroke, because typing *is* saving (`rem:saveident`). So no buffer a user could
actually produce ever satisfied `== Typechecked`. The rail read `hashed`, every binding showed a hash
at save, and the button underneath said "nothing is hashed until the whole buffer typechecks."

**Why the suite stayed green.** Both halves were already asserted, in different tests.
`test_save_is_not_commit` checked that a save reaches `Hashed`; the commit gate was described as
`Typechecked` in `scratch.re`. Nothing put the two in one place. `test_committable_after_save` now
does, and asserts the negative as well, so the gate is still shown to hold on an ill-typed buffer.

**Second instance of a pattern worth naming.** The 2026-09-02 entry on live feedback records a bug the
browser pass caught and the native tests did not. This is the same shape: the defect is in a predicate
that only the interface calls, and the substrate tests reach `Update.run` directly. Any p21 test that
never renders cannot see this class of defect, and `project_browser_verification` is the standing note
about it — `--dump-dom` would not have caught this either, since the button renders in both states and
only the `disabled` attribute differs.

---

### 2026-09-03 — Merge: three namespaces, one store

**Decision.** `src/synhash.re` and `src/merge.re`. `Merge.run` takes `~st` and three `Names.t` and
returns `Refused | Merged | Unmerged`.

**Why one store.** `prop:merge-store`: two stores grown independently from a common ancestor union
without reconciliation, because equal hashes mean equal components up to the identification
`comphash` makes and closure under references survives union. So a branch is a namespace and nothing
else, and the signature says so. It also makes the point the paper wants to make — that the property
content-addressed codebases are sold on is real and nearly vacuous — impossible to lose: there is no
code here that reconciles stores, because there is nothing for it to do.

**Why `Synhash` is a separate module with its own tag space.** It is `Abt.hash` with one clause
reversed, and the temptation is to parameterize `Abt.hash` over the reference case. Keeping them
apart makes the duality legible (`rem:syn-dual`) and makes it impossible to compare a syntactic hash
against a content hash by accident, which is the one confusion that would silently break the
classifier.

**`Names.preferring`, added for this.** Left-biased union on the *range*: for each referent, if the
left namespace names it at all, ignore the right's names for it. It is the operator `def:namer` is
built from, and it is deliberately next to `shadowing` in the same file, because the two answer
different questions — `shadowing` asks who else binds this *name*, `preferring` asks who else names
this *thing* — and the difference is the mechanism behind every rename failure in §7.

**What the tests are for, and what they caught.** All eight name a paper statement. Two of them
caught defects that the label check could not: the first `diff` dropped propagated updates instead of
partitioning them, so every clean merge lost half its result; and the first dependent computation
seeded `Store.transitive_dependents` with the very definitions it was trying to find, which that
function excludes by design (it drops the seeds' own component members). Both are recorded in
`formalism/decisions.md` as corrections to the paper, because in both cases the paper said the wrong
thing first and the implementation is what disagreed.
