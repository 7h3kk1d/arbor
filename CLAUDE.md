# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository state

`arbor` is a design exploration for a content-addressed, multi-language computational substrate. It works through Pierce's *Types and Programming Languages* (TAPL), draws on Unison, and is meant as a long-term substrate for Hazel's computational-commons vision.

Substrate design lives in `docs/design/` and outlasts any prototype. The prototypes are disposable, each in OCaml/Reason with dune, Menhir, digestif and alcotest/qcheck. p1–p9 build the core; p10–p11 explore minted identity and binding history; p12–p17 are the type-abstraction line; p21–p22 compare Unison's model with arbor's. p18–p20 (rich editors, data and recursion, splices) live on the unmerged `p18-rich-editors` branch. Each prototype's findings and limits are in `docs/prototypes/<name>/00-scope.md`.

- `p1-arithmetic` — register / lookup / evaluate for untyped arithmetic (TAPL Ch. 3).
- `p2-structural-sharing` — shallow DAG storage, and the Attachment aspect store with a derived eval-cache aspect.
- `p3-naming-layer` — a namespace of name ↔ hash bindings resolved at edit time; a separate `Surface_ast.t` keeps the internal `Ast.t` name-free; the "no silent breakage" invariant.
- `p4-lambda-calculus` — untyped λ-calculus (TAPL Ch. 5) with de Bruijn indices, so `\x. x` and `\y. y` share a hash. CBV reducer with a step budget.
- `p5-multi-language` — arithmetic and λ-calculus in one store (`Definition.t = Arith | Lc`), with a Church-encoding translator cached as a derived aspect. First test of `docs/design/05-translation.md`.
- `p6-stlc` — STLC over Bool beside untyped λ-calculus. Type-checking is enforced at ingest and cached as a `Type_of` aspect. Adds a total erasing translator and a partial checking translator whose target type is encoded in the procedure id.
- `p7-web-interface` — p6's substrate behind a Bonsai + js_of_ocaml three-pane UI, ingesting on every keystroke. Its opam switch is shared by later prototypes.
- `p8-holes` — forks p4 with an error-recovering Menhir parser. Unparseable subterms become a bare `Hole` leaf; all holes hash the same.
- `p9-typed-namespaces` — fuses p6, p8 and p7: one typed language with holes, hierarchical names with longest-suffix resolution, a `has-holes` aspect, and content-addressed types (`Definition.t = Term | Type`).
- `p10-minted-labels` — every definition mints a fresh mark by default, and labels become a minted sort referenced by hash from record types. Sources `docs/design/10-minted-identity.md` and `11-label-sort.md`.
- `p11-mint-threads` — a mint survives an explicit edit-of gesture as a thread identity. Adds append-only binding history and pin / follow / explicit-migrate update strategies over `callers_of`, `multi_rebind` and a cached dry-run aspect.
- `p12-abstract-types` — built from scratch. Abstract types alone: an `Opaque{mint, witness}` type node, `Seal` nodes, a checker parameterized by an open set, and an editing context that seals only the terms that need the representation. Sources `docs/design/12-type-abstraction.md`.
- `p13-system-f` — adds predicative System-F, so code can be generic over a carrier type.
- `p14-existentials` — adds closed existentials (`pack`/`unpack`) with avoidance; a factory's branches pack different witnesses at one type.
- `p15-open-existentials` — adds an `open` gesture that binds an existential's hidden type into the namespace as a witness-less `Abstract(mint)`, n-ary by nesting, plus lists.
- `p16-records` — adds the label sort and records canonical by sorted label hash. Only labels mint, at the record-type declaration. Record interfaces let `open` recover field names.
- `p17-translucent-modules` — replaces existentials with first-class translucent modules: `Sig`, `Struct`, ascription, n-ary `Open` and local open. Head of the type-abstraction line.
- `p21-unison-model` — built from scratch and outside the arbor line. An executable model of `formalism/paper/mini-unison.tex`, used as a control group.
- `p22-open-terms` — p21's object language with arbor's proposed storage: open terms plus explicit links. **Current prototype.**

## Layout

