# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository state

`arbor` is a design exploration for a content-addressed, multi-language computational substrate working through Pierce's *Types and Programming Languages* (TAPL), inspired by Unison and intended as a long-term substrate for Hazel's computational-commons vision.

Substrate design lives in `docs/design/` and endures across prototypes. Fifteen disposable prototypes have been scaffolded so far, each in OCaml/Reason with dune + Menhir + digestif (BLAKE2B) + alcotest/qcheck. p1–p9 build up the core (storage, naming, λ-calculus, types, holes, web UI); p10–p11 explore minted identity and binding history; p12–p15 are the **type-abstraction line** (abstract types → System-F → existentials → open existentials):

- `p1-arithmetic` — minimum register / lookup / evaluate loop for untyped arithmetic (TAPL Ch. 3).
- `p2-structural-sharing` — shallow, DAG-shaped storage plus the Attachment aspect store with a derived eval-cache aspect.
- `p3-naming-layer` — first-class namespace of name ↔ hash bindings, edit-time resolution, a separate `Surface_ast.t` that keeps the internal `Ast.t` name-free at the type level, name-aware pretty-printer, and the visible "no silent breakage" invariant.
- `p4-lambda-calculus` — untyped λ-calculus (TAPL Ch. 5) with de Bruijn indices internally and named surface syntax. First substrate demonstration of α-equivalence via canonicalization (`\x. x` and `\y. y` share a hash). Carries p3's naming layer forward and adds a CBV β-reducer with a step budget for non-terminating terms.
- `p5-multi-language` — both arithmetic and λ-calculus in one Store keyed by a `Definition.t = Arith | Lc` sum. Adds a hand-written Church-encoding translator from arithmetic to λ-calculus, invoked manually from the REPL (`:translate`) and cached as a derived aspect on the arith source. First substrate demonstration of `docs/design/05-translation.md`: translator identity `arith-to-lc-church:translate:v1`, output recorded as `Translation_target(Hash.t)` under aspect `translation-to-lc`.
- `p6-stlc` — STLC (TAPL Ch. 8+9: pure λ→ over Bool with native `true`/`false`/`if`, every lambda annotated) paired with untyped λ-calculus in one Store, keyed by `Definition.t = Lc | Stlc`. Arith is dropped. First prototype with a type system: type-checking is enforced at ingest (Store invariant "every stored stlc definition type-checks") and cached as the `stlc:type-check:v1` aspect with value `Type_of(Ty.t)` — first non-`Hash.t`-valued aspect. Two translators: `stlc-to-lc:erase-church:v1` (total; erase annotations + Church-encode booleans) and `lc-to-stlc:check:v1[ty=<hex8>]` (partial; user supplies a target type, constraint-based unification decides). First worked examples of (a) partial translators via `Translation_untypable(string)` and (b) translators with inputs beyond the source via procedure-id encoding.
- `p7-web-interface` — first interface-layer prototype. Carries p6's lc + stlc substrate verbatim and adds a Bonsai + js_of_ocaml in-browser UI (no server, state resets on reload). Three-pane layout: editor, browser (filterable list), detail. Every keystroke ingests synchronously via `Feedback.compute`. The substrate compiles to a single ~26 MB JS bundle; binding a name is the only deferred stateful operation in the UI.
- `p8-holes` — forks p4 (untyped λ-calculus) and replaces the fail-fast parser with an error-recovering one built on Menhir's incremental API. Every textual input becomes a valid `Surface_ast.t`; subterms that would fail to parse are replaced with `Hole` nodes. Holes are a new leaf constructor with tag byte `'\x04'` — all holes are structurally equal, so the BLAKE2B digest is constant. First substrate answer to `docs/design/open-questions.md` §"Holes and incomplete programs": bare `Hole`, no payload.
- `p9-typed-namespaces` — fuses p6 (typed language + Type_of aspect), p8 (parser recovery + bare Hole), and p7 (Bonsai + jsoo UI). One typed surface language with annotated lambdas, monomorphic `let`, native `Int`/`Bool`/`String`/`Product` plus primitive arith/bool/string-concat operations. Hierarchical dot-delimited names (`math.add`) with Unison-style longest-segment-suffix resolution and a distinct `Ambiguous` error. Holes integrate with the type system via a permissive bidirectional checker producing three outcomes — `Type_of(ty)`, `Type_with_holes(ty)`, or `Ill_typed` (rejects ingest). New `has-holes:v1` derived aspect caches whether a term or any DAG-reachable subterm contains a hole. UI replaces p7's filterable list with a collapsible namespace tree (has-holes badges per leaf) and adds a recovered-AST panel that re-renders the post-recovery surface AST every keystroke. **Extended (2026-04-30) with content-addressed types**: `Definition.t = Term(Node.t) | Type(Ty.t)`; `Node.Lam` carries a type *hash* rather than inline bytes; `Surface_ty.t` admits `Named(string)`; the namespace binds names to type hashes (type aliasing falls out of namespace + structural canonicalization); `Type_of`/`Type_with_holes` aspect values become `Hash.t` references; UI gains a term/type mode toggle on the editor pane and renders a `T` kind-badge on type leaves. First substrate answer to `docs/design/03-content-addressing.md` §"Hashing types as well as terms."
- `p10-minted-labels` — forks p9. First prototype of **mint-everything-by-default** identity (every Term/Type/Label mints a fresh mark on creation; coincidentally-equal definitions get *different* hashes; structural sharing collapses) and **labels as a first-class minted sort** (`Definition.t = Term | Type | Label`; record types reference labels by hash, field names never in stored bytes; renaming a label is a namespace op; two record types sharing a field name resolve to the same label = intentional sharing on demand). Adds tuples, records, monomorphic lists, and a Unison-ish surface syntax (`Capitalized` namespaces/types, `lowercase` terms/labels). Sources `docs/design/10-minted-identity.md`, `11-label-sort.md`.
- `p11-mint-threads` — forks p10. Makes the mint mark a **thread identity** preserved across edits (an explicit *edit-of-X* gesture inherits X's mark; `Store.thread_of` groups versions; plain re-ingest still mints fresh), **append-only binding history** per namespace name (bind/rebind/unbind append; orphans render `name(vN)`), and **update strategies as substrate ops** — pin / follow / explicit-migrate on three primitives: `Store.callers_of` (reverse-DAG side-index), `Store.multi_rebind` (atomic bundle), and the `follow-clean:v1` cached dry-run aspect. Sources `04-naming-layer.md` §"Update strategies", `10-minted-identity.md` §"Marks that survive content edits".
- `p12-abstract-types` — **built from scratch** (sheds p7–p11's UI, records, threads, stdlib). First **type abstraction**, taking the **unbundled** path: abstract types in isolation — no modules, functors, or signatures. Opacity lives in the **editing layer**, not at ingest. Three substrate pieces + one editor concept: a `Type` node `Opaque{mint, witness}` (mint → distinctness, `Counter`≠`Celsius` over `Int`; witness-in-hash → soundness); `Seal{opens, ty, impl}` operation nodes (the design doc's `open #A in E : T`); an **opacity-parameterized checker** taking an *open set* of abstract-type hashes (an `Opaque` unfolds to its witness iff its hash is open, else rigid); and an **editing context** that opens types so representation-touching terms become Seals on commit. **Minimal sealing** seals only terms that fail to type-check with the type opaque; the **unsealing set** (`Store.unsealers`) is derived by scanning, deliberately broader than the type's operations. Sources `docs/design/12-type-abstraction.md`.
- `p13-system-f` — forks p12. Adds **predicative System-F** (`/\t. e`, `e [T]`, `forall t. T`) so a program can be generic over the carrier — a `step` functor applies to a counter of *any* internal representation. The ML-functor argument decomposes into a type argument plus a value bundle (a `Product`, the closest to a record). Quantification-abstraction composes with p12's sealing; existentials and F-omega deferred.
- `p14-existentials` — forks p13. Adds **closed existentials** (`exists t. T`, `pack [W] e as E`, `unpack [t] x = e in body`) — a value that hides its *own* representation, chosen by the producer. The driving program `mkCounter : Bool -> exists t. t * ((t -> t) * (t -> Int))` is a factory whose two `if` branches pack different witnesses (`Int`, `Int*Int`) yet unify at one `∃` type; the consumer `unpack`s and uses the ops without learning the rep. Avoidance (the witness can't escape `unpack`) is enforced. Interface bundled as a positional `Product` (no records).
- `p15-open-existentials` — forks p14. Adds a primitive **`open`** editor gesture that extracts an existential's hidden type(s) into the namespace (binds `N.t` + `N` + named fields) — OCaml's `module M = (val e) in <rest>`. The extracted type is a minted `Abstract(mint)` (a witness-less Skolem, never unfolds; generative per open). Generalized to **n-ary open**: a module hiding several abstract types is *nested unary existentials*, peeled by composing the unary `Open` node (`Open{Open{pkg,m1},m2}`) — no substrate change. Adds **lists** (`List`, `nil`/`cons`/`fold` right-fold eliminator, `[| … |]` literals whose element type is inferred from the head) and a web UI overhaul (consolidated editor with optional bind annotation, collapsible namespace tree with inline test pass/fail badges + a sticky tally + whole-namespace filter, structure-driven "open as module" form). **Current prototype.**

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
    p10-minted-labels/
    p11-mint-threads/
    p12-abstract-types/
    p13-system-f/
    p14-existentials/
    p15-open-existentials/
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
  p10-minted-labels/
  p11-mint-threads/
  p12-abstract-types/
  p13-system-f/
  p14-existentials/
  p15-open-existentials/
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

## Current prototype (p15-open-existentials)

Most recent prototype; the next changes will likely live here or in a successor. It is the head of the **type-abstraction line** built fresh in p12 (abstract types) → p13 (System-F `∀`) → p14 (existentials `∃`) → p15 (open existentials + lists). It does **not** carry p10/p11's mint-everything, records, threads, or update strategies; the parser is a monolithic fail-fast Menhir grammar (p8's hole-recovery was dropped when p12 started from scratch — there are no holes here).

- **`Definition.t = Term(Node.t) | Type(Tnode.t)`** — terms and types are first-class definitions in one content-addressed Store, disambiguated by a leading language byte. Types `Tnode.t`: `Int`, `Bool`, `Arrow`, `Product`, plus `TVar(int)` / `Forall(body)` (System-F), `Exists(body)` (existentials), `Opaque{mint, witness}` and `Abstract(mint)` (abstract types), and `List(elem)`. Term nodes `Node.t`: `Var`/`Lit`/`BoolLit`/`Ref`, `Lam(ann_hash, body)`, `App`, `Let`, `If`, `Pair`/`Fst`/`Snd`, `Prim(op, args)` (`Add`/`Sub`/`Mul`/`Eq`), `Seal{opens, ty, impl}`, `TyLam`/`TyApp`, `Pack{witness, body, ty}`/`Unpack(scrut, body)`, `Open{pkg, mint}`, and `Nil(elem)`/`Cons`/`Fold`. de Bruijn indices for both term binders and type variables (α-equivalence by canonicalization).
- **Abstract types & editor-enforced opacity (from p12).** `Opaque{mint, witness}` types with sealed operations (`Seal`); an opacity-parameterized checker takes an *open set* of abstract-type hashes (an `Opaque` unfolds to its witness iff open). The **editing context** carries that open set; **minimal sealing** seals only terms that need the unfold; `Store.unsealers` is derived by scanning. Opacity is cooperative/editor-level — the substrate accepts any sealed op.
- **`open` (the p15 headline).** A primitive editor gesture, not a term form: it extracts an existential package's hidden type(s) into the namespace, binding `N.t` (+ `N.u`, …) for each abstract type plus `N` for the value and named positional fields — OCaml's `module M = (val e) in <rest>`. The extracted type is a minted `Abstract(mint)` (witness-less Skolem, never unfolds; generative — re-opening mints fresh, incompatible types). **n-ary**: a module hiding several abstract types is nested unary `Exists`; `open` peels every leading `exists`, minting one `Abstract` per level by composing the unary `Open` node (`Open{Open{pkg,m1},m2}`) — no substrate change. `src/open_existential.re` houses `inspect` (shape: type arity + flattened operation field types) and `open_package` (mint, nest, project positionally; returns hashes, name-free).
- **Lists.** `List(elem)` type; `nil [T]`, `cons`, `fold` (right fold = the only eliminator; no general recursion), and `[| e1, …, en |]` literals — pure surface sugar resolving to the same canonical `cons`-spine, with the element type **inferred from the head** (the one bit of inference in the otherwise-explicit language; the trailing `nil` needs a concrete element type for content addressing). The `[| |]` delimiter avoids a reduce/reduce clash with type application `e [T]`.
- Interface: Bonsai + js_of_ocaml three-pane web app (no server, state resets on reload). **Editor** (center) merges a live scratch + define into one work area — type an expression, see its synthesized type + value live, then `bind` it (annotation optional — synthesized under the open set if blank; supply it to bind at an abstract type, which is what triggers sealing) or, when it is an existential package, `open as module` via a form **populated from the package's structure** (one input per abstract type, one per operation field, each labeled with its type). **Browser** (left) is a collapsible namespace tree built per render from the flat dotted names, with a whole-namespace substring filter and a sticky pass/fail tests tally; a binding marked with the `test` aspect shows a live pass/fail badge inline. **Detail** (right) shows the selected definition's type, source (pretty-printed; round-trips `cons`-spines to `[| |]`), eval, test toggle, and a one-click open affordance for existential terms. Bootstrap seeds the Counter/Celsius-Kelvin/Range/Tally abstract-type modules, the `step` System-F functor, and three existential factories — `mkCounter` (unary), `mkScale` (2 types: celsius/kelvin), `mkCalendar` (2 types: date/span, with a real "can't add two dates" safety property) — each opened into the namespace.
- **`test` aspect.** Tests are an interface-level convention: a `Bool`-typed term tagged with the generic `test` aspect in the `Attachment` store; pass/fail is evaluated live (no caching). The substrate has no test concept.
- Stack: OCaml/Reason, dune, Menhir (`--table`, monolithic fail-fast — no recovery), ppx_deriving, **digestif.ocaml** (the pure-OCaml BLAKE2B — the C path fails under js_of_ocaml at runtime), js_of_ocaml, Bonsai/Virtual_dom/Core (Jane Street, shared p7 switch), ppx_jane, alcotest. `_opam` symlinks to `prototypes/p7-web-interface/_opam/_opam`. Build with `dune build && dune runtest && scripts/build-web.sh`; opens at `public/index.html`.

See `docs/prototypes/p15-open-existentials/00-scope.md` for the full plan, the substrate deltas, and the n-ary-open finding.
