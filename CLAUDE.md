# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository state

`lc-content-addressed` is a design exploration for a content-addressed, multi-language computational substrate working through Pierce's *Types and Programming Languages* (TAPL), inspired by Unison and intended as a long-term substrate for Hazel's computational-commons vision.

Substrate design lives in `docs/design/` and endures across prototypes. Five disposable prototypes have been scaffolded so far, each in OCaml/Reason with dune + Menhir + digestif (BLAKE2B) + alcotest/qcheck:

- `p1-arithmetic` — minimum register / lookup / evaluate loop for untyped arithmetic (TAPL Ch. 3).
- `p2-structural-sharing` — shallow, DAG-shaped storage plus the Attachment aspect store with a derived eval-cache aspect.
- `p3-naming-layer` — first-class namespace of name ↔ hash bindings, edit-time resolution, a separate `Surface_ast.t` that keeps the internal `Ast.t` name-free at the type level, name-aware pretty-printer, and the visible "no silent breakage" invariant.
- `p4-lambda-calculus` — untyped λ-calculus (TAPL Ch. 5) with de Bruijn indices internally and named surface syntax. First substrate demonstration of α-equivalence via canonicalization (`\x. x` and `\y. y` share a hash). Carries p3's naming layer forward and adds a CBV β-reducer with a step budget for non-terminating terms.
- `p5-multi-language` — both arithmetic and λ-calculus in one Store keyed by a `Definition.t = Arith | Lc` sum. Adds a hand-written Church-encoding translator from arithmetic to λ-calculus, invoked manually from the REPL (`:translate`) and cached as a derived aspect on the arith source. First substrate demonstration of `docs/design/05-translation.md`: translator identity `arith-to-lc-church:translate:v1`, output recorded as `Translation_target(Hash.t)` under aspect `translation-to-lc`.

## Layout

```
docs/
  design/                     # Substrate-level ideas — enduring across all prototypes
  prototypes/
    p1-arithmetic/            # Scope + decisions + open-questions per prototype
    p2-structural-sharing/
    p3-naming-layer/
    p4-lambda-calculus/
    p5-multi-language/
prototypes/
  p1-arithmetic/              # OCaml/Reason source per prototype
  p2-structural-sharing/
  p3-naming-layer/
  p4-lambda-calculus/
  p5-multi-language/
```

Each `docs/prototypes/<name>/` holds its own `decisions.md` (dated ADR-lite log; append-only, reversals get new entries) and `open-questions.md` (running list). Substrate-level decisions are separate from prototype-specific decisions.

Prototype code at `prototypes/<name>/` uses its own local opam switch at `prototypes/<name>/_opam/`. Standard dune commands (`dune build`, `dune exec`, `dune runtest`) work from inside each prototype directory after `eval $(opam env --switch=. --set-switch)`.

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

## Current prototype (p5-multi-language)

Most recent prototype; the next changes will likely live here or in a successor.

- Two languages in one Store: arithmetic (p3's language) and untyped λ-calculus (p4's), behind a `Definition.t = Arith(Arith_node.t) | Lc(Lc_node.t)` sum. Per-language modules are `Arith_*` and `Lc_*`; `Pretty` is a dispatching façade.
- Hash-space separation via a one-byte language tag ('A' for arith, 'L' for lc) prepended to every node encoding. "No cross-language references" from `docs/design/03-content-addressing.md` is enforced at Store registration (cross-language parent→child raises `Language_mismatch`) and at edit time (Resolver checks `Store.language_of` before inlining a namespace-bound hash).
- Church-encoding translator at `src/arith_to_lc_church.re`, procedure identity `arith-to-lc-church:translate:v1`, output stored as `Translation_target(Hash.t)` under aspect `translation-to-lc`. Cache hit on repeat invocations. Direction is one-way; reverse translation is not attempted.
- REPL mode-switches languages with `:lang arith` / `:lang lc`. New commands: `:translate <name|#pfx>` (invoke translator, with cache marker), `:translations [arg]` (list cached translations). Viewing commands (`:list`, `:names`, `:dag`, `:lookup`, `:show`, `:stats`) prepend language tags (`[arith]` / `[lc]   `, fixed 7-char width).
- Evaluators: `arith:eval:v1` and `lc:eval:v1` run side by side, dispatched by definition language at `:eval`. Lc evaluator is still CBV-WHNF (doesn't reduce under binders) — translator correctness in tests uses a deep β-normalizer.
- Still no `Ref(hash)` AST constructor in either language; inline-at-resolution carries forward. Translation's eager-closure rule (`05-translation.md:§Transitive dependencies`) is trivially satisfied today because stored arith definitions are closed deep trees with no cross-definition references.
- Stack unchanged: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, alcotest, qcheck, digestif (BLAKE2B). Opam switch symlinked to p3's.
- Interface: interactive REPL at `bin/main.re`; prompt shows the current language (e.g., `(lc) > `); `:help` lists commands.

See `docs/prototypes/p5-multi-language/{00-scope.md,decisions.md,open-questions.md}` for details.
