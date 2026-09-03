# Phase 21 — Unison's model, on the paper's language

**Status:** Written after the prototype landed; the questions below were answered by building it.
**Design context:** `../../../formalism/paper/mini-unison.tex` (the model this executes), `../../systems/unison/02-store-and-hashing.md`, `../../systems/unison/04-caching-update-merge.md`, `../../systems/unison/07-arbor-mapping.md`, `../../systems/unison/11-scratch-file.md`, `../../design/13-editing-context.md` (the scratch buffer as an editing context), `../../design/03-content-addressing.md` §"Mutual recursion" (this is Option A, built).
**Prototype design lives here:** `docs/prototypes/p21-unison-model/`.
**Implementation code lives at:** `prototypes/p21-unison-model/`.

**Thesis.** `mini-unison.tex` states twenty-nine things about Unison. Every one of them is a
property something can be *run against*. p21 is that something: Unison's model — component hashing,
references as `(hash, position)` pairs, the type inside the term hash, a namespace that is a
relation, a watch cache with no invalidation, and update-by-render-and-reparse with
evict-on-failure — implemented faithfully over the paper's own object language, with a scratch
buffer that holds several definitions and their name bindings.

It is **not** an arbor prototype in the usual sense. It does not extend the p12–p17 type-abstraction
line and it contradicts several of arbor's recorded positions on purpose. The divergences are the
output.

## Why build it rather than read the source again

A citation audit of the paper (`formalism/decisions.md` 2026-09-02) resolved every `\src{}` tag
against Unison at `db60ce2` and found five wrong claims. Four were the kind an audit catches:
behaviour described from a plausible reading of code that had genuinely been looked at. **One was
not.** `ex:ambiguous` asserted that renaming `ping` to `zing` changes the component's hash — an
arithmetic claim about the paper's *own* `def:comphash`, which no amount of reading Unison could
settle, and which a worked example got wrong precisely because it was written to *illustrate* a
theorem rather than to test one.

Running it settles it in a second. That is the argument for the prototype, and it generalizes: the
paper and the code are now a checked pair.

## Answers the build produced

**`rem:transparency-limit` is right, and three arbor documents were wrong.** A position-0 reference
contributes the referent's *component* hash; inlining would contribute its *term* hash; `def:comphash`
puts two accumulations between them even at *k* = 1. So `x = 1 + 1` and `y = x` receive different
hashes, and inlining a reference into a larger term does not preserve that term's hash. The paper
listed this as the claim it would most like checked. `02-store-and-hashing.md`, `07-arbor-mapping.md`
and `design/03-content-addressing.md` all still asserted the opposite and have been corrected.

**`thm:ordering`(b) splits in two, and the paper had only the weaker half.** When the permutation
hash is not injective:

- **(b1), always:** which member receives which `Pos` depends on the members' *names*. Renaming can
  change what `Derived(h, 0)` denotes, and hence which member of a cycle is referenced transparently.
- **(b2), in general but not for every tie:** the component hash itself depends on the names.

`ping`/`pong` exhibits (b1) and *not* (b2) — the last step of `def:comphash` accumulates the `nh`
values **sorted**, so a rename that merely permutes that multiset leaves the hash fixed. A witness
for (b2) needs members that mention the cycle asymmetrically; `foo = bar = Nat.+ (foo 0) (bar 0)`
does it. Both are tests.

**`rem:twoorders` is observable, and the asymmetry it produces is real.** Committing
`countDown`/`countUp` — written in that order, and in that order alphabetically — produces a
component in which **`countUp` holds position 0**. The user wrote two definitions symmetrically; one
of them is referenced transparently and the other opaquely, and nothing in the surface syntax says
which.

**Watches exposed a missing proposition, not just a missing feature.** The first version of this
prototype evaluated watch expressions for display and threw them away — they never reached ingest,
so they were never hashed and never cached. Fixing it against the source (`allTerms = join tlcs ++
join (snd <$> watches)`, under a comment reading "includes watches") showed that the *paper* had the
same gap: it described a cache whose key "need not be a definition" without modelling anything that
produces such a key. The paper now carries `def:ufile` (a file is a binding group plus watches) and
**`prop:watchdom`** (`dom(W) ⊄ dom(Σ)` is reachable), and `rem:watchdom-ok` records why
`thm:wstable` survives it: its proof never used that the key is in the store, only that everything
the key *reads* is. Confirmed in real Unison — a codebase with one evaluated watch interns the
watch's hash and has no `hash_object` row for it.

