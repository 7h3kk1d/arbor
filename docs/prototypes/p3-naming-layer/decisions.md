# Decisions — p3-naming-layer

Prototype-specific decision log. Lightweight ADR format. These were settled while planning and scaffolding.

---

## 2026-04-22 — Fresh prototype, not an in-place edit of p2

Naming is introduced in a new prototype at `prototypes/p3-naming-layer/`, inheriting p2's code as a starting point. p2's `00-scope.md` explicitly listed the naming layer as out of scope; editing p2 in place would corrupt that claim as a historical artifact and blur the disposable-prototype discipline from `CLAUDE.md` and `docs/design/decisions.md`.

**Alternatives considered.** Modify p2 in place (cheaper to diff; rejected because it compromises the scope doc). Keep `Ref(hash)` as a first-class AST construct and validate the full roadmap Phase 2 in one prototype (deferred; see the inlining decision below).

---

## 2026-04-22 — Internal `Ast.t` and surface `Surface_ast.t` are structurally distinct types

The parser produces `Surface_ast.t` (includes `Name(string)`). The internal `Ast.t` remains name-free. `Store.ingest` takes `Ast.t`; the type system therefore guarantees that stored programs never carry a `Name` leaf. The surface-to-internal direction goes through `Resolver.resolve`; the internal-to-surface direction is provided by `Surface_ast.of_ast` (used by the raw printer).

**Rationale.** Keeps "stored programs carry only `Ref(hash)`; names are gone by persistence" (`docs/design/04-naming-layer.md:29-33`) as a type-level invariant rather than a runtime assertion. Two modules with aligned constructors is cheap; the alternative of a single `Ast.t` with a transient `Name` case would pass the invariant off to `assert false`, which is strictly weaker.

**Alternatives considered.** Single `Ast.t` with transient `Name` (rejected: weaker invariant, sneaky bugs possible). Keep only a surface type and eagerly resolve before any API boundary (rejected: the internal name-free type is a useful thing to have on its own for tests, evaluator, and inter-language reasoning).

---

## 2026-04-22 — Names resolve to inlined subtrees, not to `Ref(hash)` AST constructors

Resolver substitutes `Name(s)` by looking up the hash in the namespace, calling `Store.reconstruct` to get a deep `Ast.t`, and inlining it. The stored DAG uses p2's shallow `Node.t` with `Hash.t` children; no `Ref(hash)` constructor is introduced at the language layer.

**Rationale.** This prototype's thesis is the naming layer — not the rest of roadmap Phase 2. Inlining preserves p2's hashing and storage model exactly, keeps structural sharing automatic (equal subtrees always hash to the same value post-inline), and still demonstrates the "no silent breakage" invariant because rebinding does not touch any stored `Node.t`. Introducing `Ref(hash)` would conflate two orthogonal changes and bloat the prototype.

**Consequence.** Re-ingesting a deeply-named expression reconstructs and re-encodes subtrees each time. Fine at prototype scale; the hash-cons means no duplication in the Store. A later prototype that introduces `Ref(hash)` can amortize this.

---

## 2026-04-22 — Namespace is its own module, not an aspect in Attachment

`Namespace.t` is a separate type and module from `Attachment.t`. Names live only in `Namespace.t`. This matches `docs/design/04-naming-layer.md:56-65` and `docs/design/decisions.md:103-111`: aspects are keyed by `(definition-hash, aspect-id)`; names are keyed by `name-string`. The design doc says keep them separate until branching gives us a reason to unify.

**Rationale.** Tempting as it is to register a `"names"` aspect, the keying directions differ (aspect forward lookup is definition-hash → value; namespace forward is name → hash). Fusing them now would paint us into a corner at branching time, when per-namespace versioning diverges from per-definition versioning.

**Alternatives considered.** A `names:registry` aspect. Rejected per the design doc.

---

## 2026-04-22 — Rename is a derived operation

`Namespace.rename` is a convenience wrapping `unbind` + `bind` with error guards (source bound, target free, target not reserved). No atomic primitive is stored or exposed.

**Rationale.** `04-naming-layer.md:93` flagged "rename atomic vs derived" as an open sub-question with implications for history tracking. This prototype does not track history, so derived is the cheap, correct position. The convenience function keeps the interface pleasant.

**Consequence.** If history tracking is added later and rename needs to be a single event, this becomes a decision to revisit.

---

## 2026-04-22 — Reserved-keyword names are disallowed at bind time

`Namespace.bind` (and `rebind`) raises `Name_reserved(_)` if the requested name is one of `true`, `false`, `succ`, `pred`, `iszero`, `if`, `then`, `else`. The reserved list lives in `namespace.re` as a single constant used by the namespace API. The lexer separately puts the keyword rules before the identifier rule, so a word like `succ` always tokenizes as the keyword — the reserved-name check ensures that even if a user tried to bind one, resolution would be unreachable.

**Rationale.** Names are opaque strings per `04-naming-layer.md:24`, but the surface syntax has a closed set of keywords. Silently letting a user bind a name they cannot reference is worse UX than a clear error.

**Alternatives considered.** A sigil prefix (`@name`, `$name`) — rejected because the substrate has no opinion on name structure. Silent keyword precedence in the resolver — rejected because silent shadowing is worse than an upfront error.

---

## 2026-04-22 — `:list` row format shows `<hash> [names] — <body>`

Each `:list` row prints the short hash, an optional bracketed list of sorted names (omitted entirely when empty), and then a name-aware rendering of the stored node's body where child subterms with bound names collapse to their (alphabetically first) name. The top-level hash's own names are on the left, not substituted into the body.

