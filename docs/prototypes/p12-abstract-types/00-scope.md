# Phase 12 — Abstract types via editor-enforced opacity

**Status:** Scope doc for the twelfth prototype.
**Design context:** `../../design/12-type-abstraction.md` (esp. §"Unbundled abstract types and the opacity trilemma"), `../../design/10-minted-identity.md` (mint for distinctness), `../../design/03-content-addressing.md` §"Hashing types as well as terms", `../../design/04-naming-layer.md` (names resolve to hashes at edit time; names stay out of stored programs), `../../design/06-architecture.md` (four-layer decomposition).
**Prototype design lives here:** `docs/prototypes/p12-abstract-types/`.
**Implementation code lives at:** `prototypes/p12-abstract-types/`.

## Thesis

p12 is the first prototype to exercise **type abstraction** — the `12` region. It deliberately takes the **unbundled** path from that doc: abstract types *in isolation*, with no module record, no functors, no first-class modules, and no signatures-as-label-records (the p10/p11 record machinery is **not** reused). "Functors are a separate aspect with record-style stuff" — out of scope here.

The novelty is **where opacity lives**. It is *not* enforced at ingest (the substrate happily accepts any sealed operation) and *not* carried by a bundled module. It lives in the **editing layer**, via an explicit notion of an **editing context** that "opens" abstract types. This is the `12` §"Unbundled abstract types and the opacity trilemma" lean made real: keep flat-&-unbundled + pure-content revalidation, and locate opacity in the editor (cooperative threat model).

**Build from scratch.** p7–p11 accreted a Bonsai UI, records, tuples/lists, mint threads, update strategies, and a Music stdlib — none of which the unbundled abstract-types story needs. p12 starts clean with the smallest substrate that makes the editing-context mechanic real. No migration machinery: consumers **pin** to hashes (per the "just do pinning" directive); follow/migrate cascades are out.

Three substrate pieces plus one editor concept:

