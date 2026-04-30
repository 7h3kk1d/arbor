# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository state

`lc-content-addressed` is a design exploration for a content-addressed, multi-language computational substrate working through Pierce's *Types and Programming Languages* (TAPL), inspired by Unison and intended as a long-term substrate for Hazel's computational-commons vision.

Substrate design lives in `docs/design/` and endures across prototypes. Nine disposable prototypes have been scaffolded so far, each in OCaml/Reason with dune + Menhir + digestif (BLAKE2B) + alcotest/qcheck:

- `p1-arithmetic` — minimum register / lookup / evaluate loop for untyped arithmetic (TAPL Ch. 3).
- `p2-structural-sharing` — shallow, DAG-shaped storage plus the Attachment aspect store with a derived eval-cache aspect.
- `p3-naming-layer` — first-class namespace of name ↔ hash bindings, edit-time resolution, a separate `Surface_ast.t` that keeps the internal `Ast.t` name-free at the type level, name-aware pretty-printer, and the visible "no silent breakage" invariant.
- `p4-lambda-calculus` — untyped λ-calculus (TAPL Ch. 5) with de Bruijn indices internally and named surface syntax. First substrate demonstration of α-equivalence via canonicalization (`\x. x` and `\y. y` share a hash). Carries p3's naming layer forward and adds a CBV β-reducer with a step budget for non-terminating terms.
- `p5-multi-language` — both arithmetic and λ-calculus in one Store keyed by a `Definition.t = Arith | Lc` sum. Adds a hand-written Church-encoding translator from arithmetic to λ-calculus, invoked manually from the REPL (`:translate`) and cached as a derived aspect on the arith source. First substrate demonstration of `docs/design/05-translation.md`: translator identity `arith-to-lc-church:translate:v1`, output recorded as `Translation_target(Hash.t)` under aspect `translation-to-lc`.
- `p6-stlc` — STLC (TAPL Ch. 8+9: pure λ→ over Bool with native `true`/`false`/`if`, every lambda annotated) paired with untyped λ-calculus in one Store, keyed by `Definition.t = Lc | Stlc`. Arith is dropped. First prototype with a type system: type-checking is enforced at ingest (Store invariant "every stored stlc definition type-checks") and cached as the `stlc:type-check:v1` aspect with value `Type_of(Ty.t)` — first non-`Hash.t`-valued aspect. Two translators: `stlc-to-lc:erase-church:v1` (total; erase annotations + Church-encode booleans) and `lc-to-stlc:check:v1[ty=<hex8>]` (partial; user supplies a target type, constraint-based unification decides). First worked examples of (a) partial translators via `Translation_untypable(string)` and (b) translators with inputs beyond the source via procedure-id encoding.
- `p7-web-interface` — first interface-layer prototype. Carries p6's lc + stlc substrate verbatim and adds a Bonsai + js_of_ocaml in-browser UI (no server, state resets on reload). Three-pane layout: editor, browser (filterable list), detail. Every keystroke ingests synchronously via `Feedback.compute`. The substrate compiles to a single ~26 MB JS bundle; binding a name is the only deferred stateful operation in the UI.
- `p8-holes` — forks p4 (untyped λ-calculus) and replaces the fail-fast parser with an error-recovering one built on Menhir's incremental API. Every textual input becomes a valid `Surface_ast.t`; subterms that would fail to parse are replaced with `Hole` nodes. Holes are a new leaf constructor with tag byte `'\x04'` — all holes are structurally equal, so the BLAKE2B digest is constant. First substrate answer to `docs/design/open-questions.md` §"Holes and incomplete programs": bare `Hole`, no payload.
- `p9-typed-namespaces` — fuses p6 (typed language + Type_of aspect), p8 (parser recovery + bare Hole), and p7 (Bonsai + jsoo UI). One typed surface language with annotated lambdas, monomorphic `let`, native `Int`/`Bool`/`String`/`Product` plus primitive arith/bool/string-concat operations. Hierarchical dot-delimited names (`math.add`) with Unison-style longest-segment-suffix resolution and a distinct `Ambiguous` error. Holes integrate with the type system via a permissive bidirectional checker producing three outcomes — `Type_of(ty)`, `Type_with_holes(ty)`, or `Ill_typed` (rejects ingest). New `has-holes:v1` derived aspect caches whether a term or any DAG-reachable subterm contains a hole. UI replaces p7's filterable list with a collapsible namespace tree (has-holes badges per leaf) and adds a recovered-AST panel that re-renders the post-recovery surface AST every keystroke.

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
    p6-stlc/
    p7-web-interface/
    p8-holes/
    p9-typed-namespaces/
