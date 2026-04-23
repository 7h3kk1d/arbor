# p7 decisions

Append-only, ADR-lite. Reversals get new dated entries rather than edits to old ones.

## 2026-04-23 — Browser-only via `js_of_ocaml`, no server

The substrate library compiles to JavaScript and runs entirely client-side. No HTTP server, no persistence story beyond in-memory state. Rejected: server + TS/JS client (more machinery; doesn't answer a question p7 needs to answer) and Melange + ReasonReact (substrate reuse under Melange requires more adaptation than `js_of_ocaml`). p7 exists to answer interface-layer questions, not persistence or multi-user questions. Full state resets on reload are acceptable per bootstrap-phase policy in `CLAUDE.md`.

## 2026-04-23 — Bonsai for the UI framework

Rejected: `incr_dom` (smaller but weaker composition story), Brr + hand-rolled state (too much UI code for a prototype), vanilla `js_of_ocaml` bindings (ditto). Bonsai's guide and examples are OCaml-first, its state-machine + effect model fits the version-counter pattern we need for mutable substrate, and it is exercised at scale by Jane Street so the stack is not exotic.

## 2026-04-23 — Vendored copy of p6's `src/`; p6 stays frozen

`cp -r prototypes/p6-stlc/src prototypes/p7-web-interface/src`, then one edit: `src/dune` — library renamed to `p7_web_interface_substrate`, `(libraries digestif)` swapped to `(libraries digestif.ocaml)`. No other substrate edits. Disposable-prototype discipline per `CLAUDE.md`; a shared library or symlink would couple p7's substrate evolution to p6 and muddy the "each prototype is its own experiment" story.

## 2026-04-23 — UI in OCaml (`.ml`); substrate stays Reason (`.re`)

`web/` and `bin/` are written in OCaml. The substrate (`src/`) stays in Reason unchanged. Rationale: Bonsai's guide, examples, and ppxes (`ppx_jane`, `js_of_ocaml-ppx`) are OCaml-first; refmt + `ppx_jane` is a known friction area. The substrate-UI boundary is a natural module boundary anyway, so switching syntax there is costless. The substrate only uses `ppx_deriving.std`, which composes cleanly under both syntaxes.

## 2026-04-23 — State pattern: mutable substrate outside Bonsai + `version : int` counter inside

`Substrate.global : { store; att; ns; step_limit }` is a module-level `let`-bound record with mutable fields. Every action that mutates one of those fields bumps a `version : int` held inside Bonsai's model; views memoize on `version` (plus their own local state). Rejected: freezing the substrate into immutable `Map.t` snapshots on every commit (would require a duplicate of every substrate API). Accepts: Bonsai can't memoize reads against substrate content, so every version bump invalidates every view. Fine at prototype scale (tens to hundreds of definitions, not millions).

## 2026-04-23 — Ingest-on-keystroke into the real Store; Bind is the only deferred stateful op

On every debounced keystroke, parse → resolve → canonicalize → `Store.ingest_*` runs against the real `Substrate.global.store`. Well-formed authored terms populate the Store immediately, unnamed. Binding a name is a separate explicit action (Enter or button click on the "Bind as" input). Rejected: throwaway Store per keystroke to avoid Store "pollution." Rejection rationale: `Store.ingest_*` is idempotent under content addressing (re-ingesting a known term is a hashtable hit); `Resolver.resolve_*` already enforces all store invariants (stlc typecheck, no cross-language refs, no unbound names); stored-but-unnamed hashes are the substrate's *natural rest state* per `03-content-addressing.md:29`. Accepts: un-named hashes accumulate with each edit to a parseable intermediate — same accepted-cost category as stale translation sources (`05-translation.md:63–70`). UX mitigation is the "Named only" browser filter.

## 2026-04-23 — Live-feedback shows "· new" vs "· already in store"

Because ingest is real, the feedback pane can distinguish first-authoring from re-typing. Either derived from `Store.has` pre-check or `Store.size` before/after. Makes the hash indicator a meaningful UX element — it tells you whether your term was authored earlier this session, produced by a translator, or is genuinely new.

## 2026-04-23 — "Named only" browser filter is the default

With ingest-on-keystroke, a typing session of a few minutes fills the Store with parseable intermediates. The browser list's default filter is "Named only" (scope: `Named_only`). An "All hashes" toggle reveals unnamed hashes in a muted rendering. This is the UX price of the ingest-always decision.

## 2026-04-23 — Clickable child hashes via web-side rich-surface traversal

`web/child_chips.ml` mirrors `Lc_pretty.surface_of_hash_ctx` / `Stlc_pretty.surface_of_hash_ctx` but produces `Vdom.Node.t` decorated with each subterm's hash. Rejected: extending the substrate `Pretty` module to emit a span tree (couples substrate to UI) and a second pass over printed strings (brittle; the printer doesn't include hashes in its general output). Keeps substrate code free of Vdom types.

## 2026-04-23 — Rebind shows a confirmation dialog enumerating direct callers

Clicking Rebind triggers a modal: the direct callers of the old hash (from `Store.entries |> List.filter_map` — O(n)) are listed, with the explicit statement that those callers remain pinned to the old hash. Confirm commits the rebind. Surfaces `04-naming-layer.md`'s "no silent breakage" invariant as a UX moment. Rejected: silent rebind (defeats the substrate claim), transitive caller scan (noisy; deferred to open-questions).

## 2026-04-23 — In-memory only

No `localStorage`, no IndexedDB. Reload resets Store, Attachment, Namespace. Bootstrap-phase accepts full state rebuilds per `CLAUDE.md`.

## 2026-04-23 — `digestif.ocaml` backend forced by jsoo

The default `digestif` library is `digestif.c` (C stubs) which does not link under `js_of_ocaml`. `src/dune` uses `(libraries digestif.ocaml)` — pure-OCaml BLAKE2B. Hash round-trip is validated via `test/test_jsoo.ml` (node) against p6's native REPL output; any divergence is a bug in either backend.

## 2026-04-23 — Dune `lang` bumped to 3.17

Required by `bonsai v0.17`. p6 was on 3.16.

## 2026-04-23 — Bootstrap seeds a canonical demo on load

`web/bootstrap.ml` runs at `Substrate.create` time, populating the Store and Namespace with a canonical set: lc combinators (`i` / `id_lc` as an alias demonstrating many-to-one naming, `k`, `s`, `omega_w`), stlc fundamentals (`id_bool`, `not`, `and`, `or`, `const_true`, `not_not`), and two worked applications (`not_true`, `and_true_false`) the user can evaluate. Rationale: the substrate's story is about named, content-addressed definitions — an empty list on first load hides what the prototype is trying to show. `not_not` referencing `not` exercises structural sharing directly; the alias `i`/`id_lc` dramatizes names as a many-to-one mapping; `omega_w` exists so the user can demonstrate `Translation_untypable`. Reset is "reload the page" — this matches the in-memory-only posture. A `reset` button in the browser header just calls `window.location.reload()`.