1. **Opaque type definitions** — a `Type` node `Opaque{ mint, witness }`. The **mint** gives distinctness (`Counter` over `Int` ≠ `Celsius` over `Int`); the **witness in the hash** gives soundness (changing the representation type changes the abstract type's identity).
2. **Sealed operation nodes** — `Seal{ opens, ty, impl }`, a thin wrapper. `impl` is a term (structurally shared like any term); `ty` is the external type over the opaque hash(es) in `opens`. The substrate's `Seal` *is* the design doc's `open #A in E : T`.
3. **Opacity-parameterized type checker** — `check`/`synth` take an **open set** of abstract-type hashes. An `Opaque{witness}` unfolds to its witness iff its hash is in the open set; otherwise it is rigid (equal only to itself). The default open set is empty → full opacity.
4. **Editing context** (the editor concept) — ephemeral editor state carrying an open set. Authoring inside a context that opens `#A` lets you write representation-touching terms with `#A` *implicitly* treated as its witness; on commit they become `Seal` nodes. Two gestures create opening contexts; the default context opens nothing.

The **unsealing set** of an abstract type (its `Store.unsealers`) is **derived, not stored**: every `Seal` that opens `#A`, found by scanning. There is no module record bundling them, and the grouping is not the namespace hierarchy. It is deliberately *broader than the type's operations* — it also catches any internal definition authored against the representation (e.g. internal tests); the substrate draws no operation-vs-test distinction. See `docs/design/12-type-abstraction.md` §"The unsealing set is broader than the operations."

## Questions this prototype should answer

1. Does **implicit opening** feel right — writing `incr = \x: t. x + 1` inside an opening context and having it silently become a sealed op, never typing `open`?
2. **Minimal sealing**: is the rule "seal only the terms that genuinely need the unfold (fail to type-check with `t` opaque); store everything else as a normal term" a clean, predictable boundary? Does it correctly separate representation-touching ops (`empty`, `incr`, `get`) from abstract-level compositions (`bump2 = incr ∘ incr`, which stays an ordinary term)?
3. Does "**open a new editing context** to add an operation later" feel like a natural, scoped capability, distinct from ordinary editing?
4. Is **editor-enforced opacity legible**? Can the user tell when they are in an opening context (transparency on) vs. the default (opaque)? If the prototype lets you hand-write a seal, is its un-sanctioned status surfaced?
5. Does the unbundled model reproduce `12`'s **dependency criteria** without a record — e.g., editing one sealed op leaves abstract-level consumers byte-identical (criterion 4)?

## Substrate model

### Definitions and types
- `Definition.t = Term(Node.t) | Type(Tnode.t)`. First-class content-addressed types (the p9+ "hashing types" line). Disambiguated by leading byte.
- `Tnode.t = Int | Bool | Product(Hash, Hash) | Arrow(Hash, Hash) | Opaque{ mint: Mint.t; witness: Hash }`. Compound types reference component types **by hash** (sharing + type aliasing via the namespace). Stable tag bytes.
- **Mint.** A fresh 16-byte mark drawn when an abstract type is created (per `10`), placed *inside* the `Opaque` hash. Re-creating "the same" abstract type mints fresh (generative). **Dies-with-hash:** a representation change is a fresh authoring (new mint), rebound to the same name — no edit-of / mint-reuse. Soundness rides witness-in-hash, distinctness wants a fresh mark, and within-checkout lineage is carried by the namespace; mint-persistence (the `12` *pair-counter* / p11 mint-thread reading) is load-bearing only across checkouts and is deferred to the collaboration phase. See `decisions.md` (2026-06-04).

### Nodes (`Node.t`, term sort)
`Var(idx)`, `Lit(int)`, `Bool(bool)`, `Lam(ty_hash, body)`, `App(f, x)`, `Let(rhs, body)`, `Pair(a, b)`, `Fst(p)`, `Snd(p)`, `If(c, t, e)`, primitive ops (`+ - mul ==`), `Ref(Hash)`, and the new **`Seal{ opens: Hash list; ty: Hash; impl: Hash }`**.
- de Bruijn for `Lam`/`Let` (α-equivalence by canonicalization).
- **`Ref(Hash)` — a departure from p9's inline-at-resolution.** Stored terms reference other definitions by hash, not by inlining. This makes the criterion-4 dependency story directly observable (a consumer *points at* `#incr`, so editing `#decr` leaves it byte-identical) and avoids inlining a sealed op's raw body into a consumer. See `decisions.md`.

### Type checking with opacity
- `synth : ctx -> open:HashSet -> Node -> Ty` and `check : ctx -> open:HashSet -> Node -> Ty -> result`. Type equality unfolds `Opaque{witness}` to `witness` **only** when that opaque's hash ∈ `open`; otherwise rigid. Unfolding applies uniformly to annotations and expected types.
- **Seal ingest rule.** To register `Seal{opens, ty, impl}`: check `impl : ty` with `open = opens` (opaques in `opens` unfold). If it holds, `Type_of(seal) = ty` under the **default (opaque)** view. The substrate does **not** otherwise restrict who may author a `Seal` — opacity is the editor's job (the trilemma resolution).
- **Composition is opaque.** A consumer applying sealed ops type-checks with `open = {}`: `#empty : #A`, `#incr : #A → #A`. `#A` is rigid; no unfold.

### Namespace
- A separate `name → hash` table; resolution at edit time; names never appear in stored nodes (`Ref` carries a hash). Dotted names with exact match (longest-suffix resolution optional, carried from p9 only if cheap). **Type identity is independent of names**: binding two names to *distinct* opaque hashes gives distinct types; binding two names to the *same* opaque hash is aliasing.

## Editing-context model (the heart of p12)

A context is ephemeral editor state:

```
context = { opens : set(Hash) }   -- abstract types transparent here (+ in-flight drafts, interface-dependent)
```

- **Default context** — `opens = {}`. Abstract types opaque. Ordinary terms and consumers authored here.
- **Create abstract type(s)** — mint, build `Opaque{mint, witness}`, ingest, bind name → opaque hash, and **add the new opaque hash(es) to the current opening context**. "As in a traditional module," several may be created at once and are opened together.
- **Open existing abstract type** — add an existing opaque hash to a fresh opening context. This is the "future operation needs internal structure → open a new editing context that opens it" gesture.
- **Authoring + commit (minimal sealing):**
  1. Type-check the draft with `open = {}` (opaque). If it succeeds and matches the annotation → store as a **normal term** (it never needed transparency). Bind name → term hash.
  2. Else type-check with `open = context.opens`. On success → **seal**: ingest the impl term and a `Seal{ opens = the opaques actually used, ty = annotated external type, impl = impl-hash }`. Bind name → seal hash.
  3. Else → type error.
- **Implementation set = derived.** `impl_set(#A) = { seal | #A ∈ seal.opens }`, by scanning. No stored bundle, no namespace subtree, no module node.

### Minimal sealing — why it matters
It draws the line between *representation-touching operations* (need the unfold → sealed) and *abstract-level compositions* (`bump2`, `readout` → ordinary terms over `#A`) **without the author classifying by hand** — the checker decides, by whether `open = {}` already suffices. This is the prototype's bet for making "implicitly opened" ergonomic without leaking seals everywhere. Worked cases:
- `empty = 0 : t` → fails at `open={}` (`Int ≠ #A`), succeeds at `open={#A}` → **sealed**.
- `incr = \x: t. x + 1` → fails at `open={}`, succeeds at `open={#A}` → **sealed**.
- `bump2 = \c: t. incr (incr c)` → succeeds at `open={}` (`#incr : #A→#A`) → **ordinary term** `\. #incr (#incr $0)`. Exactly the doc's `#B`.

### Raw-body sharing (internal/external split)
For representation-only impls (the common ops), the editor **normalizes** the impl by substituting each opened `#A ↦ witness`, yielding a witness-typed raw body (`incr`'s impl becomes `\. $0 + 1`, shared with any `Math.inc` and with every increment-on-`Int`). The Seal's `ty` keeps the opaque view. Normalization is an editor optimization for sharing, **not** a correctness requirement — ingest's rule is simply "check `impl : ty` under `opens`". Impls that *both* touch the representation *and* call a seal are the case where normalization does not fully apply; see `open-questions.md`.

## Language

Minimal but enough for interesting witnesses (`Counter` over `Int`; pair-counter over `Product`):

```
e ::= x | n | true | false
    | \x: T. e | e e | let x = e in e
    | if e then e else e | (e, e) | fst e | snd e
    | e + e | e - e | e mul e | e == e | (e)

T ::= Int | Bool | T -> T | T * T | <name>    -- <name> resolves to a concrete or opaque type hash
```

- Annotated lambdas; monomorphic `let`; bidirectional check (a trimmed p9 — no String, no holes, no `/`/`mod`; add only if a motivating example needs them).
- `mul` keyword (because `*` is the product-type constructor), as in p9.
- **No surface `open`/`seal` keyword** — sealing is implicit, driven by the editing context. That is the whole point.

## Interface — REPL **and** Bonsai web (both built)

The substrate + editing-context core is interface-independent and was built and tested first. Two interfaces drive it:

- **REPL** (`bin/repl.re`): `:abstract`/`:open`/`:close`/`:let`/`:ctx`/`:impl`/`:show`/`:ls` + bare-expr eval. The lightest way to exercise the context state machine.
- **Bonsai + js_of_ocaml web app** (`web/`, entry `webmain/main.ml`): three panes — namespace browser, editor + editing-context indicator, detail. The editing context is **browser-driven** (per the 2026-06-04 design choice): each abstract type carries an open/close toggle in the namespace tree; the editor shows the open set; binding shows a live Normal/Sealed badge; detail shows an abstract type's derived unsealing set. Bootstraps the Counter example on load.

## In scope

- Opaque type defs (mint + witness-in-hash); `Seal` nodes; opacity-parameterized checker; minimal-sealing commit rule; raw-body normalization for sharing.
- Editing-context state machine: default vs. opening contexts; create-type and open-existing gestures; implicit sealing.
- Derived unsealing-set query (`Store.unsealers`); `Ref`-by-hash dependency; pinning only.
- Representation change via fresh authoring (dies-with-hash; no edit-of), with the soundness boundary it implies (old-rep value rejected by new-rep op).
- Test suite: opaque-hash distinctness (Counter ≠ Celsius over the same witness); witness-in-hash (representation change moves the hash); seal ingest type rule; minimal-sealing classification; opaque composition (consumer type-checks at `open={}`, cannot unfold); unsealing-set derivation; criterion-4 (editing one op leaves a consumer byte-identical); raw-body sharing; evaluation (abstraction erased); representation-change soundness (fresh mint, old value rejected by new op).

## Out of scope

- Modules / functors / first-class modules / records / signatures-as-label-records (the `12` bundled design; explicitly deferred).
- Cryptographic / capability enforcement of opacity. **Editor-enforced only** (cooperative threat model); the mint-as-keypair path stays a `12` open question.
- Follow/migrate cascades, full mint threads (p11), binding history.
- Holes (p8/p9), translation, multi-language, persistence.
- Abstract-over-abstract witnesses, recursive abstract types (open questions).

## Relationship to design docs

- `12` §"Unbundled abstract types and the opacity trilemma" — this prototype *is* that section made real; findings feed back there.
- `10` — mint for distinctness; minimal edit-of for cross-witness lineage.
- `03` §"Hashing types as well as terms" — opaque types as first-class `Type` defs; witness-in-hash is the sharp consequence.
- `04` — names resolve to hashes at edit time; opacity is **not** the namespace.
