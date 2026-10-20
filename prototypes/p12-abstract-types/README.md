# p12 — Abstract types via editor-enforced opacity

The first prototype of the type-abstraction line (`docs/design/12-type-abstraction.md`). It builds abstract types on their own, with no modules, functors or signatures. Opacity is enforced by the editing layer: an editing context opens abstract types, and the store does no opacity check at ingest.

Design and scope: `docs/prototypes/p12-abstract-types/` (`00-scope.md`, `decisions.md`, `open-questions.md`).

## Status

Complete: content-addressed substrate, surface language, CBV evaluator, REPL, and a Bonsai + js_of_ocaml web interface. `dune runtest` passes 8 tests.

The tests cover mint distinctness (`Counter` ≠ `Celsius` over the same `Int`), witness-in-hash (changing the representation changes the type), minimal sealing (`empty`/`incr`/`get` become Seals, `bump2` stays ordinary), external types, sharing of raw bodies (`incr`'s implementation is `Math.inc`), opacity (a consumer in the default context cannot touch the representation), rejection of a bogus Seal at ingest, the derived implementation set, and edit isolation (editing `decr` leaves `bump2` byte-identical).

## Substrate (`src/`, library `P12_substrate`)

- `hash.re` — BLAKE2B content hashes (from p11).
- `mint.re` — deterministic minted marks, counter-sourced.
- `tnode.re` — `Int | Bool | Product | Arrow | Opaque{mint, witness}`; sort byte `T`.
- `node.re` — term nodes, including `Ref(Hash)` and `Seal{opens, ty, impl}`; de Bruijn; sort byte `P`.
- `definition.re` — `Term(Node.t) | Type(Tnode.t)`, told apart by leading byte.
- `typecheck.re` — the opacity-parameterized checker (`whnf`/`equal_ty` unfold an opaque only if it is open), the `Seal` ingest rule, witness normalization.
- `store.re` — ingest, the `Type_of` cache, and the derived implementation-set scan.
- `editing_context.re` — the open set, `open_type`, and the minimal-sealing `commit`.

## Surface and interface

- `surface.re`, `surface_ty.re` — surface ASTs with names and no holes.
- `lexer.mll`, `parser.mly`, `parse.re` — a trimmed p9 language; monolithic, fail-fast Menhir. There is no `open` or `seal` keyword.
- `namespace.re` — `name → hash` with exact matching and a reverse index for display.
- `resolver.re` — resolves names to hashes at edit time and converts to de Bruijn.
- `pretty.re` — name-aware rendering (`Counter.t -> Int`). It never shows a witness.
- `eval.re` — CBV evaluation. Abstraction is erased at runtime; a `Seal` is transparent.
- `bin/repl.re` — `:abstract` (create and open), `:open` (re-open to extend), `:let` (commit with minimal sealing), `:close`, `:ctx`, `:impl`, `:show`, `:ls`. A bare expression prints its type and value.

The web app (`web/`, entry `webmain/main.ml`) has three panes: namespace browser, editor with the current open set, and detail. Each abstract type has an open/close toggle in the namespace tree. Binding a term shows a live Normal or SEALED badge. The detail pane shows an abstract type's derived implementation set. The Counter example is seeded on load.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
dune exec ./bin/repl.exe      # :help walks through the Counter example
scripts/build-web.sh          # builds public/p12.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Stack: OCaml ≥ 5.2, Reason, dune ≥ 3.17, Menhir 3.0, ppx_deriving, digestif (BLAKE2B), alcotest, qcheck; Bonsai, Virtual_dom, Core and js_of_ocaml (v0.17) for the web layer only.

## Decided against

Carrying a mint forward across a representation change (see `decisions.md`). A representation change mints a fresh type instead. Soundness rests on the witness being in the hash, distinctness needs a fresh mark, and lineage within a checkout is the namespace's job. The resulting boundary, where an old-representation value is rejected by a new-representation operation, is tested.

Deferred: longest-suffix name resolution (from p9).