**`prop:print-parse` caught a bug on its first run.** The printer omitted parentheses around an
application in operand position, so `f (g x y)` printed as `f g x y`. Unison maintains this property
with a golden-file corpus named `reparses-with-same-hash.u`; over a language this small it is a
qcheck property, and it found the defect immediately. This is the arbor-side argument for
`10-followups.md`'s "adopt the round-trip corpus as an executable check" — a property is cheaper
than a corpus and strictly stronger, *at this scale*.

## The scratch buffer, and its own store

The buffer's state is a **separate module** (`src/scratch.re`), deliberately not part of `Store`.
That is the model rather than tidiness: Unison's editing context is two fields of an in-memory
record (`latestFile`, `latestTypecheckedFile`), it is a `Maybe` rather than a set, it holds **no
hashes**, and it dies with the process. Putting it in the Store would suggest a buffer definition is
a stored one, which is the single thing this prototype exists to say it is not. `Scratch.t` is a
cache in the ordinary sense — recomputable from the text, discardable at any moment.

Unison's `Either UnisonFile TypecheckedUnisonFile` is opened out into four stages —
`Unparsed → Parsed → Resolved → Typechecked` — and a buffer stops at the first one that fails.
That is what the UI's stage rail reads.

A textarea holding several `name : T` / `name = e` pairs, watch expressions (`> e`), and a `---`
fold below which everything is inert. Re-parsed and re-typechecked every keystroke, with per-binding
status. One commit gesture, not two — at `db60ce2` Unison's `add` is literally an alias for `update`;
it is disabled until the whole buffer typechecks, which is the commit gate made visible.

### Save is not commit

**Typing is saving.** Every keystroke runs the full save: typecheck, hash the whole buffer including
its watches, evaluate the watches, write results to the cache on a miss. `commit` is a separate
gesture that stores and binds names.

This was wrong in the first version of the prototype, and wrong in the paper, in the same way. Both
had identity conferred at commit. Unison's save path (`loadUnisonFile` → `evalUnisonFile`) typechecks,
hashes, evaluates watches and calls `putWatch` — `only when (not isHit && cache)` — with no
`addDefsToCodebase` anywhere. So identity comes from **typechecking**; storing and naming are later.
`Ingest.hash_components` gained `~register`, which is deliberately the only difference between the two
paths, and a test asserts they agree on the hash.

The gate is still the largest divergence from arbor, but it bites earlier than recorded: a buffer that
does not typecheck has nothing to talk about *at all*, not merely nothing stored.

**The cache is observable, not just fast.** Per watch, the UI shows the reference it got at save and a
`cache hit` / `evaluated` badge; the scratch-store view counts expressions actually evaluated this
save. So the sequence that matters can be watched happening:

| | |
|---|---|
| first save of `> Nat.* 7 6` | key `#19acf8e608`, **evaluated**, 1 expression ran |
| add an unrelated comment | same key, **cache hit**, 0 ran |
| change it to `> Nat.* 7 7` | key `#dcac1b1f1d`, **evaluated**, 1 ran |
| change it back | key `#19acf8e608`, **cache hit**, 0 ran |
| a watch over an *uncommitted* definition | evaluates, via an overlay carrying the buffer's own definitions |
| after all five saves | **namespace empty, zero components** |

The fourth row is `cor:wnoinval` interactively: nothing invalidated the first entry, and nothing
could have. The last is the whole point — five saves, no commits, an untouched codebase.

### Making the pipeline visible

The editor shows each stage's output rather than only its verdict, because in this model the route
matters more than the result. Per binding: the resolved term, the variables resolution deliberately
left free, and **one chip per variable occurrence naming which of five routes it took** —
`Local`, `Group`, `Builtin`, `Resolved`, `Unresolved`, `Ambiguous`.

That last part is the point. **Three of those outcomes produce an identical output node.** A group
member awaiting its hash, a name that resolves to nothing, and a name that resolves to too many
things are all just `Var(x)`, and nothing downstream can tell them apart — `def:uresolution`'s
partiality is invisible in its own output. So `Resolve.term_traced` returns the decisions alongside
the term, and the UI colours them. An editor that says "this stayed a variable" without saying
*which* of the three reasons applies is hiding the interesting half of the definition.

The right pane shows **the watch cache itself** — every entry as a reference and the value it points
at, with the ones the current buffer references marked. Not a summary: the cache's two properties are
only visible as *contents*.

