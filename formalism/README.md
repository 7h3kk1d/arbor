# arbor — formalism

A mathematical formalism of the arbor substrate, developed the way the prototypes are:
**start small, work up.** The first artifact, `arbor-core`, formalizes the untyped
λ-calculus fragment of the substrate together with its three stores and an edit calculus.

This directory is a first-class, *enduring* artifact (like `docs/design/`), distinct from the
disposable `prototypes/`. Where a design doc argues in prose and a prototype demonstrates in
code, the formalism states — precisely, and eventually machine-checked — *what the substrate
guarantees and why content-addressing earns its complexity.*

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

Two deliberate generalizations of the prototypes are recorded in `decisions.md`: the formalism
**reintroduces `Ref(hash)`** (which p4 elided by inlining), and **drops the mint/thread axis**
(p10/p11) and **types** (p9 line) from this first artifact. The gate p11 implements as a
typecheck becomes an untyped gate that is in fact **total** — Follow cannot fail here. A third
block of commitments (2026-07-30, from the first design review and revised the same day) added
premises on the `Ingest`/`Bind`/`Rebind` transitions, a **ref-acyclicity** clause in `wf`, and
the **no-definition-sort** stance (a briefly-introduced explicit root set `R` was retracted as
mint-flavored; migration became the whole-store rewrite) — see `decisions.md`.

## Building the paper

Requires a TeX distribution with `latexmk`.

```sh
cd formalism/paper
latexmk -pdf arbor-core.tex     # -> arbor-core.pdf
latexmk -c                      # clean aux files
```

## Layout

```
formalism/
  README.md            # this file
  decisions.md         # ADR-lite log of the modeling choices
  open-questions.md    # running backlog for the "work up" successors
  paper/
    arbor-core.tex     # the document (Parts I–V + metatheory)
    macros.tex         # notation
    references.bib     # bibliography
  agda/
    README.md          # mechanization roadmap (no code yet)
```
