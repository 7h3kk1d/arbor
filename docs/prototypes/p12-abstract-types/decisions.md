# p12 — Decisions (ADR-lite)

Append-only. Reversals get new dated entries. Substrate-design commitments that
generalize beyond this prototype migrate to `docs/design/`; entries here are
prototype-scoped.

---

### 2026-06-04 — Unbundled, editor-enforced opacity

**Decision.** Abstract types are handled *in isolation* — no module record, no
functors, no signatures-as-label-records. Opacity is enforced in the **editing
layer** (an editing context that "opens" abstract types), not at ingest and not
by a bundled module.

**Why.** The opacity trilemma (`12` §"Unbundled abstract types and the opacity
trilemma"): flat-&-unbundled / pure-content revalidation / enforced opacity —
pick two. We keep the first two and accept editor-only enforcement (cooperative
threat model). Cryptographic / capability enforcement (mint-as-keypair) is the
deferred path to adversarial safety and stays a `12` open question.

---

### 2026-06-04 — Mint required on the opaque type; witness in the hash

**Decision.** An abstract type is `Type(Opaque{ mint, witness })`. The mint
(fresh 16-byte mark per `10`) is in the hash for **distinctness**; the witness
hash is in the hash for **soundness**.

**Why.** Without the mint, `Counter` and `Celsius` over `Int` collapse to one
type. Without the witness in the hash, a representation-type change would not
move the abstract type's identity and old/new values could be mixed.

---

### 2026-06-04 — Minimal sealing

**Decision.** On commit, a draft is sealed **iff** it fails to type-check with
`open = {}` but succeeds with `open = context.opens`. Anything that already
type-checks opaque is stored as an ordinary term.

**Why.** Separates representation-touching operations (sealed) from
abstract-level compositions (ordinary terms over `#A`) without the author
classifying by hand, and keeps the seal population minimal. Makes "implicitly
opened" ergonomic.

---

### 2026-06-04 — `Seal{ opens, ty, impl }` node; ingest checks `impl : ty` under `opens`

**Decision.** The sealed-op node carries the opened abstract-type hashes, the
external type hash, and the impl hash. Ingest's only correctness rule is
"check `impl : ty` with `open = opens`". The substrate places **no** restriction
on who may author a `Seal`.

**Why.** This is the design doc's `open #A in E : T` as a node. Ingest
deliberately does *not* enforce opacity (that is the editor's job). Raw-body
normalization (substituting `#A ↦ witness` for representation-only impls, to get
shared bodies like `\. $0 + 1`) is an **editor optimization for sharing**, not an
ingest rule.

---

### 2026-06-04 — `Ref(Hash)`, not inline-at-resolution

**Decision.** Stored terms reference other definitions by hash (`Ref(Hash)`),
departing from p9's inline-at-resolution.

**Why.** Criterion-4 ("editing one op leaves untouched consumers byte-identical")
must be *observable*, which requires the consumer to point at the op rather than
inline it; and inlining a sealed op would drag its raw body into the consumer.
Resolution still maps names → hashes at edit time; names stay out of stored nodes.

---

### 2026-06-04 — Build from scratch; minimal language; pinning only

**Decision.** Fresh codebase (no fork of p11). Trimmed p9 language (Int, Bool,
Product, Arrow; annotated lambdas; monomorphic let; `+ - mul ==`; `if`; pairs).
Consumers pin to hashes; no follow/migrate, no migration machinery.

**Why.** The unbundled abstract-types story needs none of p7–p11's accreted
machinery (Bonsai UI, records, tuples/lists, mint threads, update strategies,
Music stdlib). "We can just do pinning."

---

### 2026-06-04 — Dies-with-hash; no edit-of / mint-reuse

**Decision.** A representation change is a fresh `:abstract` (fresh mint) rebound
to the same name. No edit-of gesture, no mint carried forward. Reverses the
earlier in-scope "minimal edit-of" plan.

**Why.** The mint plays two separable roles: distinctness (wants a fresh mark)
and lineage thread (the survives-edits axis). Abstract-type **soundness depends on
neither** — it rides witness-in-hash, so a representation change yields a distinct
type whatever the mark, and old/new values cannot mix (a test asserts a v1 value
is rejected by a v2 op). Within a single checkout, lineage is already carried by
the namespace binding. Mark-survival is load-bearing only across checkouts/branches
(collaboration phase), so it is deferred. See `docs/design/10-minted-identity.md`
§"Marks that survive content edits" and `docs/design/12-type-abstraction.md`
pair-counter note.

---

### 2026-06-04 — Interface: REPL first, then a Bonsai web app

**Decision.** Built the substrate + editing-context core as a tested library,
then a REPL exposing the context state machine, then a Bonsai + js_of_ocaml web
interface. Both interfaces are kept.

**Web — browser-driven editing context.** The open/close toggle for an abstract
type lives on the type in the namespace browser (not a persistent context bar,
not a modal implement-mode); the editor shows a read-only indicator of the
current open set. Chosen for keeping the gesture anchored to the type being
worked on. Binding surfaces a live Normal/Sealed badge (minimal sealing made
visible); the detail pane renders an abstract type's derived implementation set.

**Consequence — substrate wrapped.** To let the web layer `open Core` without
`Base.Hash` shadowing the substrate's `Hash`, the `src/` library dropped
`(wrapped false)`; it is now the wrapped module `P12_substrate`, and the REPL and
tests `open P12_substrate`. No substrate code changed.
