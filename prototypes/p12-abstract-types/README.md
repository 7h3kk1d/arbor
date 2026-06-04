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

Built so far:

- `src/hash.re` — BLAKE2B content hashes (carried from p11).
- `src/mint.re` — deterministic minted marks (counter-sourced, reproducible).

Planned module order (substrate first, interface last):

1. `tnode.re` — `Int | Bool | Product | Arrow | Opaque{mint, witness}`; content encoding.
2. `node.re` — term nodes incl. `Ref(Hash)` and `Seal{opens, ty, impl}`; de Bruijn; content encoding.
3. `definition.re` — `Term(Node.t) | Type(Tnode.t)`; leading-byte disambiguation.
4. `store.re` — content-addressed ingest/reconstruct; `Type_of` aspect; implementation-set scan.
5. `surface.re` / `lexer.mll` / `parser.mly` — trimmed p9 language; no `open`/`seal` keyword.
6. `resolver.re` — names → hashes at edit time; surface → internal (de Bruijn).
7. `typecheck.re` — opacity-parameterized bidirectional checker; minimal-sealing classification.
8. `namespace.re` — separate `name → hash` table.
9. `editing_context.re` — open set; create-type / open-existing gestures; implicit-seal commit.
10. interface (REPL) — pending confirmation.

## Build

```sh
eval $(opam env --switch=. --set-switch)   # shared switch (symlinked to p7's)
dune build && dune runtest
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Tech stack: OCaml ≥ 5.2,
Reason, dune ≥ 3.17, Menhir 3.0, ppx_deriving, digestif (BLAKE2B), alcotest /
qcheck for tests. No Bonsai / js_of_ocaml / Core (deliberately shed).
