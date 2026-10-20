# Architecture

**Status:** Draft. Supersedes the earlier "Core vs Interface" framing with an explicit four-layer decomposition.

## Purpose

The system's internal structure: its layers, their responsibilities, and the dependency rules that keep interfaces pluggable and languages additive.

## The four layers

From the bottom up:

1. **Store.** Content-addressed definition storage. Holds `Definition.t` values (a sum over the enumerated languages) keyed by `Hash.t`, and enforces the no-cross-language-references invariant.
2. **Attachment.** The aspect store and namespace(s). Holds typed `AspectValue.t` entries keyed by definition hash plus aspect identifier, and `Hash.t` entries keyed by name, with bidirectional queries.
3. **Language.** Per-language modules (AST types, canonicalizers, type checkers, evaluators, primitives) and inter-language translators. Exposes pure functions consumed by Store and by interface code.
4. **Interface.** User-facing modalities. Manages draft state, presentation, and orchestration of lower-layer operations.

**Dependency rule: upward only.** A layer may depend on layers below it, never above. Translators live in the Language layer but read from Store and write to Store and Attachment when they run. This does not break the rule, since those reads and writes flow downward. It is the one place the layering leaks, accepted because translation inherently spans languages.

## Layer 1: Store

The definition store. All state about which definitions exist lives here.

- **Typed values at the boundary.** The API takes and returns `Definition.t`; no byte arrays cross it. Hashing produces bytes internally, but the public API is "give a well-formed definition, get its hash; give a hash, get its definition."
- **Store sees the language enumeration.** `Definition.t` is a sum over the registered languages, so adding a language extends the sum. Like adding `AspectValue.t` variants in Attachment, this coupling is accepted for a single-codebase bootstrap; plugin support is a long-term concern.
- **Canonicalization is dispatched.** Registering a definition calls the appropriate Language module's pure canonicalizer, then hashes the canonical value. Store orchestrates canonicalization without owning its rules.
- **Invariants are enforced at registration.** Store checks, via dispatch, that every `Ref(hash)` in a definition resolves to a definition in the referring language, and rejects cross-language references.
- **API sketch** (not pinned down):
  - `register(Definition.t) → Hash.t`: canonicalize, validate, hash, persist.
  - `lookup(Hash.t) → Definition.t option`: retrieve by hash.
  - `exists(Hash.t) → bool`: cheap presence check.
- **Content is immutable.** A hash identifies its definition forever, and Store never mutates an entry.

## Layer 2: Attachment

The aspect store and the namespace, structurally kin (see `02-definitions-and-derived-data.md` and `04-naming-layer.md`) but kept as separate modules in this layer.

