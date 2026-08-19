# arbor — formalism

A mathematical formalism of the arbor substrate, developed the way the prototypes are:
**start small, work up.** The first artifact, `arbor-core`, formalizes the untyped
λ-calculus fragment of the substrate together with its three stores and an edit calculus.
The second, `arbor-stlc`, adds simple types as a delta document over it.

This directory is a first-class, *enduring* artifact (like `docs/design/`), distinct from the
disposable `prototypes/`. Where a design doc argues in prose and a prototype demonstrates in
code, the formalism states — precisely, and increasingly machine-checked — *what the substrate
guarantees and why content-addressing earns its complexity.* Both of `arbor-core`'s headline
theorems are now proved in Agda; see [Mechanization](#mechanization).

## What `arbor-core` covers

One content-addressed **term store** `Σ`, one **namespace** `N` (name → hash), one derived
**evaluation cache** `E`, and an **edit calculus** (bind / rebind / unbind / explicit
migration) operating on the configuration `⟨Σ, N, E, H⟩` (`H` = append-only binding history).
The store carries **no definition sort**: naming a closed stored term is the only assertion of
definition-hood, and migration is a **whole-store structural rewrite** — names follow their
hashes (unit-scoped following would need mints, which this artifact defers).

The document is built around two dual metatheorems, both consequences of one fact —
`eval` is a pure function of `(Σ, hash)` and `Σ` is immutable and monotone:

- **No silent breakage** — editing names never changes the meaning of a stored term.
- **Evaluation stability / no re-evaluation** — a once-computed result is valid in every future
  store; migration re-evaluates only genuinely new definitions.

See `paper/arbor-core.tex` §1 for the full scope statement and the deferred list.

## Relationship to the rest of the repo

The formalism is faithful to two prototypes and two design docs; every definition in the paper
cites its source of truth by path:

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

Two deliberate generalizations of the prototypes are recorded in `decisions.md`: the formalism
**reintroduces `Ref(hash)`** (which p4 elided by inlining), and **drops the mint/thread axis**
(p10/p11) and **types** (p9 line) from this first artifact. The gate p11 implements as a
typecheck becomes an untyped gate that is in fact **total** — Follow cannot fail here. A third
block of commitments (2026-07-30, from the first design review and revised the same day) added
premises on the `Ingest`/`Bind`/`Rebind` transitions, a **ref-acyclicity** clause in `wf`, and
the **no-definition-sort** stance (a briefly-introduced explicit root set `R` was retracted as
mint-flavored; migration became the whole-store rewrite) — see `decisions.md`.

## What `arbor-stlc` covers

The STLC rung (`paper/arbor-stlc.tex`), a **delta document**: it restates only what changes
and cites arbor-core (via `xr-hyper`; external references render "arbor-core Definition
N.M"). It adds simple types per p6 with **content-addressed types** per p9's extension (the
store becomes two-sorted; a `Lam` carries a type hash; type equality = hash equality), a
syntax-directed typing judgment with a new **T-Ref** rule (p6 inlines references; the rule is
new formal content), and a second derived aspect `Θ` (hash-valued `TypeOf`). Nothing is
gated on types — store *and namespace* stay permissive; typing is observational. Headline
results: **local soundness** (a well-typed term evaluates safely amid ill-typed neighbors),
**residual exactness** (migration commits and reports exactly the names it broke — the
"continue the refactor" list, with a standing `broken` query), **type-preserving migration is
total** (same type at the seed ⟹ empty residual under every scope), and the **clean oracle**
(decidable ahead of time, but *not* stable under store growth — exposing an unsound cache in
p11's `follow-clean:v1`). Sources: `decisions.md` 2026-07-30 (arbor-stlc entry).

## Mechanization

`arbor-core` is largely machine-checked (Agda 2.7.0, `agda-stdlib` 2.1): **17 of the
paper's 18 statements**, including both headline theorems — *no silent breakage* and
*evaluation stability* — plus coherence preservation, history coherence, α-collapse,
monotonicity, cache soundness, incrementality, the fuel/⇓ bridge, and the naming
round-trips, and `cor:transfer` — the sharing property a commons needs, which turned out
to be `thm:stability`'s proof verbatim, stated too narrowly. M3 is under way: **ρ itself is now defined** (well-founded recursion on
the reference graph, with `def:cascade`'s seed clause proved), and what remains is
registering its image and re-establishing `wf` for the rewritten store — the latter
blocked on the fact that ρ is not injective, so acyclicity does not transfer along it
and the registration order has to be made explicit. `arbor-stlc` is M4.

Mechanizing turned up four gaps in the paper, now fixed there: `lem:closed-no-stuck`
was **false** as stated (there is a machine-checked counterexample), which in turn
forced a missing closedness premise onto the `Eval` transition; `thm:mono` tacitly
assumed the store is keyed by hash; and two hypotheses elsewhere are redundant.

The whole library checks under `agda --safe` with **no postulates**: the paper's one axiom
(★) is a field of a record the development is parameterized over, and a consistency witness
for that record is exhibited, so no proved statement is vacuous. Statements not yet proved
are named types with no inhabitant, which is why a green build cannot be mistaken for a
complete one — and why nothing can quietly lean on an unproved lemma.

```sh
cd formalism/agda
make check     # agda --safe Everything.agda
make status    # proved / open / deferred, per paper label
make labels    # paper labels with no counterpart in the Agda sources
```

`make labels` is the anti-drift check: it diffs the paper's `\label{}`s against `(label)`
mentions in the Agda sources. It exists because the two artifacts had drifted badly —
`agda/README.md` sat unchanged across the whole `arbor-stlc` landing, and both it and
`decisions.md` went on indexing theorems as `T1`–`T8`, a numbering neither paper uses.
Paper labels are now the only names. See `agda/README.md` for the status table and the
representation choices, and `decisions.md` 2026-08-05 for why each was made.

## Building the paper

Requires a TeX distribution with `latexmk`.

```sh
cd formalism/paper
latexmk -pdf arbor-core.tex     # -> arbor-core.pdf  (build this FIRST)
latexmk -pdf arbor-stlc.tex     # -> arbor-stlc.pdf  (reads arbor-core.aux via xr)
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
    macros.tex         # notation (shared; arbor-stlc additions are additive)
    references.bib     # bibliography
  scripts/
    check-labels.sh    # paper \label{} <-> Agda (label) mirror check
    status.sh          # proved / open / deferred, per paper label
  agda/
    README.md          # status table, representation choices, milestones
    Everything.agda    # the --safe check target
    Arbor/             # Prelude, Hash, NodeSig + Core/ (the untyped λ instance)
```
