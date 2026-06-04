# p12 — Abstract types via editor-enforced opacity

First prototype in the type-abstraction (`docs/design/12-type-abstraction.md`)
region. Takes the **unbundled** path: abstract types in isolation — no module
record, no functors, no records-as-signatures. Opacity lives in the **editing
layer** (an editing context that "opens" abstract types), not at ingest.

Design and scope: `docs/prototypes/p12-abstract-types/` (`00-scope.md`,
`decisions.md`, `open-questions.md`).

## Status

Scaffolding. Substrate + editing-context core is built and tested first;
interface (REPL-leaning) decided before any interface code lands.

Built so far (substrate core — green, 4 tests incl. the full Counter worked example):

- `src/hash.re` — BLAKE2B content hashes (carried from p11).
- `src/mint.re` — deterministic minted marks (counter-sourced, reproducible).
- `src/tnode.re` — `Int | Bool | Product | Arrow | Opaque{mint, witness}`; content encoding (sort byte `T`).
- `src/node.re` — term nodes incl. `Ref(Hash)` and `Seal{opens, ty, impl}`; de Bruijn; encoding (sort byte `P`).
- `src/definition.re` — `Term(Node.t) | Type(Tnode.t)`; leading-byte disambiguation.
- `src/typecheck.re` — opacity-parameterized checker (`whnf`/`equal_ty` unfold open opaques); `Seal` ingest rule; witness-`normalize`.
- `src/store.re` — content-addressed ingest; `Type_of` cache; derived `impl_set` scan; no ingest-level opacity.
- `src/editing_context.re` — open set; `open_type`; minimal-sealing `commit` (ordinary term vs. seal).

`test/test_p12.re` proves: Counter≠Celsius (mint distinctness), witness-in-hash (rep change moves the type), minimal sealing (empty/incr/get sealed, `bump2` ordinary), correct external types, raw-body sharing (incr's impl == `Math.inc`), opacity (a default-context consumer cannot touch the representation), bogus-seal rejection at ingest, derived implementation set, and criterion-4 (editing `decr` leaves `bump2` byte-identical).

Remaining:

- `surface.re` / `lexer.mll` / `parser.mly` — trimmed p9 language; no `open`/`seal` keyword.
- `resolver.re` — names → hashes at edit time; surface → internal (de Bruijn).
- `namespace.re` — separate `name → hash` table.
- `pretty.re` — name-aware rendering (`Counter.t`, `Counter.incr`); never show witness outside an opening context.
- edit-of (mint carry-forward across a witness change — the pair-counter lineage).
- interface (REPL) — pending confirmation.

## Build

```sh
eval $(opam env --switch=. --set-switch)   # shared switch (symlinked to p7's)
dune build && dune runtest
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Tech stack: OCaml ≥ 5.2,
Reason, dune ≥ 3.17, Menhir 3.0, ppx_deriving, digestif (BLAKE2B), alcotest /
qcheck for tests. No Bonsai / js_of_ocaml / Core (deliberately shed).
