# Phase 22 — Open terms and explicit links

**Status:** Written after the prototype landed; the questions below were answered by building it.
**Design context:** `../../design/2026-09-17-design-doc.md` §Binding strategy (the thesis), `../../design/03-content-addressing.md` §"Mutual recursion: three ways to close the cycle" (which asks for a prototype), `../../design/04-naming-layer.md` §"Update strategies" ("No prototype has exercised the cascade primitives yet"), `../../design/13-editing-context.md` §"Multiplicity belongs to namespaces, not to bindings".
**Forks:** p21's object language, surface syntax and front end. Replaces its storage core.
**Prototype design lives here:** `docs/prototypes/p22-open-terms/`.
**Implementation code lives at:** `prototypes/p22-open-terms/`.

**Thesis.** `2026-09-17-design-doc.md` argues that Unison's naming-sensitive
propagation failures share one cause: a free variable is resolved to a hash at
edit time, the name is discarded, and every later operation that needs to know
what the author meant reconstructs it from the namespace. Its §Binding strategy
proposes keeping the two apart — store **open terms** with free variables
abstracted into a telescope, and **links** as separate objects, so that binding
is a recorded transformation rather than a collapse.

p22 is that, built on p21's object language so the comparison is controlled
rather than rhetorical: same STLC over `Nat`/`Text`/arrows, same six builtins,
same surface syntax, same buffer, opposite storage model. The four worked
examples in the design doc's Findings section are replayed as assertions.

## Answers the build produced

**All four replayed failures are unreachable, and three of them for the same
structural reason.** `scale`/`double`, the `fmt`/`f` alias defeating
propagation, and `foo`/`bar` order sensitivity all depend on propagation running
through a pretty-print environment. `propagate.re` contains no printer, no
parser and no name lookup; it rewrites an array of link targets. There is
nothing for a name to steer.

**`scale`/`double` is the positive version of that, and it is the headline.** A
followed dependent keeps its open hash *exactly* and gets a new link. In p21 the
same edit gives `double` a new hash from character-identical source text.

**The pair is confined to closed cycles, as hoped.** Because free variables
become `Slot(i)` rather than hash references, every member of a recursive group
has an independent open hash and nothing orders the group to hash it. Only
`Def.Member(bundle, i)` carries an index, and its *hash* is as opaque as any
other, so `Link.Def` still holds a bare `Hash.t`. That answers
`03-content-addressing.md`'s first cost for Option A — "every reference in the
system becomes a pair" — in the negative: no ordinary reference does.

**A third option for closing the cycle, and it needs no object-language
machinery.** `03` compares closing in the hasher (A, Unison) against closing in
the object language with a fixpoint over a tuple (B1) or a labelled record (B2).
p22 closes it in the **link**: `bundle.re` is a fixpoint over an array of links
with `Link.Self(i)` as the bound occurrence. No `Fix`, no `Pair`/`Fst`/`Snd`, no
record, no projection former, no new reduction rule. B1 is called "the cheapest
option in machinery"; this is cheaper, because the machinery is zero.

**And B1's automorphism argument survives the move, which I got wrong first
time.** `bundle.re` originally claimed the argument could not reach the link
layer, "because a link array has no projections to renumber symmetrically", and
the test written to demonstrate the resulting cost *failed*. For a symmetric
group the members share an open term and reordering renumbers the `Self` indices
in a way that exactly undoes the permutation: `[ping, pong]` and `[pong, ping]`
produce the same member array, so the bundle hash and both member hashes are
invariant under how the group was written. What varies is which *name* denotes
which member — weaker than the cost `03` predicts for B1, where the residual
ambiguity yields two structurally distinct, observationally identical
projections. Here it yields one object with two names, and since the members are
observationally identical nothing downstream can tell. p21 refuses to ingest
this case at all.

Residual presentation-dependence remains for *asymmetric* groups, where members
are distinguishable and declaration order does reach the bundle hash. That is
the opposite of the usual difficulty, and it composes: sorting on member open
hash is total exactly where it is needed, because distinguishable members have
distinct open hashes, and its known incompleteness (ties) lands only on
symmetric groups, which no longer need it. `Bundle.canonical_order` is identity
today and this is written up rather than implemented.

**The three histories separate, and need nothing asserted.** `scale` edited once
by its author and carried along twice by edits to `base` has three versions, one
authored edit and three link moves — visible as three readings of one column of
versions. Unison answers the same question with synhash (rehash with names
substituted for references), which is a reconstruction whose own source says it
"cannot handle renames very well"; p11 answers it with a minted thread, which
needs an asserted mark. Here it is two hash comparisons, and it survives renames
because no name appears in either hash.

**Recorded intent buys something Unison structurally cannot have.** The
`foo`/`bar` case is only a dilemma because a link holds a hash, and with an
alias a hash cannot say which name the author meant. Unison resolves it by
rendering through a PPE that picks one name per hash by
shortest-then-alphabetical, so which alias you edit decides whether propagation
happens. p22 wrote the name down at bind time (`Store.intent_of`), so
`Propagate.relink_one ~via` can just ask. This is not a reintroduction of
name-sensitivity: nothing is reconstructed and nothing is printed.

**`via` applies to the first hop only, and that is where the ambiguity is.**
Threading it down the whole cascade silently stopped propagation at depth one — a
real bug, caught by the three-histories test. A dependent of a dependent follows
a hash that has exactly one meaning; only the sites that *directly named* the
edited definition can be ambiguous.

