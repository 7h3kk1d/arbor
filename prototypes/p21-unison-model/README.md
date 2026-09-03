# p21 — Unison's model, on a small typed language

An executable counterpart to `formalism/paper/mini-unison.tex`. The paper states twenty-nine things
about Unison's content addressing and update scheme; this runs all of them.

Built from scratch — it is not a fork of p17, and it carries none of the p12–p17 type-abstraction
line. What it implements instead:

- **Component hashing.** A strongly connected component of mutually recursive definitions is hashed
  as a *unit*, in three steps and **two distinct orderings** — one fixing the de Bruijn indices used
  while hashing member bodies, one fixing the stored positions that appear in references. When two
  members are structurally equivalent modulo their references to each other, ingest aborts rather
  than store an identity that depends on their names.
- **References are pairs.** `Derived(hash, position)`. The store maps a hash to a *component*, not to
  a definition. Transparency holds exactly at position 0.
- **The type is inside the term hash.** A change of signature is a change of identity.
- **A namespace that is a relation.** One name may denote several referents; conflict is a legal
  state, and it is the operations that demand its absence.
- **A watch cache with no invalidation**, keyed by a reference, holding result terms inline.
- **Update by render-and-reparse.** Dependents are printed back to source, re-parsed, re-typechecked
  and re-ingested; retargeting happens because a printed *name* resolves against the new namespace.
  On failure, every name the file bound and every dependent it rendered leaves the namespace, and the
  source comes back into the buffer.

```
scale : Nat -> Nat
scale = \n. Nat.* n 2

double : Nat -> Nat
double = \n. scale n

> Nat.+ 2 3

---
everything below the fold is inert
```

Hashing is **SHA3-512** over Unison's own tagged, length-prefixed token stream — the first prototype
in the series whose hash function is the modelled system's, since `digestif.ocaml`'s pure-OCaml
SHA3-512 works under js_of_ocaml. Not byte-compatible with real Unison: the surface language's full
encoding is not modelled.

Design and scope: `docs/prototypes/p21-unison-model/00-scope.md`.

## Status

Implemented end to end:

1. A scratch buffer holding several definitions with their signatures, watch expressions, and a
   `---` fold below which everything is inert. Re-parsed and re-typechecked every keystroke.
2. Ingest as a group: resolve against the buffer-shadows-codebase overlay, typecheck the whole group,
   decompose into SCCs, component-hash in dependency order, store, bind. Nothing is stored until the
   whole buffer typechecks.
3. Update with both branches — a successful one that rebuilds dependents, and a failing one that
   evicts and hands the source back.
4. Evaluation with a step budget, cached against the expression's reference.

Substrate core (the `src/` library, `P21_substrate`):

- `hash.re` — SHA3-512 over the token stream; `Tag` / `Nat` / `Text` / `Hashed`, version-prefixed.
- `reference.re` — `Builtin(text) | Derived(hash, pos)`, and `Referent`.
- `ty.re` — `Nat | Text | Arrow`, hashed on their own domain-separated layer.
- `term.re` — the **two grammars**: `Source.t` without references, `Term.t` with them.
- `abt.re` — term hashing, and `hash_component`: the three steps, both orderings, the side condition.
- `scc.re` — Tarjan, dependencies first.
- `store.re` — `Hash ⇀ Component`, plus the dependents index and the transitive closure.
- `names.re` — the namespace relation, `unconflicted`, and `shadowing`.
- `watch.re` — `(Hash × Pos) ⇀ Term`, with no invalidation and none needed.
- `typecheck.re`, `eval.re`, `builtins.re` — syntax-directed typing, CBV with a budget, primitives.
- `resolve.re` — deliberately partial: three of four cases leave a variable.
- `parse.re`, `lexer.mll`, `parser.mly`, `surface.re` — fail-fast, layout-split stanzas.
- `ppe.re`, `pretty.re` — rendering a stored definition back to source.
- `slurp.re` — the typed diff between buffer and namespace.
- `ingest.re`, `update.re` — the group-at-a-time commit, and the five steps.

Run the tests:

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
```

Run the web app:

```sh
eval $(opam env --switch=. --set-switch)
scripts/build-web.sh   # then open public/index.html
```

Check that the paper and the prototype still agree on what exists:

```sh
../../formalism/scripts/check-labels.sh \
  ../../formalism/paper/mini-unison.tex src
```

### Known limits

No history, branching or merge; no suffix resolution or `lib`-depth priority; no data declarations,
`unique`-type minting, synhash, abilities or ANF. Names are atomic. Every binding needs a signature —
the language has no inference. State resets on reload.

## Build

Local opam switch, shared with p7: `_opam` is a symlink to `../p7-web-interface/_opam/_opam`. OCaml
5.2 / Reason, dune, Menhir (fail-fast, conflict-free), ppx_deriving, digestif.ocaml, js_of_ocaml,
Bonsai/Virtual_dom/Core, ppx_jane, alcotest, qcheck.