```
watch cache            4 entries · nothing evaluated this save · hashed
  #19acf8e608111341bab7  →  42        ← in buffer
  #0d446496ce94b9aefa45  →  5
  #8e13a7f6a6c0b98b7a77  →  "abcd"
  #dcac1b1f1d84adb92b61  →  49
```

The unmarked rows are expressions the buffer no longer contains. Nothing removed them and nothing
can — the key *is* the expression, so a stale entry is unreachable rather than wrong. Editing a watch
away and then back gets a hit on a row that spent a save orphaned, with `nothing evaluated this save`
next to it. That is `cor:wnoinval` as data rather than as an argument.

An earlier version of this pane was a block of scratch-state statistics — line counts, binding and
component counts, decisions grouped by route. It was replaced because almost none of it was
load-bearing: the counts restate what the editor pane already shows per binding, and the route
summary duplicated its chips.

The buffer also previews the **SCC decomposition** before commit — which definitions would share a
hash, and how many positions the component would have. That is the one structural consequence a user
cannot read off their own text, and it stops being actionable the moment it is committed.

The interesting behaviour, all driven in a real browser (below):

- **Co-dependent edits work with no gesture**, because every buffer name shadows the codebase
  (`Names.shadowing`). If `f` calls `g` and both are in the buffer, `f`'s mention of `g` stays a
  variable and is discharged by ingest against the *new* `g`.
- **A successful update rebuilds the dependents.** Changing `scale` rebuilds `double`, whose source
  text is character-for-character unchanged and whose hash changes anyway — because the printed
  *name* resolved against the new namespace. The old definitions stay in the store, orphaned.
- **A failed update evicts.** Changing `scale`'s type so `double` breaks removes **both** names from
  the namespace — not just the broken one — ingests nothing, and writes `F ∪ S` back into the buffer
  under Unison's own banner. The buffer becomes their only home.
- **The ambiguous ordering aborts.** `ping`/`pong` is refused with the structurally-equivalent set
  named.
- **Watches are members of the binding group, and are stored differently.** `> e` is hashed in the
  same call as the definitions, so it may mention a binding the buffer has not committed yet — and
  then it is deliberately *not* registered in the store and never named. Only its value is kept, in
  the cache, at the reference ingest just computed.

## Substrate deltas from every prior arbor prototype

1. **Terms are a named ABT, not de Bruijn.** p4–p17 store de Bruijn indices and invent display names.
   Here binder names are stored and erased only while hashing. This is forced rather than stylistic:
   `update` renders stored definitions back to source a human edits, so what comes back has to carry
   the author's variable names. A de Bruijn store would hand the user their own code with someone
   else's variables in it (`rem:names`).
2. **`Σ : Hash ⇀ Component`.** A definition is a *pair*. This is `03-content-addressing.md`'s Option
   A, which the design docs lean away from, built so the cost is visible.
3. **SHA3-512, and the real token discipline.** Not BLAKE2B. `digestif.ocaml`'s pure-OCaml SHA3-512
   works under js_of_ocaml, so for the first time in the series the prototype's hash function is the
   modelled system's. Tag byte, `word64BE` length prefixes, a leading version token, per-layer
   domain separation. *Not* byte-compatible with real Unison: the surface language's full encoding
   is not modelled.
4. **The watch cache is `(Hash × Pos) ⇀ Term`**, per the audit's correction — the key is a reference,
   and the value is the result term held inline rather than a hash into the store.

## Deliberate divergences from arbor's recorded positions

Each of these is a place p21 does the opposite of what arbor has written down, on purpose.

| arbor's position | p21 |
|---|---|
| Ingest everything; what is uncommitted is *names* (`13-editing-context.md`, **settled**) | Identity is conferred by typechecking. A buffer definition has no hash |
| Type is a derived aspect; the store holds ill-typed and holey terms (p8/p9, `feedback: visible breakage`) | The type is inside the hash. Holes are not expressible: there is nothing for an incomplete term to be the identity of |
| Names never appear in stored programs (`decisions.md` 2026-04-20) | A tie makes position assignment name-dependent, and usually the hash too |
| `Σ : Hash ⇀ Node`; close cycles with a fixpoint (options B1/B2) | `Σ : Hash ⇀ Node⁺`, Option A |
| `N` is single-valued (`13-editing-context.md`, 2026-08-20) | `N` is a relation; conflict is representable, and `update` refuses it |
| Callers pin automatically; no silent breakage (p3/p4/p7/p8 each carry a test) | Follow only, scope fixed. And a failed update *silently removes* your bindings |
| The residual is a set of names; commit and report | The residual is source, delivered into the buffer |
| Migration is a structural rewrite; no round-trip obligation | Migration goes through surface syntax, and the round trip is load-bearing |

