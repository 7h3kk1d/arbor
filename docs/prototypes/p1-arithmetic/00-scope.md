# Phase 1 — Untyped Arithmetic

**Status:** Scope doc for the first prototype.
**Design context:** `../../design/09-roadmap.md` (Phase 1).
**Prototype design lives here:** `docs/prototypes/p1-arithmetic/`.
**Implementation code will live at:** `prototypes/p1-arithmetic/` (not yet scaffolded).

## Thesis

Validate the substrate's core model end-to-end with the simplest plausible language. Learn what chafes before adding more moving parts.

The three questions this prototype should answer:

1. Does our AST representation work in code?
2. Does canonicalization + hashing produce stable, deterministic identifiers?
3. Does register / lookup / evaluate compose as a minimum workflow?

## Language

Untyped arithmetic expressions from TAPL Chapter 3:

```
t ::= true | false
    | if t then t else t
    | 0 | succ t | pred t | iszero t
```

Values are booleans (`true`, `false`) and natural numbers (`0`, `succ 0`, `succ (succ 0)`, …). No binders, no variables, no functions. Canonicalization is trivial — the AST as written is already canonical (no α-equivalence, no reordering of anything).

## Tech stack

Following Hazel's stack where practical:

- **OCaml** ≥ 5.2
- **Reason** ≥ 3.12 (syntax)
- **dune** ≥ 3.16 (build)
- **Menhir** (parser generator)
- **ppx_deriving** (derived eq/show/compare)
- **digestif** (cryptographic hashing; BLAKE3)
- **alcotest** + **qcheck** + **qcheck-alcotest** (tests)

Hazel deps we skip: the browser stack (`incr_dom`, `bonsai`, `ezjs_idb`), Grove/UID work (`uuidm`), type-inference machinery (`unionFind`, `bignum`), formatting utilities (`csv`, `omd`). None of them are relevant to Phase 1.

## Interface — interactive REPL, in-memory only

The REPL holds a single in-memory Store (a hashtable from hash to definition). State does not persist across invocations; exiting the REPL loses everything.

Proposed command syntax (subject to refinement during implementation):

```
> succ (succ 0)
h:a3f42c1b8…
⇒ succ (succ 0)

> succ (pred (succ 0))
h:b1c29de77…
⇒ succ 0

> :list
h:a3f42c1b8… — succ (succ 0)
h:b1c29de77… — succ (pred (succ 0))

> :lookup a3f42c
succ (succ 0)

> :eval a3f42c
⇒ succ (succ 0)

> :quit
```

- Bare expressions are parsed, registered in the Store, and evaluated. Both the hash and the evaluated value are printed.
- Lines starting with `:` are commands.
  - `:list` — enumerate stored definitions with their AST pretty-printed.
  - `:lookup <hash-prefix>` — print the stored AST for the given hash (prefix matching for convenience).
  - `:eval <hash-prefix>` — re-run evaluation on a stored definition.
  - `:register-only <expr>` — parse and register without evaluating; prints hash.
  - `:eval-expr <expr>` — evaluate without storing; prints value only.
  - `:help` — list commands.
  - `:quit` / `:exit` — leave the REPL.

## Mapping to the substrate layers

From `../../design/06-architecture.md`, the substrate has four layers. Phase 1 exercises three:

- **Store.** In-memory `Hashtbl.t` from `Hash.t` to `Definition.t`. For Phase 1, `Definition.t` is a type alias for `Arith.AST.t` — the full sum-over-languages model from the substrate design collapses to a single type at this scale. When a second language arrives, the alias grows into a sum; callers barely notice.
- **Attachment.** **Not exercised.** Phase 1 has no aspects and no naming.
- **Language.** Modules for the arithmetic AST, parser, canonicalizer, pretty-printer, evaluator.
- **Interface.** The REPL.

The "no cross-language references" invariant is not interesting here (only one language exists). When we reintroduce the invariant (Phase 2's cross-definition references, Phase 4's multiple languages), `Definition.t` grows and Store's registration grows a dispatch.

## In scope

- AST types for the arithmetic language (in Reason).
- Canonicalization (identity for this language — functions through to hashing).
- Hashing via digestif BLAKE3; displayed as `h:<hex>` with a short prefix for convenience.
- An in-memory Store: `Hashtbl.t` from `Hash.t` to `Definition.t`.
- A Menhir parser for the surface syntax.
- A pretty-printer (prefers readable output over strict grammar roundtripping).
- A big-step evaluator (one recursive function from expression to value).
- The REPL.
- Tests: parser roundtrip, canonicalization stability, hash determinism, evaluator correctness per constructor.

## Out of scope

Per `../../design/09-roadmap.md`'s Phase 1 scope:

- No naming layer. Definitions are referred to by hash, full stop.
- No aspects. No evaluation cache, no type-check (untyped), no translation.
- No multiple languages.
- No cross-definition `Ref(hash)` references inside expressions. A program is a single self-contained term.
- No persistence. REPL state dies on exit.
- No Hazel integration, no hole-awareness, no collaboration.

## What "done" looks like

- You start the REPL, enter `iszero (pred (succ 0))`, see it register with a hash and evaluate to `true`.
- `:list` enumerates what's stored; `:lookup <prefix>` and `:eval <prefix>` work on stored hashes.
- Tests pass: parser roundtrips, hash determinism holds across runs (same expression → same hash), the evaluator produces expected values for every constructor.
- You have honest opinions about what was awkward, and the next most interesting question is clear.

## Proposed directory layout (for when we scaffold the code)

```
prototypes/p1-arithmetic/
  dune-project
  p1-arithmetic.opam
  src/
    dune               # one library
    ast.re             # arithmetic AST
    parser.mly         # Menhir grammar
    lexer.mll          # ocamllex lexer
    canonicalize.re    # identity for this language; the slot exists
    pretty.re          # AST → string
    hash.re            # BLAKE3 over a hand-rolled encoding of Definition.t
    store.re           # Hashtbl.t from Hash.t to Definition.t
    eval.re            # big-step evaluator
    definition.re      # `type t = Ast.t` — the type alias
  bin/
    dune               # the REPL executable
    main.re            # REPL entry point; reads stdin, drives src/
  test/
    dune
    test_p1.re         # alcotest + qcheck suite
```

Single library; one executable; one test suite. `Definition.t` is a type alias for `Ast.t`. The Store / Language boundary exists at the module level (`Store` and `Ast`/`Eval` are distinct modules) but we don't pre-factor a multi-language sum. When a second language arrives in a later prototype, the alias becomes a sum and a second library appears — both local refactors.