- **Typed values at the boundary.** Aspect entries are `(Hash.t, AspectId.t, AspectValue.t)`. `AspectValue.t` is a sum over aspect categories whose payloads may be language-specific (e.g., a `static-type` variant carries a type from some language's type system). Adding an aspect category with a novel payload extends the sum, the same bootstrap tradeoff as `Definition.t`. Namespace entries are `(name-string, Hash.t)`.
- **Bidirectional queries.** Hash → aspects, aspect-value → hashes, name → hash, hash → names. Indexing is an implementation concern; the interface is symmetric.
- **Asserted vs. derived** is declared on the aspect and enforced here: asserted entries are mutable, derived entries are immutable for a given procedure identity. See `02`.
- **No aspect semantics.** Attachment does not know what a static type means; it stores and retrieves typed values.
- **Depends on Store.** Attachment holds `Hash.t` values and relies on Store to resolve them, but never modifies Store.

## Layer 3: Language

Per-language modules plus translators: the semantics layer.

- **One module per language.** Each owns its `AST.t`, canonicalizer, type checker (if any), evaluator (if any), and primitives. Languages share no ASTs; cross-language reuse goes through translation.
- **Pure functions.** Canonicalization, type checking, and evaluation take a definition (or an AST and context) and return a result, without mutating Store or Attachment. Store calls the canonicalizer at registration; interfaces call type checking and evaluation on drafts.
- **Translators span languages.** A translator for L1 → L2 imports both language modules. It reads Store to find L1 transitive dependencies, produces new L2 definitions, and writes the target definition to Store and a `translation-to-L2:<translator-identity>` aspect on the source to Attachment. Translators are the only Language-layer code that spans languages.
- **Procedure identity** (from `02`) is declared here: a type checker has an identity like `stlc:type-check:v1`, a translator one like `stlc-to-stlc+subtyping:translate:v1`.

## Layer 4: Interface

Everything the user sees and touches. Plural by design; several interfaces may coexist.

- **Owns draft state, for today's languages.** The substrate does not track drafts. An interface with a scratch buffer keeps its draft in its own state, uses pure Language-layer procedures for live feedback, and turns commits into `register` calls. This is phase-specific. The long-term direction is for some languages to encode draft state as first-class constructs, typed holes à la Hazel being the canonical example. In those languages an incomplete program is a well-formed program, the interface registers it normally, and the draft/committed distinction disappears.
- **Owns presentation.** How multiple translators are surfaced (the "available actions" framing in `05-translation.md`), how outdated references display, and how "rename all callers" or "upgrade all callers" runs.
- **Owns orchestration.** Compound actions (rename + upgrade callers + re-translate dependents) compose lower-layer calls. The substrate offers primitives; interfaces combine them.
- **Owns name-resolution context.** Conventions for hierarchical names, language qualifiers, and suffixes are interface-side.

## Pure vs. stateful operations

Across Store, Attachment, and Language, the API splits into two tiers:

- **Pure**: given a definition (or AST + context), return a result. Canonicalization, type checking, evaluation, translation of a single AST. Callable on drafts with no change to substrate state.
- **Stateful**: register a definition, write an aspect entry, bind a name. Called at commit points.

The split lets structured editors and notebook-style interfaces run live type checks and evaluation on work in progress without polluting the store. Live feedback therefore does not depend on hashing incomplete programs, which `03-content-addressing.md` defers.

For hole-aware languages the pure tier's role in draft state shrinks, since incomplete programs are ordinary stored programs and live feedback can run on them through stateful operations. The pure tier stays useful for previews that should commit nothing.

## Multi-interface coexistence

- **Concurrent reads are free** under content-addressed storage.
- **Concurrent writes** are allowed with no reconciliation guarantees in the bootstrap phase. Last writer wins on namespace conflicts; revisit when it becomes a real problem.
- **No interface-specific state in lower layers.** Store, Attachment, and Language know nothing about which interface called them.

## Transport

Whether the substrate is linked in-process as a library, run as a local daemon, or accessed over a network is a tech-stack question deferred to `08-tech-stack.md`. The API shape (typed values, pure/stateful split) keeps all three reachable without semantic change.

## Accepted costs

- **Language enumeration coupling** in `Definition.t` and **aspect-category coupling** in `AspectValue.t` (Layers 1 and 2). Acceptable for a single-codebase bootstrap; plugin support is long-term.
- **Translators cut across layers** (see the dependency rule above).

## Non-goals (current phase)

- **A rich first interface.** One prototype is enough, likely scratch-buffer-style, possibly a thinner CLI first.
- **Pluggable languages.** The four-layer shape tolerates eventual plugins, but the plugin machinery is not built now.
- **Concurrent-writer reconciliation.** Last writer wins.
- **Incremental re-checking in lower layers.** Interfaces cache their own pure-procedure results if they want live feedback.
- **Notifications or subscriptions from lower layers to interfaces.** Interfaces poll or act as needed.

## Open sub-questions

Tracked in `open-questions.md` under "Architecture" and "Interfaces."

- **First interface shape.** Scratch-buffer-style, or a thinner CLI (define, translate, evaluate, lookup) first to stress-test the API?
- **Concrete API signatures.** Deferred until the host language is picked (`08-tech-stack.md`).
- **Plugin architecture for languages.** Long-term; unclear when the single-codebase model stops being acceptable.
- **Where `AspectValue.t` variants live.** In Attachment as one big sum, or contributed by Language modules, making Attachment's type open? A small question with real consequences for how languages are added.
- **Live-feedback conventions.** Cancellation tokens, timeouts: shared or per-interface?
- **Reactive updates.** If interfaces want to subscribe to store changes, what does that look like? Deferred until more than one interface exists.
