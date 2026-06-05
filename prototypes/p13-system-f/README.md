# p13 — System-F over abstract types (functorization)

Forks **p12** (abstract types via editor-enforced opacity) and adds **System-F
polymorphism**: type variables, type abstraction (`/\t. e`), type application
(`e [T]`), and `∀` types. The goal is *functorization* — write a program generic
over the carrier type and apply it to a counter with **any** internal
representation, e.g.

```
step : forall t. ((t -> t) * (t -> t)) -> t -> Bool -> t
step = /\t. \ops: (t -> t) * (t -> t). \x: t. \b: Bool. if b then (fst ops) x else (snd ops) x

step [Counter.t] (Counter.incr, Counter.decr) Counter.empty true   -- : Counter.t
```

Existentials / first-class modules (the `∃` / `{ type t; … }`-as-a-value form)
are deliberately out of scope here; see `docs/design/12-type-abstraction.md`.

Design and scope: `docs/prototypes/p13-system-f/` (`00-scope.md`,
`decisions.md`, `open-questions.md`).

## Status

Forked from p12 (verbatim, renamed) and building green; System-F additions in
progress. Everything p12 had — content-addressed substrate, opaque types +
seals, surface language, CBV evaluator, REPL, Bonsai web interface, tests
aspect — carries over.

Substrate core (the `src/` library, `P13_substrate`):

- `src/hash.re` — BLAKE2B content hashes (carried from p11).
- `src/mint.re` — deterministic minted marks (counter-sourced, reproducible).
- `src/tnode.re` — `Int | Bool | Product | Arrow | Opaque{mint, witness}`; content encoding (sort byte `T`).
- `src/node.re` — term nodes incl. `Ref(Hash)` and `Seal{opens, ty, impl}`; de Bruijn; encoding (sort byte `P`).
- `src/definition.re` — `Term(Node.t) | Type(Tnode.t)`; leading-byte disambiguation.
- `src/typecheck.re` — opacity-parameterized checker (`whnf`/`equal_ty` unfold open opaques); `Seal` ingest rule; witness-`normalize`.
- `src/store.re` — content-addressed ingest; `Type_of` cache; derived `impl_set` scan; no ingest-level opacity.
- `src/editing_context.re` — open set; `open_type`; minimal-sealing `commit` (ordinary term vs. seal).

`test/test_p13.re` proves: Counter≠Celsius (mint distinctness), witness-in-hash (rep change moves the type), minimal sealing (empty/incr/get sealed, `bump2` ordinary), correct external types, raw-body sharing (incr's impl == `Math.inc`), opacity (a default-context consumer cannot touch the representation), bogus-seal rejection at ingest, derived implementation set, and criterion-4 (editing `decr` leaves `bump2` byte-identical).

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
scripts/build-web.sh          # builds public/p13.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

Decided against (see `docs/prototypes/p13-system-f/decisions.md`):

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
the web layer only. The `src/` substrate library is wrapped (`P13_substrate`);
the REPL and tests `open P13_substrate`.
