# Architecture

**Status:** Draft. Supersedes the earlier "Core vs Interface" framing with a more explicit four-layer decomposition.

## Purpose

Draw the internal structure of the system and the boundaries between its parts. Name the layers, their responsibilities, and the dependency rules that keep interfaces pluggable and languages additive.

## The four layers

From the bottom up:

1. **Store.** Content-addressed definition storage. Holds `Definition.t` values (a sum type over the enumerated languages) keyed by `Hash.t`. Enforces the "no cross-language references" invariant.
2. **Attachment.** Aspect store + namespace(s). Holds typed `AspectValue.t` entries keyed by definition hash + aspect identifier, and `Hash.t` entries keyed by name. Provides bidirectional queries.
3. **Language.** Per-language modules (AST types, canonicalizers, type-checkers, evaluators, primitives) and inter-language translators. Expose pure functions consumed by Store and by interface-level code.
4. **Interface.** User-facing modalities. Manages draft state, presentation, and orchestration of lower-layer operations.

**Dependency rule: upward only.** A layer may depend on layers below it. Lower layers never depend on higher ones. The one nuance: translators logically live in the Language layer but read from Store and write to both Store and Attachment when running — that is expected and does not violate the rule, because reads/writes flow downward.

## Layer 1: Store

The definition store. All state about what definitions exist lives here.

- **Typed values at the boundary.** The API takes and returns `Definition.t`. No byte arrays cross the boundary. Internally the hashing implementation surely produces bytes under the hood; externally the public API is "give me a well-formed definition, get its hash; give me a hash, get its definition."
- **Language enumeration is visible to Store.** `Definition.t` is a sum over the registered languages. Adding a language means extending this sum. We accept this coupling during bootstrap; future plugin support is noted as a long-term concern.
- **Canonicalization is dispatched.** Registering a definition canonicalizes it by calling into the appropriate Language module's pure canonicalizer, then hashes the canonical value. Store does not own canonicalization rules; it orchestrates them.
- **Invariant enforcement at registration.** Store checks, via dispatch, that every `Ref(hash)` inside a definition resolves to a definition whose language matches the referring language. Attempts to register a cross-language reference are rejected here.
- **API shape** (not pinned down; sketch):
  - `register(Definition.t) → Hash.t` — canonicalize, validate, hash, persist.
  - `lookup(Hash.t) → Definition.t option` — retrieve by hash.
  - `exists(Hash.t) → bool` — cheap presence check.
- **Content is immutable.** A definition's hash identifies it forever. Store never mutates an existing entry.

## Layer 2: Attachment

The aspect store and the namespace. The two subsystems are structurally kin (see `02-definitions-and-derived-data.md` and `04-naming-layer.md`) but kept as separate modules inside this layer.