**Typing is parameterized by the link, and an unbound slot is an ordinary type
error.** The same open term has different types under different links, which is
the content of the design rather than a defect: an open term is a shape, and a
shape is not typed until you say what it is over. A term with unresolvable names
is abstracted, hashed and stored, and *then* fails to typecheck with a slot index
and a name to report. That is `feedback: visible breakage, not gates` at exactly
the layer where p21 has its commit gate.

## Substrate model

| | |
|---|---|
| `Open_term.t` | `Bound(i) \| Slot(i) \| Lam \| App \| Nat \| Text \| Ann`. de Bruijn binders, no names anywhere. Layer tag 2. `hash` is **total** — there is no free variable left to fail on. |
| `Link.t` | `{arity, targets}` where `target = Def(h) \| Builtin(s) \| Self(i) \| Unbound`. Layer tag 3. Hashes arity and targets; **never** names. |
| `Bundle.t` | An array of `(open hash, link)` whose links may use `Self`. Layer tag 4. |
| `Def.t` | `Simple(open, link) \| Member(bundle, i)`. Layer tag 5. The only index in the system. |
| `Store.t` | Four populations plus `types`, an **intent** table `(link, slot) → names`, and `dependents` (the reverse index, the one not recomputable by traversal). |
| `Namespace.t` | Single-valued, per `13-editing-context.md:136`. Aliases are free and cannot affect propagation. |
| `Oplog.t` | One append-only log with grouping ids. The three histories are filters over it. |

**Slot order is first occurrence** in a left-to-right traversal — a function of
structure alone, never of names (`03-content-addressing.md:146`).

**The intent table is a table rather than a field.** Names must not reach stored
bytes, but excluding them from a hashed record would mean two authors who wrote
different names for identical targets are one object and one set of names wins —
which is precisely the clobber the design doc identifies in Unison's *binder*
names. Accumulating into a list means nothing clobbers and nothing named is
hashed.

## Design decisions (confirmed)

1. **Positional telescope, stored; labelled view deferred.** The plan was to
   build both and compare. Positional landed; the labelled view (p16 labels as
   an aspect over the open hash) did not, because nothing in the prototype
   needed it — see `open-questions.md`.
2. **Binders are de Bruijn**, reversing p21, which stores binder names because
   `update` renders definitions back to source (`rem:names`). p22 can afford de
   Bruijn precisely because it never hands rendered text back to the machinery.
3. **Printing is display-only.** `pretty.re` is demoted from a correctness
   obligation to a convenience: the round-trip property p21 must maintain, p22
   does not have to state.
4. **Only a genuine cycle becomes a bundle.** The buffer is decomposed by
   `Scc.components`; singletons commit one at a time in dependency order. p21
   must treat the whole buffer as a group because its ingest is
   group-at-a-time; doing that here would put every definition in a bundle and
   throw the confinement away.
5. **BLAKE2B, not p21's SHA3-512.** p21 used Unison's hash because it models
   Unison. p22 returns to the arbor line's.

## In scope

Open terms, links, bundles, the three histories, pin/follow/explicit,
`follow-clean`, recorded intent, a three-pane web UI.

## Out of scope (deferred)

Merge and branching (wants `Causal`-shaped history — its own prototype); the CST
layer and the rest of the projection tower; message-key locales; partial
checkouts; concurrent agents; undo actually implemented (the log has the
grouping it would need, and nothing consumes it).

## Relationship to design docs

- **Answers** `03-content-addressing.md` §"Mutual recursion" with a fourth
  option, and with the finding that the automorphism argument reaches it.
- **Exercises** `04-naming-layer.md` §"Update strategies", which recorded that
  no prototype had. Finds that the three are not three policies but one
  primitive over three site sets.
- **Contradicts nothing recorded**, unlike p21. It sits on arbor's own side of
  every position p21 was built to diverge from: permissive ingest, single-valued
  `N`, no names in stored programs, type as an aspect of the definition.
- **Bears on** `open-questions.md:131` ("is arbor accumulating exactly what
  Unison spent three years deleting?"). A link is reified because it records
  *intent that cannot be recomputed*, which is the defensible side of that line —
  and unlike Unison's deleted metadata, it is keyed by hash rather than by name.

## Interface

Bonsai + js_of_ocaml, three panes, no server, state resets on reload. Left: the
namespace and the store's four counts, with the sharing claim in numbers
("6 definitions over 4 open terms"). Centre: the buffer, a strategy selector,
and a per-slot preview of what a commit would store — which is possible before
committing because abstraction is a pure function of the text and its hash is
already the store's. Right: the selected definition and **the three histories**,
rendered as three readings of one column of versions so that their disagreement
is visible. Open hashes are one colour throughout and link hashes another.

Driven in the browser, not merely rendered: seed commit, an edit to `base`
relinking two dependents, and the resulting history split were all performed
through the UI.

## Directory layout

```
src/     hash ty term open_term link bundle def store namespace
         bind propagate oplog session typecheck eval watch
         pretty scc parse lexer.mll parser.mly surface builtins
test/    test_p22.re   — 18 cases
web/     substrate state ops app browser editor detail
```

## Tech stack

OCaml/Reason 5.2, dune, Menhir (fail-fast), ppx_deriving, **digestif.ocaml
BLAKE2B**, js_of_ocaml, Bonsai/Virtual_dom/Core, ppx_jane, alcotest, qcheck.
`_opam` symlinks to `prototypes/p7-web-interface/_opam/_opam`.

Build: `eval $(opam env --switch=. --set-switch) && dune build && dune runtest && scripts/build-web.sh`,
then open `public/index.html`.
