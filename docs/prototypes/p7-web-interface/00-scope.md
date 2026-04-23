# Phase 7 — Browser-only web interface over the p6 substrate

**Status:** Scope doc for the seventh prototype.
**Design context:** `../../design/04-naming-layer.md` (names as editing-layer concern; no silent breakage), `../../design/06-architecture.md` (four-layer decomposition; pure vs. stateful tiers), `../../design/05-translation.md` (potential actions; partial translators; translators with extra inputs), `../../design/02-definitions-and-derived-data.md` (aspects; procedure identity).
**Prototype design lives here:** `docs/prototypes/p7-web-interface/`.
**Implementation code lives at:** `prototypes/p7-web-interface/`.

## Thesis

p1–p6 each shipped a REPL. The REPL has been enough to shake out substrate mechanics — content addressing, aspects, namespaces, translators, typechecking — but it makes some of the substrate's most distinctive properties *harder to see* than they should be. p7 is the first prototype that tests whether the four-layer substrate carries into a structured UI without breaking. The substrate is vendored from p6 unchanged; what's new is the **Interface tier** (`06-architecture.md` Layer 4): a browser-only Bonsai app compiled through `js_of_ocaml`.

Four substrate properties the REPL blurs and a UI can dramatize:

1. **Names are an editing-layer concern** (`04-naming-layer.md:23–33`). A REPL collapses authoring, storage, and naming into a single submission per line. p7 separates them: well-formed authored terms ingest into the Store on every debounced keystroke (unnamed), and binding a name is the sole deferred stateful action. The visible UX is **store-as-default, name-as-action** — which is the substrate's stance stated plainly.
2. **"No silent breakage"** (`04-naming-layer.md:46–54`). Rebinding a name leaves existing callers pinned to the old hash. In the REPL this is visible by typing `:list` twice; in p7 the rebind dialog enumerates direct callers and spells out that they remain pinned before confirming.
3. **"Potential actions"** for translators (`05-translation.md:9–18`). Multiple translators per pair, partial ones, ones with extra inputs. p7's Detail view lists every applicable translator and every cached variant side-by-side.
4. **Aspect-store browsability.** `Attachment.entries_for` and `Attachment.by_value` support queries like "every definition with type `Bool → Bool`." p6 has the API but no UX; p7 surfaces aspect chips on the Detail view that back-filter into the list on click.

### Questions this prototype should answer

1. Does the `Store`/`Attachment`/`Namespace`/`Resolver` API shape survive being consumed by a structured UI, or does the mutable-`Hashtbl` interior force accommodations (immutable snapshots, indirection, duplicate APIs)?
2. Does *ingest-always, name-explicitly* dramatize "names are an editing-layer concern" the way we expect, or does it produce unacceptable noise in the Store (enough to warrant soft-GC or a design change)?
3. Is "no silent breakage" observably useful via the rebind dialog, or does it feel like pointless friction?
4. How much friction does `js_of_ocaml` add to keeping the substrate alive — specifically, does swapping to `digestif.ocaml` for the BLAKE2B backend reproduce p6's hashes byte-for-byte?
5. Does a clickable-child-hash pretty-printer change how the user understands the DAG relative to p6's REPL `:show` / `:lookup`?

## Languages

Both languages are inherited verbatim from p6:

- **Untyped lc** (`lang = Lc`): as in p4/p5/p6.
- **Simply-typed lc** (`lang = Stlc`, TAPL Ch. 8+9): pure λ→ over Bool with native `true`/`false`/`if`, every lambda annotated.

Behind the same `Definition.t = Lc | Stlc` sum. Same tag-byte hash-space separation (`'L'` / `'S'`). Same translators (`stlc-to-lc:erase-church:v1`, `lc-to-stlc:check:v1[ty=<hex8>]`). Same type-check aspect (`stlc:type-check:v1`).

## What "done" looks like

- `dune build` succeeds in the prototype directory (native targets).
- The js_of_ocaml build target `bin/p7_main.bc.js` produces a loadable JS bundle.
- `public/index.html` loads in a modern browser (Firefox/Chrome/Safari) and the 13-step walkthrough in the plan file runs clean, end to end, in a real browser. See `../../.claude/plans/okay-we-have-made-polymorphic-hejlsberg.md` §Verification.
- First page load shows a non-empty browser list seeded from `web/bootstrap.ml` — lc combinators (`i`, `id_lc` aliasing, `k`, `s`, `omega_w`), stlc fundamentals (`id_bool`, `not`, `and`, `or`, `const_true`, `not_not`), and two worked applications (`not_true`, `and_true_false`).
- `test/test_substrate.re` (p6's alcotest/qcheck suite, carried over verbatim) passes under native build.
- `test/test_jsoo.ml` (new) compiles under `(modes js)` and, when run under `node`, prints canonical hashes that match byte-for-byte with the same definitions ingested by p6's native REPL. This proves `digestif.ocaml` is a drop-in for `digestif.c` on our inputs.

## Out of scope (record in `open-questions.md`)

- Persistence of any kind. Reload resets state. No `localStorage`, no IndexedDB, no server.
- Multi-user or shared state.
- URL-as-state / deep-linking.
- Graph visualization of the DAG as a node-link diagram.
- Rebind-and-upgrade-callers refactor tooling. p7 surfaces the pinning; it does not offer to rewrite callers.
- Transitive caller scan for the rebind dialog. Direct callers only; transitive is recorded as an open question.
- Keyboard accelerators. p7 is mouse-only.
- An author-view "commit variants" workflow. Bind clears the bind-as field but keeps the buffer; users can bind again manually.
- Structured / projectional editing (Hazel-style). p7 is plain-text surface syntax.
- A dedicated aspects-by-value explorer pane. Aspects-by-value queries are surfaced as filter chips that push into the browser filter; a dedicated pane is deferred.
- Soft-GC of un-named hashes. p7 accumulates every well-formed intermediate; if this becomes unworkable, the answer likely feeds back into substrate-level GC design.
- Hole-aware languages / structure-editing calculi (`07-hazel-substrate.md` research lines). Not in scope.
- Collaborative editing (Grove). Not in scope.
