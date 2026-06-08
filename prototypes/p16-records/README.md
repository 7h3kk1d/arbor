# p16 — Records / labeled modules

Forks **p15** (open existentials) and folds in the **label sort** (design/11): a
first-class minted `Label` definition, record types/literals that reference
labels by *hash* (canonical by sorted label hash), and an existential package
whose interface is a **record** rather than a positional `Product`. The payoff
lands on `open`, which recovers field names from the record's labels:

```
mkCounter : Bool -> exists t. { empty: t, incr: t -> t, get: t -> Int }
:open mkCounter true as Counter        -- binds Counter.empty / incr / get by label name
Counter.get (Counter.incr Counter.empty)
```

Only labels mint; terms and types stay structural (no return to p10's
mint-everything). Confirmed decisions: labels mint at the record-type
declaration; projection is a distinct `p#x` operator; `Record_update` deferred.

Design and scope: `docs/prototypes/p16-records/00-scope.md` (the plan).

## Status

Forked from p15 (verbatim, renamed `P16_substrate`) and building green — at this
point **identical to p15**. The records / label-sort additions are in progress
per the plan's build order (label sort + record substrate → existentials over
records + open → surface `{…}` / `p#x` → web/bootstrap). Everything p15 had —
opaque types + seals, System-F (`∀`), existentials (`∃`, `open`), lists, surface
language, evaluator, REPL, Bonsai web, tests aspect — carries over.

Substrate core (the `src/` library, `P16_substrate`):

- `src/hash.re` — BLAKE2B content hashes (carried from p11).
- `src/mint.re` — deterministic minted marks (counter-sourced, reproducible).
- `src/tnode.re` — `Int | Bool | Product | Arrow | Opaque{mint, witness}`; content encoding (sort byte `T`).
- `src/node.re` — term nodes incl. `Ref(Hash)` and `Seal{opens, ty, impl}`; de Bruijn; encoding (sort byte `P`).
- `src/definition.re` — `Term(Node.t) | Type(Tnode.t)`; leading-byte disambiguation.
- `src/typecheck.re` — opacity-parameterized checker (`whnf`/`equal_ty` unfold open opaques); `Seal` ingest rule; witness-`normalize`.
- `src/store.re` — content-addressed ingest; `Type_of` cache; derived `impl_set` scan; no ingest-level opacity.
- `src/editing_context.re` — open set; `open_type`; minimal-sealing `commit` (ordinary term vs. seal).

`test/test_p16.re` proves: Counter≠Celsius (mint distinctness), witness-in-hash (rep change moves the type), minimal sealing (empty/incr/get sealed, `bump2` ordinary), correct external types, raw-body sharing (incr's impl == `Math.inc`), opacity (a default-context consumer cannot touch the representation), bogus-seal rejection at ingest, derived implementation set, and criterion-4 (editing `decr` leaves `bump2` byte-identical).

Surface + interface layer (built — REPL drives the editing-context model by hand):

- `surface.re` / `surface_ty.re` — surface ASTs (names; no holes).
- `lexer.mll` / `parser.mly` — trimmed p9 language, fail-fast (Menhir monolithic); no `open`/`seal` keyword.
- `parse.re` — fail-fast wrappers returning `result`.
- `namespace.re` — separate `name → hash` table (exact match; reverse index for display).
- `resolver.re` — names → hashes at edit time; surface → internal (de Bruijn); surface types → registered hashes.
- `pretty.re` — name-aware type rendering (`Counter.t`, `Counter.t -> Int`); never shows a witness.
- `eval.re` — CBV evaluator; abstraction erased at runtime (a `Seal` is transparent, abstract values reduce to their representation).
- `bin/repl.re` — `:abstract` (create + open), `:open` (re-open to extend), `:let` (auto-seal via minimal sealing), `:close`, `:ctx`, `:impl`, `:show`, `:ls`, bare expr → type + value.

Web interface (`web/`, Bonsai + js_of_ocaml; entry `webmain/main.ml`): three panes —
namespace browser (left), editor + editing-context indicator (center), detail (right).
The editing context is **browser-driven**: each abstract type carries an "open for edit"
/ "close" toggle in the namespace tree, and the editor shows the current open set. Binding
a term shows a live **Normal / SEALED** badge (minimal sealing made visible); the detail
pane shows an abstract type's derived implementation set. Bootstraps the Counter example
on load. Substrate is the same `.re` library, untouched.

Run the REPL:

```sh
eval $(opam env --switch=. --set-switch)
dune exec ./bin/repl.exe      # :help for the worked Counter example
```

Run the web app:

```sh
eval $(opam env --switch=. --set-switch)
scripts/build-web.sh          # builds public/p16.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

Decided against (see `docs/prototypes/p16-records/decisions.md`):

- edit-of / mint carry-forward across a representation change. Dies-with-hash instead: soundness rides witness-in-hash, distinctness wants a fresh mark, within-checkout lineage is the namespace; mint-persistence deferred to the collaboration phase. The soundness boundary it implies (old-rep value rejected by new-rep op) is tested.

Remaining (optional):

- longest-suffix name resolution (deferred from p9).

## Build

```sh
eval $(opam env --switch=. --set-switch)   # shared switch (symlinked to p7's)
dune build && dune runtest
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Tech stack: OCaml ≥ 5.2,
Reason, dune ≥ 3.17, Menhir 3.0, ppx_deriving, digestif (BLAKE2B), alcotest /
qcheck for the substrate; Bonsai / Virtual_dom / Core / js_of_ocaml (v0.17) for
the web layer only. The `src/` substrate library is wrapped (`P16_substrate`);
the REPL and tests `open P16_substrate`.
