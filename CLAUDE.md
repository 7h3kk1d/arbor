# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository state

**Design stage only — no code yet.** This is `lc-content-addressed`: a design exploration for a content-addressed, multi-language computational substrate working through Pierce's *Types and Programming Languages* (TAPL), inspired by Unison and intended as a long-term substrate for Hazel's computational-commons vision.

The repository contains only `docs/`. There is no build system, no `dune-project`, no tests. The first prototype (`p1-arithmetic`) is scoped but not yet scaffolded into code.

## Layout

```
docs/
  design/           # Substrate-level ideas — enduring across all prototypes
  prototypes/
    p1-arithmetic/  # First prototype's scope and decisions (code TBD)
```

Each directory holds its own `decisions.md` (dated ADR-lite log; append-only, reversals get new entries) and `open-questions.md` (running list; resolved items struck through, not deleted). Substrate-level decisions are separate from prototype-specific decisions.

Eventual prototype code will live at `prototypes/<name>/` (paralleling `docs/prototypes/<name>/`). Nothing is there yet.

## Reading order

To orient before modifying:

1. `docs/design/00-overview.md` — vision, core model, glossary, doc map.
2. `docs/design/06-architecture.md` — four-layer decomposition.
3. `docs/design/decisions.md` and `docs/design/open-questions.md`.

Other design docs (`01–05`, `07`, `09`) are topic-specific and read on demand. `docs/design/07-hazel-substrate.md` is the sourcing farm for Hazel-related reuse and research lines, including Grove (POPL 2025) as the foundation for eventual collaborative editing.

## Conventions

- **Aspect, not kind.** Categories of associated data (types, names, translations, etc.) are called *aspects*. "Kind" is reserved for the type-theory concept that will appear literally once F-omega is instantiated.
- **Typed values at substrate APIs.** No byte arrays at public boundaries. Serialization is internal to layers that need it. Adding a language extends the `Definition.t` sum; plugin languages are future work.
- **Disposable prototypes.** A series of experiments, not one long-lived codebase. Each prototype answers specific questions and may be discarded. Learnings that refine the substrate migrate back into `docs/design/` with the prototype cited as source.
- **Don't pre-solve future concerns.** Note them in `open-questions.md`. Don't elaborate speculative infrastructure. Bootstrap phase accepts full state rebuilds; don't design migration machinery for early-phase changes.

## Four-layer architecture

From `docs/design/06-architecture.md`, upward-only dependencies:

1. **Store** — content-addressed definition storage; enforces "no cross-language references" at registration.
2. **Attachment** — aspect store and namespace(s); bidirectional queries.
3. **Language** — per-language modules (AST, canonicalizer, type-check, evaluator, primitives) plus inter-language translators.
4. **Interface** — user-facing modalities.

## Phase 1 prototype (p1-arithmetic)

Not yet scaffolded. Target when it is:

- Language: untyped arithmetic from TAPL Ch. 3.
- Stack: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, alcotest, qcheck, digestif (BLAKE3).
- Interface: interactive REPL, in-memory only (no persistence).
- Layout: single library at `src/`, executable at `bin/`, tests at `test/`.

See `docs/prototypes/p1-arithmetic/00-scope.md` for full scope. Standard dune commands (`dune build`, `dune exec`, `dune test`) will work from inside `prototypes/p1-arithmetic/` once code is scaffolded.
