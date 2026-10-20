# p21 — Unison's model, on a small typed language

An executable counterpart to `formalism/paper/mini-unison.tex`, which models Unison's content addressing and update scheme. All 49 statements in the paper are mirrored in the sources.

p21 is built from scratch. It is not a fork of p17 and carries nothing from the p12–p17 type-abstraction line. The object language is the paper's: STLC with `Nat`, `Text` and arrows. What it implements:

- **Component hashing.** A strongly connected component of mutually recursive definitions is hashed as a unit, in three steps with two orderings. One ordering fixes the de Bruijn indices used while hashing member bodies; the other fixes the stored positions that references use. If two members are structurally equivalent apart from their references to each other, ingest aborts, because otherwise the stored identity would depend on their names.
- **References are pairs**, `Derived(hash, position)`. The store maps a hash to a component. A reference is transparent only at position 0.
- **The type is inside the term hash.** Changing a signature changes identity.
- **The namespace is a relation.** One name may denote several referents. Conflict is a legal state; operations that need no conflict check for it.
- **The watch cache has no invalidation.** It is keyed by reference and holds result terms inline.
- **Update by render and reparse.** Dependents are printed to source, reparsed, re-typechecked and re-ingested. They retarget because a printed name resolves against the new namespace. On failure, every name the file bound and every dependent it rendered is removed from the namespace, and the source returns to the buffer.
- **Merge.** Three namespaces over one shared store, with a syntactic hash (`synhash`) as the provenance test.

```
scale : Nat -> Nat
scale = \n. Nat.* n 2

double : Nat -> Nat
double = \n. scale n

> Nat.+ 2 3

---
everything below the fold is inert
```

Hashing is SHA3-512 over Unison's tagged, length-prefixed token stream. It is the first prototype in the series to use the modelled system's hash function; `digestif.ocaml`'s pure-OCaml SHA3-512 works under js_of_ocaml. Hashes are not byte-compatible with real Unison, because the full surface encoding is not modelled.

Design and scope: `docs/prototypes/p21-unison-model/00-scope.md`.

## Status

Complete. 45 alcotest/qcheck cases, each naming the paper statement it checks.

1. A scratch buffer holds definitions with signatures, watch expressions, and a `---` fold below which everything is inert. It is reparsed and re-typechecked on every keystroke.
2. Ingest works on the whole group: resolve against the buffer-over-codebase overlay, typecheck, decompose into SCCs, component-hash in dependency order, store, bind. Nothing is stored until the whole buffer typechecks.
3. Update has both branches: success rebuilds dependents; failure evicts and hands the source back.
4. Evaluation runs with a step budget and is cached against the expression's reference.
5. Merge combines three namespaces with a per-name five-way diff, then renders, parses and typechecks against the merged namespace.

## Substrate (`src/`, library `P21_substrate`)

- `hash.re` — SHA3-512 over the token stream (`Tag`, `Nat`, `Text`, `Hashed`), version-prefixed.
- `reference.re` — `Builtin(text) | Derived(hash, pos)`, and `Referent`.
- `ty.re` — `Nat | Text | Arrow`, hashed on their own domain-separated layer.
- `term.re` — two grammars: `Source.t` without references, `Term.t` with them.
- `abt.re` — term hashing and `hash_component`: the three steps, both orderings, the side condition.
- `scc.re` — Tarjan, dependencies first.
- `store.re` — `Hash ⇀ Component`, the dependents index and its transitive closure.
- `names.re` — the namespace relation, `unconflicted`, `shadowing`.
- `watch.re` — `(Hash × Pos) ⇀ Term`, never invalidated.
- `typecheck.re`, `eval.re`, `builtins.re` — syntax-directed typing, CBV with a budget, primitives.
- `resolve.re` — partial: three of four cases leave a variable.
- `parse.re`, `lexer.mll`, `parser.mly`, `surface.re` — fail-fast parsing, one layout-split stanza at a time.
- `ppe.re`, `pretty.re` — rendering a stored definition back to source.
- `slurp.re` — the typed diff between buffer and namespace.
- `ingest.re`, `update.re` — group commit and the five update steps.
- `scratch.re` — the buffer's own state, kept out of `Store`.
- `synhash.re`, `merge.re` — the syntactic hash and three-way merge.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
scripts/build-web.sh   # then open public/index.html
```

Check that the paper and the prototype agree on which statements exist:

```sh
../../formalism/scripts/check-labels.sh \
  ../../formalism/paper/mini-unison.tex src
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Stack: OCaml 5.2, Reason, dune, Menhir (fail-fast, conflict-free), ppx_deriving, digestif.ocaml, js_of_ocaml, Bonsai, Virtual_dom, Core, ppx_jane, alcotest, qcheck.

## Known limits

No history or branching; no suffix resolution or `lib`-depth priority; no data declarations, `unique`-type minting, abilities or ANF. Names are atomic. Every binding needs a signature, since the language has no inference. State resets on reload.