## What the divergences taught

**The commit gate is the whole difference, and it is not free.** `13-editing-context.md` argues that
arbor's permissive ingest is what makes gate-free live editing sound — "everything is already
committed to the store the moment it is typed, so nothing needs to be gated." p21 is the control
group. What it costs to gate: live feedback has to re-run the *whole* pipeline on every keystroke
against a group that does not exist yet, using each member's *declared* type as a stand-in for its
identity. That works, and it is not even slow — but the feedback is a simulation of ingest rather
than a query about stored objects, and nothing it computes can be cached, because none of it is
keyed by a hash. arbor's posture buys exactly that: feedback about things that already exist.

**The relation-valued namespace pays for itself only at merge — now modelled, and it still does not
pay.** Every operation demands unconflictedness and `as_unconflicted` is all-or-nothing, with no way
to take the unconflicted part. Merge was the motivating case for making `N` a relation at all, and
having built it (`src/merge.re`): merge never puts a conflict *into* `N` either. `def:mergepre`
demands unconflictedness of all three inputs, the merged namespace is assembled only from the
resolved half of the diff, and conflicted names are **removed** from it and written into a file. So
the representable-conflict state is not used by the one operation it exists for. That is evidence for
the single-valued-`N` position gathered with the motivating case present rather than absent.

**Evict-on-failure is better than a residual of names, and it is also silent breakage.** The paper
calls this the row where Unison is ahead, and using it confirms that: getting the broken code back
in the buffer, ready to edit, is plainly more useful than a list. But it is worth naming what it
costs — a failed update *removes bindings you did not ask it to touch*, including the one you were
editing, with no confirmation. arbor's no-silent-breakage invariant and Unison's residual delivery
are in genuine tension, and the resolution is probably that the eviction should be visible and
undoable rather than that either half is wrong.

## Interface

Bonsai + js_of_ocaml, three panes, no server, state resets on reload. Left: the namespace (a
relation, with conflicted names badged) and the store (components, with each member's position, and
position 0 marked). Centre: the buffer, live feedback, one commit button. Right: the selected
component — every member's reference, names, type, stored form, dependencies, dependents and cached
value.

**Driven in headless Chrome over CDP** (`project: browser verification`), not merely rendered: the
seed commit; a successful update rebuilding a dependent; a failing update evicting both names and
writing `F ∪ S` back into the buffer; the `ping`/`pong` abort; `edit`-into-buffer including its
idempotence; component selection; and unbind. The only paths checked natively rather than in the
browser are the qcheck round-trip and the hashing algebra.

## Out of scope

Everything `mini-unison.tex` §1.7 lists, and for the same reasons: the namespace's representation as
a hashed Merkle tree, `Causal` and history and merge, suffix resolution and the `lib`-depth priority
rule, data declarations and `unique`-type minting, synhash, abilities, the ANF runtime, and
serialization. Also: byte-compatibility with real Unison hashes, and any of p12–p17's
`Opaque`/`Seal` machinery.

## Directory layout

```
src/     hash reference ty term abt scc builtins store names watch
         typecheck ingest eval resolve ppe pretty parse slurp update
         scratch                      — the buffer's own state, kept out of Store
         lexer.mll parser.mly surface
test/    test_p21.re   — 25 cases, each naming the paper statement it checks
web/     substrate state ops app browser editor detail
```

## Tech stack

OCaml/Reason 5.2, dune, Menhir (fail-fast, conflict-free, one stanza at a time with layout handled
in the driver), ppx_deriving, **digestif.ocaml SHA3-512**, js_of_ocaml, Bonsai/Virtual_dom/Core,
ppx_jane, alcotest, qcheck. `_opam` symlinks to `prototypes/p7-web-interface/_opam/_opam`.

Build: `eval $(opam env --switch=. --set-switch) && dune build && dune runtest && scripts/build-web.sh`,
then open `public/index.html`.

The paper↔prototype link is checked:

```
formalism/scripts/check-labels.sh formalism/paper/mini-unison.tex prototypes/p21-unison-model/src
```

49 statements, 49 mirrored.