- **Typed values at the boundary.** Aspect entries are `(Hash.t, AspectId.t, AspectValue.t)`. `AspectValue.t` is a sum over aspect categories, with variants whose payloads may be language-specific (e.g., a `static-type` variant carries a type from some language's type system). Namespace entries are `(name-string, Hash.t)`.
- **Bidirectional queries.** Hash → aspects; aspect-value → hashes; name → hash; hash → names. Indexing is an implementation concern; the interface shape is symmetric.
- **Asserted vs. derived** is a property declared on the aspect, enforced by this layer (asserted entries are mutable; derived entries are immutable for a given procedure identity). See `02`.
- **Doesn't understand aspect semantics.** Attachment doesn't know what a static type *means*; it just stores and retrieves the typed value.
- **Dependency on Store.** Attachment holds `Hash.t` values and relies on Store to resolve them when needed, but does not itself modify Store.

## Layer 3: Language

Per-language modules plus translators. The semantics layer.

- **One module per language.** Each language module owns its `AST.t`, its canonicalizer, its type-checker (if any), its evaluator (if any), and its primitives. Languages do not share ASTs; cross-language reuse goes through translation.
- **Pure functions.** Canonicalization, type-checking, and evaluation are pure functions: they take a definition (or an AST and some context) and return a result. They do not mutate Store or Attachment. Callers (Store for canonicalization at registration time; interfaces for type-checking and evaluation on drafts) invoke them.
- **Translators are cross-cutting within this layer.** A translator for L1 → L2 imports both language modules. It reads from Store (to find transitive dependencies in L1), produces new L2 definitions, and writes to Store + Attachment (the target definition + a `translation-to-L2:<translator-identity>` aspect on the source). Translators are the only Language-layer code that spans multiple languages.
- **Procedure identity** (from `02`) is declared here. A language's type-checker has an identity like `stlc:type-check:v1`; translators have identities like `stlc-to-stlc+subtyping:translate:v1`.

## Layer 4: Interface

Everything the user sees and touches. Plural by design: multiple interfaces may coexist.

- **Owns draft state — for today's languages.** The substrate does not track drafts. An interface offering a scratch-buffer affordance manages its draft in its own state, using pure Language-layer procedures for live feedback. Commits turn drafts into `register` calls. This is phase-specific: **the long-term direction is for certain languages to encode their draft state as first-class constructs** (typed holes à la Hazel being the canonical example). For those languages, an "incomplete" program is a well-formed program in a richer language; the interface registers it normally and the substrate understands it natively. The draft/committed distinction dissolves for hole-aware languages.
- **Owns presentation.** How multiple translators are surfaced (per the "potential actions" framing in `05-translation.md`), how outdated references display, how "rename all callers" or "upgrade all callers" is run — all interface concerns.
- **Owns orchestration.** Compound actions (rename + upgrade callers + re-translate dependents) compose lower-layer calls. The substrate offers primitives; interfaces string them together.
- **Owns name-resolution context.** Conventions for hierarchical names, language qualifiers, suffixes — all interface-side.

## Pure vs. stateful operations

Across Store, Attachment, and Language, the API splits cleanly into two tiers:

- **Pure** — given a definition (or AST + context), return a result. Canonicalization, type-checking, evaluation, translation-of-a-single-AST. Callable on drafts with no substrate state change.
- **Stateful** — register a definition, write an aspect entry, bind a name. Callable at commit points.

The split matters because structured editors and notebook-style interfaces can run live type-checks and evaluations on works-in-progress via the pure tier, without polluting the store. It also means "live feedback" does not depend on the substrate supporting incomplete-program hashing (deferred per `03-content-addressing.md`).

Note that the pure tier's role in supporting draft state shrinks as hole-aware languages come online: for those languages, incomplete programs are ordinary stored programs, and live feedback can run via stateful operations on them directly. The pure tier remains useful for preview-style operations that shouldn't commit anything, regardless of whether the target language is hole-aware.

## Multi-interface coexistence

- **Concurrent reads are free.** Content-addressed storage supports them naturally.
- **Concurrent writes.** Allowed, but we make no guarantees about reconciliation in the bootstrap phase. Last writer wins on namespace conflicts. Revisit when it's a real problem.
- **No interface-specific state in lower layers.** Store, Attachment, and Language know nothing about which interface called them.

## Transport

Whether the substrate is linked in-process as a library, run as a local daemon, or accessed over a network is a tech-stack question deferred to `08-tech-stack.md`. The API shape (typed values, pure/stateful split) is written so that any of those is reachable without semantic change.

## Accepted costs

- **Language enumeration coupling.** Adding a language means extending `Definition.t` and whatever `AspectValue.t` variants it introduces. Single-codebase bootstrap makes this acceptable; plugin support is a long-term concern.
- **Attachment knows about `AspectValue.t`.** Adding a new aspect category with a novel payload means extending the sum. Same bootstrap tradeoff.
- **Translators are somewhat cross-cutting.** They live in Language but touch Store and Attachment. This is the one place the clean layering leaks slightly; we accept it because translation inherently spans languages.

## Non-goals (current phase)

- **A rich first interface.** One prototype is plenty — likely scratch-buffer-style, possibly a thinner CLI first.
- **Pluggable languages.** The four-layer shape tolerates eventual plugins, but we don't build the plugin machinery now.
- **Concurrent-writer reconciliation.** Last-writer-wins.
- **Incremental re-checking in lower layers.** Interfaces cache their own pure-procedure results if they want live feedback.
- **Notifications / subscriptions from lower layers to interfaces.** Interfaces poll or act as needed.

## Open sub-questions

Tracked in `open-questions.md` under "Architecture" and "Interfaces."

- **First interface shape.** Scratch-buffer-style or a thinner CLI (define, translate, evaluate, lookup) first, to stress-test the API?
- **Concrete API signatures.** Deferred until the host language is picked (`08-tech-stack.md`).
- **Plugin architecture for languages.** Long-term goal; unclear when the single-codebase model stops being acceptable.
- **Where `AspectValue.t` variants live.** In Attachment (so it's one big sum)? Contributed by Language modules (making Attachment's type open)? Small architectural question with real implications for how languages are added.
- **Live-feedback conventions.** Cancellation tokens, timeouts — shared or per-interface?
- **Reactive updates.** If interfaces want to subscribe to store changes, what does that look like? Deferred until more than one interface exists.