**Rationale.** `:list`'s purpose is to show what's stored; always substituting the top-level hash would hide the hash, which is the whole point of content addressing. Brackets on the left surface the namespace without displacing the hash. Name substitution in the body makes structural sharing legible: once you bind `one := succ 0`, seeing `succ one` instead of `succ (succ 0)` recognizes the shared structure.

**Alternatives considered.** Substitute the top-level too (rejected: hides hash); only show names at the top level and never substitute into bodies (rejected: misses the structural-sharing-legibility win); require an explicit `--named` flag (rejected: this is the default view users will read).

---

## 2026-04-22 — Unnamed in-store subterms reconstruct; only truly missing hashes print `<missing h:...>`

The name-aware printer falls back to reconstructing unnamed child subterms, same as p2's raw printer. The `<missing h:...>` marker is reserved for hashes absent from the store, which cannot happen for hashes produced by `ingest` but is expressed honestly.

**Rationale.** After a rebind, the old target hash is often still in the store, just no longer bound to any name. Reconstructing it gives the user visible context (they can see what `two` contains), matching p2's existing behavior and avoiding a regression.

**Alternatives considered.** Short-hash fallback for all unnamed subterms (rejected: regression vs p2; confusing once users expect readable output). Inline only if the unnamed subterm is an atom (rejected: ad hoc).

---

## 2026-04-22 — Multi-name tiebreak: alphabetically first

When a hash has multiple names, the name-aware pretty-printer uses the alphabetically first one. `Namespace.names_of` sorts its output; `Pretty.surface_of_hash` takes the head.

**Rationale.** Deterministic and test-friendly. Aliases should still appear in `:list`'s bracket list and in `:name-of`, so the information is not lost.

**Alternatives considered.** Insertion order (rejected: hashtable-order-dependent, brittle). Show all aliases inline (rejected: too noisy in long expressions).

---

## 2026-04-22 — Hash display uses `#` prefix; `:bind` accepts either an expression or a hash prefix

Hashes render as `#<hex>` everywhere (was `h:<hex>`). Inputs still accept the legacy `h:` form for backward compatibility. `:bind <name> <arg>` now dispatches on `arg`: if it matches a hash-prefix shape (`#<hex>`, `h:<hex>`, or a bare lowercase-hex string of ≥ 4 chars), it resolves the prefix against the Store and binds the name to that hash; otherwise it parses the argument as an expression with edit-time name resolution and ingests. `:bind-hash` remains as an explicit hash-only alias when the user wants to force the hash interpretation.

**Rationale.** `#` is shorter and visually distinctive; the `h:` form collides with the REPL's identifier lexer (`h` is a valid identifier start) and made `:bind foo h:abc` awkward to tokenize conceptually. Unifying `:bind` with `:bind-hash` matches how users actually think — "give this thing a name" is one operation whether the thing is an expression or a hash in your clipboard.

**Alternatives considered.** Keep `h:` (rejected: verbose, no real affordance). Keep `:bind-hash` as the only hash-binding command and leave `:bind` expression-only (rejected: users naturally try `:bind five #abc`; making that work is a small UX win for a small implementation cost). Use a sigil other than `#` such as `@` (rejected: `#` is conventional for content hashes).

**Consequence.** Bare-hex arguments (like `f63e7408`) are interpreted as hash prefixes, which could shadow a future use of identifiers that happen to be all-lowercase-hex. Names already must start with a non-hex-only alphabetic prefix in practice; if this collision ever bites, require `#` for hash prefixes explicitly.

---

## 2026-04-22 — REPL script loader (`:load` and `--load`)

The REPL supports loading a file of commands via `:load <path>` or `--load <path>` (repeatable) on the command line. The file format is one REPL command per line; `#`-prefixed and blank lines are skipped. Each executed line echoes with a `▸ ` prefix before running. `--no-repl` suppresses the interactive prompt after loads complete. A canonical seed corpus lives at `scripts/bootstrap.repl`.

**Rationale.** Hand-typing every bind every time you start the REPL doesn't scale past five commands; a loadable script makes it realistic to explore with a nontrivial corpus and keeps the invocation reproducible. Implementing it as both a REPL command and a CLI flag covers "I want to bootstrap" and "I want to reload / add more" without a bespoke bootstrap mechanism.

**Alternatives considered.** A dune rule that builds a preloaded REPL (rejected: over-engineered for a prototype, and baking state into the executable fights the "disposable prototype" posture). Piping stdin (rejected: works but the `▸ ` echoing and the clean `:load` from inside the REPL are nicer affordances). Embedding the corpus into the binary as a default startup seed (rejected: coupling the executable to a corpus would make the prototype less malleable).

---

## 2026-04-22 — Dedicated local opam switch

p3 has its own local opam switch at `prototypes/p3-naming-layer/_opam/` with OCaml 5.2.0 and the same dependency set as p2 (alcotest 1.9.1, digestif 1.3.0, dune 3.22.0, menhir 20260209, ppx_deriving 6.1.1, qcheck/qcheck-alcotest 0.91, reason 3.17.3).

**Rationale.** Matches the convention from p2's scope doc ("fresh local opam switch inside the prototype directory") and keeps prototypes mutually isolated — a future change to p3's dependency set won't affect p2. The initial scaffold briefly used p2's switch to shorten the feedback loop; once the prototype stabilized, the dedicated switch was created.

**Consequence.** `dune build` and `dune runtest` work from inside `prototypes/p3-naming-layer/` after `eval $(opam env --switch=. --set-switch)`.