```
docs/
  design/          # substrate-level ideas, enduring across prototypes
  related-work/    # themed literature index (00-index.md is the entry point)
  systems/         # source-level reads of other implementations (00-index.md)
  prototypes/<name>/   # per-prototype scope, decisions, open questions
prototypes/<name>/     # per-prototype OCaml/Reason source
formalism/             # LaTeX (→ Agda) formalism; paper/mini-unison.tex
```

`docs/prototypes/` and `prototypes/` use the same directory names (`p1-arithmetic` … `p22-open-terms`). Each `docs/prototypes/<name>/` holds `decisions.md` (dated, append-only; reversals get new entries) and `open-questions.md`. p15–p17 kept only `00-scope.md`, with `## Findings` and `## Limits` sections; p21 and p22 have all three files.

Each prototype has a local opam switch at `prototypes/<name>/_opam/`. p4–p6 and p8 symlink it to p3's switch; p9 onward symlink to `prototypes/p7-web-interface/_opam/_opam`. From inside a prototype directory:

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
scripts/build-web.sh      # web prototypes; then open public/index.html (no server)
```

## Reading order

1. `docs/design/00-overview.md` — vision, core model, glossary, doc map.
2. `docs/design/06-architecture.md` — the four layers.
3. `docs/design/decisions.md` and `docs/design/open-questions.md`.

Other design docs are topic-specific; read them on demand. `docs/design/07-hazel-substrate.md` collects Hazel-related reuse and research lines, including Grove (POPL 2025) for collaborative editing.

`docs/related-work/` has twelve theme files plus `00-index.md`, which holds the verification convention, a priority-read list, citation corrections and negative findings. Check it before making a novelty or positioning claim, and add to it rather than re-deriving citations.

`docs/systems/` holds source-level reads of other implementations, pinned to a commit, with evidence tags (`[source-verified]`, `[verified by absence]`, `[historical — removed in …]`, `[unread]`). Several of these systems have no publication, so the source is the citable artifact.

- `docs/systems/unison/` (read @ `db60ce2`, 2026-08-07): thirteen files on store and hashing, namespace and history, caching/update/merge, what Unison deleted, branching (`13-branching.md`), the scratch file, and a census of all maintained state (`12-state-inventory.md`). Read `07-arbor-mapping.md` before claiming how arbor differs from Unison; several older claims in `docs/design/` were wrong.
- `docs/systems/jj/` (read @ `253d3be`, 2026-08-19): Jujutsu's operation log and `View`, workspaces, change IDs, first-class conflicts, and the arbor mapping. jj keeps the namespace and working sets in one content-addressed object and lets a name be conflicted.

## Conventions

- **Aspect, not kind.** Categories of associated data (types, names, translations) are *aspects*. "Kind" is reserved for the type-theory concept.
- **Typed values at substrate APIs.** No byte arrays at public boundaries; serialization is internal. Adding a language extends the `Definition.t` sum.
- **Disposable prototypes.** Each answers specific questions and may be discarded. Learnings move into `docs/design/` with the prototype cited.
- **Don't pre-solve future concerns.** Record them in `open-questions.md`. The bootstrap phase accepts full state rebuilds; don't build migration machinery.

## Four-layer architecture

From `docs/design/06-architecture.md`. Dependencies point upward only.

1. **Store** — content-addressed definition storage; rejects cross-language references at registration.
2. **Attachment** — aspect store and namespaces; bidirectional queries.
3. **Language** — per-language AST, canonicalizer, type checker, evaluator, primitives, and translators between languages.
4. **Interface** — user-facing modalities.

## Current prototype (p22-open-terms)

`docs/design/2026-09-17-design-doc.md` argues that Unison's naming-related propagation failures come from resolving a free variable to a hash at edit time and discarding the name. p22 stores open terms and links separately so that binding is a recorded step. It reuses p21's language so the comparison is controlled.

- `Def.t = Simple(open, link) | Member(bundle, i)`. `Open_term.t` has free variables as `Slot(i)`, de Bruijn binders and no names; `Link.t` is `{arity, targets}` with `target = Def | Builtin | Self | Unbound`. `Open_term.hash` is total, so a term can be stored before its names are resolved.
- Open terms are maximally shared: `\n. scale n` and `\n. shrink n` are one open term, so an unchanged open hash means the author did not edit.
- Propagation rewrites link targets with no printer, parser or name lookup. A followed dependent keeps its open hash; only its link changes. Pin, follow and explicit are one primitive (rewrite a link) over three sets of sites.
- Recursive groups close in the link: a `Bundle` over links with `Self(i)`, needing no `Fix` in the object language. For symmetric groups the bundle hash is invariant under reordering; asymmetric groups still depend on presentation order.
- `Store.intent` maps `(link hash, slot)` to every name used there, so nothing is clobbered and no name is hashed. `Propagate.follow ~via` uses it to restrict a cascade's first hop.
- One append-only operation log yields three histories: authored edits, link moves, and name history.
- Interface: Bonsai + jsoo, three panes. The buffer previews per slot what a commit would store; the detail pane shows the three histories side by side.
- 18 alcotest/qcheck cases. Stack as p21 but with `digestif.ocaml` BLAKE2B.

See `docs/prototypes/p22-open-terms/00-scope.md`.

## Previous prototype (p21-unison-model)

An executable model of `formalism/paper/mini-unison.tex` over STLC with `Nat`, `Text` and arrows. It is not part of the arbor line and contradicts several arbor positions on purpose, so the differences can be measured.

- `Σ : Hash ⇀ Component`; references are `Derived(hash, pos)` (`03-content-addressing.md`'s Option A). Component hashing uses two orderings and aborts ingest when a group's identity would depend on member names. Position 0 is transparent, but `x = 1 + 1` and `y = x` still hash differently.
- SHA3-512 over Unison's token stream, not byte-compatible with `ucm`. Terms are a named ABT, because `update` renders them back to source.
- The namespace is a relation and may hold conflicts. The watch cache `(Hash × Pos) ⇀ Term` is never invalidated.
- Ingest is group-at-a-time and gated: nothing is stored until the whole buffer typechecks. Typechecking confers identity on every keystroke; `commit` stores and binds. `src/scratch.re` holds buffer state outside `Store`.
- `update` is render → parse → typecheck → re-ingest. On failure it unbinds every name the file bound and every dependent it rendered, and returns the source to the buffer.
- `merge` (`src/synhash.re`, `src/merge.re`) runs over three namespaces sharing one store. Its tests show three ways a rename defeats the classifier.
- 45 alcotest/qcheck cases. `formalism/scripts/check-labels.sh formalism/paper/mini-unison.tex prototypes/p21-unison-model/src` checks that all 49 paper statements are mirrored.

See `docs/prototypes/p21-unison-model/00-scope.md`.

## Earlier: the type-abstraction line (p17-translucent-modules)

p12 → p17 built type abstraction from scratch: abstract types, System-F, existentials, open existentials with lists, records, translucent modules. It does not carry p10/p11's mint-everything, threads or update strategies. The parser is fail-fast Menhir with no holes.

- `Definition.t = Term | Type | Label`, sort bytes `P`/`T`/`L`; de Bruijn throughout. Types include `Opaque{mint, witness}`, `Abstract(mint)`, `Forall`, `List`, `Record` (sorted by label hash) and `Sig`.
- `Sig` uses the rank rule: only opaque components bind, as `TVar(i)` by rank in label-hash order, so `Sig` is order-insensitive. `Tnode.opaque_ranks` is the single source of truth.
- Ascription `M :> S` mints nothing and allows width subtyping. Top-level `Open{pkg, mints}` mints one `Abstract` per opaque component and binds names from the sig's labels; manifest members bind to their equations. Local `open e as M in body` is scoped like `unpack` and mints nothing.
- p12's `Opaque`/`Seal`, opacity-parameterized checker, editing context with minimal sealing, and `Store.unsealers` remain alongside the module forms.
- Limits: opened types are frozen, and module-producing functors are blocked by avoidance.
- Build: `dune build && dune runtest && scripts/build-web.sh`, then open `public/index.html`. Use `digestif.ocaml`; the C BLAKE2B path fails under js_of_ocaml.

See `docs/prototypes/p17-translucent-modules/00-scope.md`.
