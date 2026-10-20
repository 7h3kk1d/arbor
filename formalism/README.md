# arbor — formalism

A mathematical formalism of the arbor substrate, developed the way the prototypes are: start small,
work up. The first artifact, `arbor-core`, formalizes the untyped λ-calculus fragment of the
substrate with its three stores and an edit calculus. The second, `arbor-stlc`, adds simple types as
a delta document over it. A third, `mini-unison`, is not a rung: it models another system, so that
arbor's choices can be compared against something precise.

This directory is an enduring artifact, like `docs/design/`, distinct from the disposable
`prototypes/`. A design doc argues in prose and a prototype demonstrates in code; the formalism
states precisely, and increasingly machine-checked, what the substrate guarantees and why content
addressing is worth its complexity. Both of `arbor-core`'s headline theorems are proved in Agda; see
[Mechanization](#mechanization).

## What `arbor-core` covers

One content-addressed **term store** `Σ`, one **namespace** `N` (name → hash), one derived
**evaluation cache** `E`, and an **edit calculus** (bind / rebind / unbind / explicit migration) over
the configuration `⟨Σ, N, E, H⟩`, where `H` is the append-only binding history. The store has no
definition sort: naming a closed stored term is the only assertion of definition-hood. Migration is a
whole-store structural rewrite in which names follow their hashes. Unit-scoped following would need
mints, which this artifact defers.

The document is built around two dual metatheorems. Both follow from one fact: `eval` is a pure
function of `(Σ, hash)`, and `Σ` is immutable and monotone.

- **No silent breakage.** Editing names never changes the meaning of a stored term.
- **Evaluation stability / no re-evaluation.** A computed result is valid in every future store;
  migration re-evaluates only new definitions.

`paper/arbor-core.tex` §1 has the full scope statement and the deferred list.

## Relationship to the rest of the repo

The formalism follows two prototypes and two design docs, and every definition in the paper cites its
source of truth by path:

| Formal object | Source of truth |
| --- | --- |
| Surface / core syntax, `shift`/`subst`/`beta`, α-canonicity | `prototypes/p4-lambda-calculus/src/{surface_ast,ast,canonicalize}.re` |
| Shallow node, hash encoding, `ingest`/`reconstruct` | `prototypes/p4-lambda-calculus/src/{node,hash,store}.re` |
| Evaluation (CBV/WHNF) + cache discipline | `prototypes/p4-lambda-calculus/src/{eval,attachment}.re` |
| Namespace, `bind`/`rebind`/`unbind`, history, orphan `name(vN)` | `prototypes/p11-mint-threads/src/namespace.re` |
| `callers_of`, `multi_rebind`, pin/follow/explicit migration | `prototypes/p11-mint-threads/src/{store,update_strategy,follow_clean}.re` |
| Content-addressing invariants (`Ref`, no-dangle, immutability) | `docs/design/03-content-addressing.md` |
| Naming layer, resolution, no-silent-breakage, update strategies | `docs/design/04-naming-layer.md` |
| Type system, values, fueled evaluator (arbor-stlc) | `prototypes/p6-stlc/src/{stlc_ast,stlc_node,ty,stlc_typecheck,stlc_eval}.re` |
| Content-addressed types, hash-valued `Type_of` (arbor-stlc) | `prototypes/p9-typed-namespaces/src/{node,ty,definition,typecheck}.re` |
| Clean oracle / dry run, migration report (arbor-stlc) | `prototypes/p11-mint-threads/src/{follow_clean,update_strategy}.re` |

`decisions.md` records where the formalism generalizes the prototypes. It reintroduces `Ref(hash)`,
which p4 elided by inlining. It drops the mint/thread axis (p10/p11) and types (p9 line) from the
first artifact. The gate p11 implements as a typecheck becomes an untyped gate that is total: Follow
cannot fail here. The 2026-07-30 entries, from the first design review and revised the same day, add
premises on the `Ingest`/`Bind`/`Rebind` transitions, a ref-acyclicity clause in `wf`, and the
no-definition-sort stance. A briefly introduced explicit root set `R` was retracted as mint-flavored,
and migration became the whole-store rewrite.

## What `arbor-stlc` covers

The STLC rung (`paper/arbor-stlc.tex`) is a delta document. It restates only what changes and cites
arbor-core via `xr-hyper`; external references render as "arbor-core Definition N.M". It adds:

- simple types per p6, with content-addressed types per p9's extension (the store becomes two-sorted,
  a `Lam` carries a type hash, and type equality is hash equality);
- a syntax-directed typing judgment with a new **T-Ref** rule (p6 inlines references, so the rule is
  new formal content);
- a second derived aspect `Θ`, the hash-valued `TypeOf`.

Nothing is gated on types. Store and namespace stay permissive, and typing is observational. Headline
results:

- **Local soundness.** A well-typed term evaluates safely amid ill-typed neighbors.
- **Residual exactness.** Migration commits and reports exactly the names it broke, the "continue the
  refactor" list, with a standing `broken` query.
- **Type-preserving migration is total.** Same type at the seed ⟹ empty residual under every scope.
- **The clean oracle** is decidable ahead of time but not stable under store growth, which exposes an
  unsound cache in p11's `follow-clean:v1`.

Source: `decisions.md` 2026-07-30 (arbor-stlc entry).

## What `mini-unison` covers — and why it is not a rung