prototypes/
  p1-arithmetic/              # OCaml/Reason source per prototype
  p2-structural-sharing/
  p3-naming-layer/
  p4-lambda-calculus/
  p5-multi-language/
  p6-stlc/
  p7-web-interface/
  p8-holes/
  p9-typed-namespaces/
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

## Current prototype (p9-typed-namespaces)

Most recent prototype; the next changes will likely live here or in a successor.

- Single typed language: native `Int`, `Bool`, `String`, `Product`; annotated lambdas (p6-style); monomorphic `let`; `if`/`then`/`else`; pairs with `fst`/`snd`; primitive operations `+ - mul / mod && || not ++ ==`. `mul` is a keyword because `*` is the product-type constructor. Types: `Ty.t = Int | Bool | String | Arrow | Product` with stable tag bytes 0x10..0x14 in node encoding. No lc, no stlc, no Church encoding. `Definition.t = Node.t` (single-language).
- **Hierarchical dot-delimited names with Unison-style longest-segment-suffix resolution.** Substrate keeps names as opaque dotted strings (per `04-naming-layer.md` Threads §Hierarchical paths, "editing-layer convention only"). `Namespace.resolve_query` first tries full-path lookup; on miss, scans for entries whose dot-segments end with the query's segments. Zero matches → `Unbound`; one → `Ok h`; multiple → `Ambiguous(list(string))`. `add` matches `math.add`; `dd` does not match `add`.
- **Holes from p8 + permissive bidirectional checker.** Lexer is total; Menhir incremental-API recovery (`parse_recover.re`) is cloned verbatim from p8. Grammar adds partial-form productions for truncated `let`, `if`, and lambda. Hole tag byte 0x0d (under language tag 'P'). `Typecheck.check_top` returns one of `Well_typed(ty)`, `Well_typed_with_holes(ty)`, or `Ill_typed(msg)`. Hard mismatches (`1 + true`, `(\x:Int. x) "abc"`) reject ingest; holey-but-consistent terms ingest with the `Type_with_holes(Ty.t)` aspect cached. Hole rules: `check ctx Hole expected → Ok` (accepts any expected type); `synth ctx Hole → Int` as best-guess. No unification yet — the planned escalation if too many sensible programs reject.
- **`has-holes:v1` derived aspect.** Pure DAG traversal from a root hash; cached at every visited node as `Has_holes(bool)`. Sub-DAGs have stable has-holes status by content addressing, so caching is correct everywhere. Aspect-value sum extends p6's with `Type_with_holes(Ty.t)` and `Has_holes(bool)`.
- **Let bindings are de-Bruijn indexed like Lam.** `Surface_ast.Let(name, rhs, body)` keeps the name for printing; `Ast.Let(rhs, body)` and `Node.Let(rhs_hash, body_hash)` drop it. `let x = 1 in x` and `let y = 1 in y` produce identical hashes — α-equivalence-by-canonicalization extended to let.
- Interface: Bonsai + js_of_ocaml three-pane web app (no server). Left pane is a collapsible namespace tree built per render from the flat dotted-string namespace; leaves show the leaf segment, short hash, and ◌ badge if `has-holes:v1` is true. Center pane is a textarea over a recovered-AST panel (re-renders the post-recovery surface AST every keystroke, with Hole nodes visually marked) over a feedback area. Right pane is the selected hash's detail view with type and aspect rows. Bootstrap seeds `math.add`/`sub`/`mul`/`inc`, `vector.add` (collides on suffix `add` for ambiguity demo), `string.greet`, `logic.implies`, and `draft.todo` (holey, demos the badge).
- Stack: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.17, Menhir 3.0 (`--table`), menhirLib, ppx_deriving, digestif, js_of_ocaml ≥ 5.6, Bonsai/Virtual_dom/Core v0.16, ppx_jane, alcotest, qcheck, qcheck-alcotest. `_opam` symlinks to `prototypes/p7-web-interface/_opam/_opam` (the inner switch under p7's nested wrapper). Build with `dune build && dune runtest && scripts/build-web.sh`; opens at `public/index.html`.

See `docs/prototypes/p9-typed-namespaces/{00-scope.md,decisions.md,open-questions.md}` for details.
