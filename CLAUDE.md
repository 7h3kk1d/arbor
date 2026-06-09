# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository state

`arbor` is a design exploration for a content-addressed, multi-language computational substrate working through Pierce's *Types and Programming Languages* (TAPL), inspired by Unison and intended as a long-term substrate for Hazel's computational-commons vision.

Substrate design lives in `docs/design/` and endures across prototypes. Seventeen disposable prototypes have been scaffolded so far, each in OCaml/Reason with dune + Menhir + digestif (BLAKE2B) + alcotest/qcheck. p1–p9 build up the core (storage, naming, λ-calculus, types, holes, web UI); p10–p11 explore minted identity and binding history; p12–p17 are the **type-abstraction line** (abstract types → System-F → existentials → open existentials → records → translucent modules):

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
- `p15-open-existentials` — forks p14. Adds a primitive **`open`** editor gesture that extracts an existential's hidden type(s) into the namespace (binds `N.t` + `N` + named fields) — OCaml's `module M = (val e) in <rest>`. The extracted type is a minted `Abstract(mint)` (a witness-less Skolem, never unfolds; generative per open). Generalized to **n-ary open**: a module hiding several abstract types is *nested unary existentials*, peeled by composing the unary `Open` node (`Open{Open{pkg,m1},m2}`) — no substrate change. Adds **lists** (`List`, `nil`/`cons`/`fold` right-fold eliminator, `[| … |]` literals whose element type is inferred from the head) and a web UI overhaul (consolidated editor with optional bind annotation, collapsible namespace tree with inline test pass/fail badges + a sticky tally + whole-namespace filter, structure-driven "open as module" form).
- `p16-records` — forks p15. Adds the **label sort** (design/11): `Definition.t` gains `Label(Label.t)` (a label is a bare mint; sort byte `'L'`), and **records** at both levels — `Tnode.Record(list((label-hash, ty-hash)))` and `Node.Record_lit`/`Project_field` — canonical by **sorted label hash** (`{x,y}` = `{y,x}`; field identity is the label hash, names never in stored bytes). Only labels mint (no return to p10's mint-everything); the **record-type declaration is the mint site** — literals and `e#x` projection (a distinct `#` operator, avoiding the dotted-IDENT lexing clash) require already-bound labels, and an already-bound field name reuses its label (intentional sharing). Record type equality is exact (width/depth subtyping deferred). Existential packages get **record interfaces**: `open` recovers every field's name from its label (no user-supplied `providing` list) — fixing p15's over-split positional-Product limitation.
- `p17-translucent-modules` — forks p16. Fuses records with the existential line into **first-class translucent modules** (Harper–Lillibridge/Leroy, exercising design/12's "signature = binder + label-record"). **Drops** `Exists`/`Pack`/`Unpack`/unary `Open` (tags retired); **keeps** p12's unbundled `Opaque`/`Seal`/editing-context machinery alongside (orthogonal — sigs never unfold). Four new forms: `Tnode.Sig` — labeled components `Sopaque` (`type t`), `Smanifest` (`type u = Int`), `Sval`, bound by the **rank rule** (only opaques bind; the opaque at rank i in *label-hash sort order* is `TVar(i)`, so component order carries no information and `Sig` stays order-insensitive like `Record`); `Node.Struct` (type members + term members; type = fully-manifest sig); `Ascribe{impl, sg}` = **`M :> S`, the new pack** — no mint, purely structural (two branches of a factory unify at one sig), width subtyping (private members), manifest-must-match-exactly, witnesses substituted via `retarget`; **n-ary `Open{pkg, mints}`** — ONE node mints one witness-less `Abstract` per opaque component, every binding named from the sig's own labels (`N.t`, `N.u`-to-its-equation = translucency, `N.f`); and **`Open_local{k, scrut, body}`** — `open e as M in body` *inside* a term (the p15 open-question answered), **scoped like unpack, no mint** (α-equivalence preserved; occurs-check avoidance; `M#f`/`M.f`/`M.t`-annotations resolve via the resolver's new `tctx` binder-type tracking). Module-producing functors remain blocked by avoidance (sound: the witness varies at runtime); opened types are frozen (extension-in-place stays on the `Opaque`/`Seal` path). **Current prototype.**

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
    p16-records/
    p17-translucent-modules/
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
  p16-records/
  p17-translucent-modules/
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

## Current prototype (p17-translucent-modules)

Most recent prototype; the next changes will likely live here or in a successor. It is the head of the **type-abstraction line** built fresh in p12 (abstract types) → p13 (System-F `∀`) → p14 (existentials `∃`) → p15 (open existentials + lists) → p16 (label sort + records) → p17 (translucent modules). It does **not** carry p10/p11's mint-everything, threads, or update strategies; the parser is a monolithic fail-fast Menhir grammar (no holes). p14/p15's `Exists`/`Pack`/`Unpack`/unary `Open` are **gone** (tags retired, not reused); p12's unbundled `Opaque`/`Seal`/editing-context machinery is **kept alongside** the module forms.

- **`Definition.t = Term(Node.t) | Type(Tnode.t) | Label(Label.t)`** — terms, types, and labels (a label is a bare mint; field identity, never a name) in one content-addressed Store, disambiguated by sort bytes `'P'`/`'T'`/`'L'`. Types `Tnode.t`: `Int`, `Bool`, `Arrow`, `Product`, `TVar(int)`/`Forall` (System-F), `Opaque{mint, witness}` and `Abstract(mint)`, `List(elem)`, `Record(list((label, ty)))` (canonical by sorted label hash), and **`Sig(list((label, sig_comp)))`** with `sig_comp = Sopaque | Smanifest(ty) | Sval(ty)`. Term nodes: `Var`/`Lit`/`BoolLit`/`Ref`, `Lam`, `App`, `Let`, `If`, `Pair`/`Fst`/`Snd`, `Prim`, `Seal{opens, ty, impl}`, `TyLam`/`TyApp`, `Nil`/`Cons`/`Fold`, `Record_lit`/`Project_field`, and the p17 four: **`Struct`** (members `Mtype(ty-hash) | Mval(term)`), **`Ascribe{impl, sg}`**, **`Open{pkg, mints}`** (n-ary; mints in hashed bytes), **`Open_local{k, scrut, body}`**. de Bruijn throughout (α-equivalence by canonicalization).
- **The rank rule (Sig binding).** Only `Sopaque` components bind: with k opaques, the opaque at rank i in **label-hash sort order** is `TVar(i)`; every manifest/value payload sits under all k binders at once (enclosing `Forall` vars at ≥ k). Index = sorted rank, not declaration position, so component order carries no information and `Sig` is order-insensitive like `Record`. `Tnode.opaque_ranks` is the single source of truth (resolver, checker, `open_module`, pretty all use it). Manifest components don't bind — surface references inline at resolution, but the component is retained in the node (sig identity; what ascription checks; what open binds as `N.u`).
- **Ascription `M :> S` — the new pack.** No mint, purely structural (identical ascriptions share a hash; a factory's two struct branches with different witnesses unify at the one sig type). Matching: each opaque component is witnessed by the impl's type member; manifest components match exactly; value types match exactly after witness substitution (`Typecheck.retarget`, a telescope-to-telescope parallel substitution); **width subtyping** — impl members not in S are private. Forgetting (re-ascribing a sealed module to a narrower/more-opaque sig) works. A struct's type is its fully-manifest sig; inside a struct, type members are transparent to later members (resolver-side `mtypes`).
- **Open, top-level (generative) vs local (scoped).** Top-level `Open{pkg, mints}` is ONE node minting one witness-less `Abstract` per opaque component; the editor gesture binds `N.t` per opaque, `N.u` per manifest **to its equation** (translucency), `N`, `N.f` per value member — every name recovered from the sig's own labels (`src/open_module.re`: `inspect` + `open_package`; no peeling). Re-opening mints fresh, incompatible types. **Local** `open e as M in body` is a term form answering p15's open question: scoped like p14's unpack — k de Bruijn type vars over the body, occurs-check avoidance, **no mint** (α-equivalence preserved). `M#f`, `M.f` sugar, and `M.t`-annotations resolve via the resolver's **`tctx`** (binder-type tracking; side effect: `[| x |]` under a binder now infers its element type). Module-producing functors are blocked by avoidance — sound, since the witness varies at runtime; recorded with a possible closed-scrutinee relaxation in the scope doc.
- **Kept from p12–p16:** `Opaque{mint, witness}` + `Seal` + opacity-parameterized checker (open set; `whnf` unfolds an `Opaque` iff open) + editing context with minimal sealing + `Store.unsealers`; records as plain data (`{x = 1}`, `e#x`; record-type declaration is the label mint site — and in p17, so are sig declarations and struct members); lists + `[| … |]`; the `test` aspect (interface-level convention, evaluated live).
- Interface: Bonsai + js_of_ocaml three-pane web app (no server, state resets on reload). The editor's "open as module" block now **previews every binding from the sig's labels** (types included) — nothing to fill in beyond the module name; sigs get a `sig` badge and a component listing in the detail pane. Bootstrap seeds the p16 unbundled examples (Counter, Celsius/Kelvin, Tally, `step`, `Geom.Point`, `demo.nums`) plus the module tour: `CounterSig`/`counter.impl` (private `raw` member = width)/`counter.sealed`, `mk_counter` → `Box` and `Box2` (generativity), `CalSig`/`mk_calendar` → `Cal` (one sig, two hidden types), `VecSig` → `Vec` (manifest `scalar` stays `Int`), and `total` (local open inside a function).
- Stack: OCaml/Reason, dune, Menhir (`--table`, monolithic fail-fast — no recovery), ppx_deriving, **digestif.ocaml** (the pure-OCaml BLAKE2B — the C path fails under js_of_ocaml at runtime), js_of_ocaml, Bonsai/Virtual_dom/Core (Jane Street, shared p7 switch), ppx_jane, alcotest. `_opam` symlinks to `prototypes/p7-web-interface/_opam/_opam`. Build with `dune build && dune runtest && scripts/build-web.sh`; opens at `public/index.html`.

See `docs/prototypes/p17-translucent-modules/00-scope.md` for the substrate deltas, the rank-rule decision, and the findings/limits (frozen opened types, blocked module-producing functors, Sig-vs-Record unification question).