`paper/mini-unison.tex` is a contrast artifact. It formalizes Unison's content addressing and update
scheme, faithful to the implementation at commit `db60ce2`, and proposes nothing. Unison is the system
arbor is closest to and has no publication, so every "arbor differs because…" claim in `docs/` was
being made against prose instead of a stated design.

Scope: a small object language (untyped λ + `Nat`/`Text` + simple types, typed because Unison's
`update` re-typechecks and the interesting half of `update` is what happens when that fails);
components and `(Hash, Pos)` references; reference transparency; the type inside the term hash; names
as a relation; the watch cache; and `update` as render/parse/typecheck with eviction on failure.
Deferred, and listed in the paper's §1.4: the namespace's Merkle representation, `Causal` history,
branching and merge, suffix resolution, data declarations and minting, synhash. (Merge and synhash
have since been added as §7; see `decisions.md` 2026-09-03.)

Three early results:

- **Presentation-independence of component hashing is conditional.** When two members of a cycle are
  structurally equivalent modulo intra-cycle references, the component's hash depends on the members'
  names. The condition is decidable, and the implementation aborts instead of storing a
  name-dependent hash. That is stronger than the "partial function" reading the source read had
  recorded.
- **Reference transparency holds only at `Pos = 0`.** Non-head cycle members hash opaquely, so
  transparency and the component model are not independent features.
- **Unison's `update` rests on print-then-parse preserving hashes.** Unison never states this property
  and maintains it with a golden-file corpus, `reparses-with-same-hash.u`. It is `arbor-core`'s
  `prop:print-elab`, proved.

Two of these corrected `docs/systems/unison/02-store-and-hashing.md`. Formalizing another system's
design is a way of reading it, and it finds different things than prose reading does.

**`mini-unison` is paper-only.** It is outside the Agda discipline. `make check`, `make status` and
`make labels` target `arbor-core` and `arbor-stlc` and should stay that way, so a green build is never
read as saying anything about this document.

## Mechanization

`arbor-core` is machine-checked in Agda 2.7.0 with `agda-stdlib` 2.1: **17 of the paper's 18
statements**. These include both headline theorems (no silent breakage, evaluation stability),
coherence preservation, history coherence, α-collapse, monotonicity, cache soundness, incrementality,
the fuel/⇓ bridge, the naming round-trips, and `cor:transfer`, the sharing property a commons needs,
whose proof is `thm:stability`'s verbatim. M3 is done: ρ is constructed by well-founded recursion on
the reference graph, its image registered and `wf` re-established for the rewritten store
(`decisions.md` 2026-08-07). Only `lem:cascade-order` remains, deferred and no longer a prerequisite.
`arbor-stlc` is M4, now under way on a signature-generic store layer (`agda/README.md`).

Mechanizing found four gaps in the paper, now fixed there. `lem:closed-no-stuck` was false as stated
(there is a machine-checked counterexample), which forced a missing closedness premise onto the `Eval`
transition. `thm:mono` tacitly assumed the store is keyed by hash. Two hypotheses elsewhere are
redundant.

The library checks under `agda --safe` with no postulates. The paper's one axiom (★) is a field of a
record the development is parameterized over, and a consistency witness for that record is exhibited,
so no proved statement is vacuous. Unproved statements are named types with no inhabitant, so a green
build cannot be mistaken for a complete one and nothing can lean on an unproved lemma.

```sh
cd formalism/agda
make check     # agda --safe Everything.agda
make status    # proved / open / deferred, per paper label
make labels    # paper labels with no counterpart in the Agda sources
```

`make labels` is the anti-drift check. It diffs the paper's `\label{}`s against `(label)` mentions in
the Agda sources. It exists because the two artifacts had drifted: `agda/README.md` sat unchanged
through the `arbor-stlc` landing, and both it and `decisions.md` indexed theorems as `T1`–`T8`, a
numbering neither paper uses. Paper labels are now the only names. `agda/README.md` has the status
table and representation choices; `decisions.md` 2026-08-05 has the reasons.

## Building the paper

Requires a TeX distribution with `latexmk`.

```sh
cd formalism/paper
latexmk -pdf arbor-core.tex     # -> arbor-core.pdf  (build this FIRST)
latexmk -pdf arbor-stlc.tex     # -> arbor-stlc.pdf  (reads arbor-core.aux via xr)
latexmk -pdf mini-unison.tex    # -> mini-unison.pdf (reads both .aux files via xr)
latexmk -c                      # clean aux files (breaks arbor-stlc's external refs
                                #   until arbor-core is rebuilt)
```

## Layout

```
formalism/
  README.md            # this file
  decisions.md         # ADR-lite log of the modeling choices
  open-questions.md    # running backlog for the "work up" successors
  paper/
    arbor-core.tex     # first artifact (Parts I–V + metatheory)
    arbor-stlc.tex     # second artifact: the STLC rung, a delta over arbor-core
    mini-unison.tex    # contrast artifact: a model of Unison (paper-only, not a rung)
    macros-unison.tex  # mini-unison notation, deliberately disjoint from macros.tex
    macros.tex         # notation (shared; arbor-stlc additions are additive)
    references.bib     # bibliography
  scripts/
    check-labels.sh    # paper \label{} <-> Agda (label) mirror check
    status.sh          # proved / open / deferred, per paper label
  agda/
    README.md          # status table, representation choices, milestones
    Everything.agda    # the --safe check target
    Arbor/             # Prelude, Hash, Sig, NodeSig, Generic/, Core/ (untyped λ), Stlc/
```
