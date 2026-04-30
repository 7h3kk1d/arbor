# Phase 9 — Typed namespaces (typed language + hierarchical names + holes + browser UI)

**Status:** Scope doc for the ninth prototype.
**Design context:** `../../design/03-content-addressing.md` (canonicalization, no cross-language references), `../../design/04-naming-layer.md` (single namespace; opaque strings; *Threads under exploration → Hierarchical paths*), `../../design/06-architecture.md` (four-layer decomposition), `../../design/07-hazel-substrate.md` (typed-holes lines).
**Prototype design lives here:** `docs/prototypes/p9-typed-namespaces/`.
**Implementation code lives at:** `prototypes/p9-typed-namespaces/`.

## Thesis

p9 is the first prototype to fuse the substrate's three biggest unresolved UX threads:

1. **A single typed surface language** so the substrate isn't always demonstrating two-language coexistence (p5/p6) or one untyped baseline (p1/p2/p3/p4/p8). p7 inherited p6's lc+stlc pair; p9 instead picks one richer typed language and lets the substrate be evaluated against that.
2. **Hierarchical dot-delimited names with Unison-style longest-suffix resolution.** This is the *Hierarchical paths* thread from `04-naming-layer.md` taken to a working prototype: substrate stores opaque dotted strings, edit-time resolution offers full-path matching plus longest unambiguous segment-suffix matching, with a distinct `Ambiguous` error so the UI can list candidates.
3. **Holes integrated with the type system.** p8 answered "how do holes hash?" for an untyped language. p9 answers the next-up question: how does a typechecker behave when it encounters holes? The answer here is a permissive bidirectional checker that produces three outcomes — `Type_of(ty)` for hole-free terms, `Type_with_holes(ty)` (best-guess) for terms whose holes don't break type discipline, and `Ill_typed` for hard mismatches that reject ingest.

A new derived aspect, `has-holes:v1`, caches whether a term (or any subterm reachable through the stored DAG) contains a hole. The `Has_holes(bool)` aspect-value variant joins `Type_with_holes(Ty.t)` in extending p6's aspect_value sum.

The interface is a Bonsai + js_of_ocaml browser UI, three panes:

- **Namespace tree (left)** — collapsible by dot-prefix; has-holes badge on leaves; click to open a hash in the detail pane.
- **Editor + recovered AST + feedback (center)** — a textarea over the substrate (every keystroke ingests synchronously, since ingest is idempotent under content addressing), a panel that re-renders the post-recovery surface AST with hole markers as you type, and a feedback area showing the ingested hash, type, has-holes flag, and resolved chips.
- **Detail (right)** — selected hash's names, body (name-aware pretty-printed), and aspect badges.

The lineage: p8 (parser recovery, Hole hashing) + p6 (typecheck-at-ingest, Type_of aspect) + p7 (Bonsai web UI). Arith + lc + stlc are dropped; the language is a fresh, single sum with native Int/Bool/String/Product, lambdas, let, if/then/else, pairs, and primitive operations.

### Questions this prototype should answer

1. Does longest-segment-suffix resolution feel natural in practice, or do users hit unexpected ambiguities? (The bootstrap deliberately seeds `math.add` and `vector.add` to surface ambiguity on bare `add`.)
2. Does the permissive (non-unifying) checker accept enough holey programs to be useful, or does it reject too eagerly when a hole's type can't be determined locally? If the latter, the planned escalation is ref-cell unification (~50 lines, no occurs check needed since types are first-order).
3. Is the recovered-AST panel — re-rendering on every keystroke as the parser inserts holes — a clear UX, or does it just repeat what the textarea already shows? (Hypothesis: it's clearest when the recovery actually inserts holes, since the user sees where they landed.)
4. Does the has-holes-transitively aspect propagate the way users expect (i.e., a definition that depends on a holey definition shows the badge)? With inline-at-resolution, "depends on" means the inlined subtree, which is what DAG traversal sees.
5. What does the UX look like when a user binds `math.add` and then types `add 1 2` — does the suffix resolution feel like Unison's, or does it surprise?

## Language

```
e ::= x                        -- variable; may be dotted (math.add)
    | n | true | false | "s"   -- literals: Int, Bool, String
    | \x: T. e                 -- annotated lambda
    | e e                      -- application
    | let x = e in e           -- let binding (NOT polymorphic)
    | if e then e else e       -- conditional
    | (e, e)                   -- pair
    | fst e | snd e            -- pair projection
    | e + e | e - e            -- Int * Int -> Int
    | e mul e | e / e | e mod e
    | e && e | e || e | not e  -- Bool * Bool -> Bool, Bool -> Bool
    | e ++ e                   -- String * String -> String
    | e == e                   -- {Int|Bool|String}^2 -> Bool
    | (e)                      -- grouping
    | ?                        -- hole (explicit or recovery-inserted)

T ::= Int | Bool | String      -- base types
    | T -> T                   -- arrow (right-assoc)
    | T * T                    -- product (binds tighter than arrow)
    | (T)                      -- grouping
```

Notes:

- **`mul` keyword instead of `*`** in expressions — `*` is reserved for the product type syntax.
- **Lambdas require an annotation.** Bidirectional check; monomorphic. `let` inherits its variable's *synthesized* type — no generalization.
- **Equality is loosely polymorphic.** `==` accepts any of Int/Bool/String pair, returns Bool; the checker synthesizes the LHS's type, then checks the RHS at the same type. (Holes on either side flow through the permissive path.)
- **Native types in hashes.** `Ty.encode` uses tag bytes 0x10..0x14 for Int, Bool, String, Arrow, Product. The Lam node embeds its annotation, so `\x: Int. ?` and `\x: Bool. ?` produce different hashes.
- **Hole tag** is 0x0d in this prototype (vs. p8's 0x04, both prefixed by the language tag 'P' so the hashes aren't comparable across prototypes anyway).

## Architecture mapping

Four layers, structurally parallel to p7 with the deltas called out:

- **Store.** Single-language. `Node.t` carries 13 constructors (Var, Int_lit, Bool_lit, String_lit, Lam, App, Let, If, Pair, Fst, Snd, Prim, Hole). Language tag byte `'P'`. Prim encodes as `Prim(op, list(Hash.t))` with one-byte op-tags so future ops can be added without invalidating existing hashes.
- **Attachment.** Aspect-value sum extends p6's with `Type_with_holes(Ty.t)` and `Has_holes(bool)`. Three derived descriptors register at startup: `typecheck`, `has-holes`, `eval`.
- **Language.**
  - `Surface_ast.t` and `Ast.t` mirror Node.t (surface keeps names, internal de Bruijn — for *both* Lam and Let bindings, extending p4's α-equivalence-by-canonicalization to Let).
  - `Lexer` is total (unknown bytes → HOLE; '?' → HOLE).
  - `Parser` is the rich grammar with HOLE token + grammar-level partial-form productions for truncated `let`, `if`, and `\binder: ty.` constructs.
  - `Parse_recover` carries p8's incremental-API rewind-and-inject-HOLE driver verbatim (only renames Parser/Lexer module references).
  - `Namespace` keeps the flat `Hashtbl` from p3-p6 with dotted-string keys; adds `resolve_query` for full-path-then-segment-suffix lookup with `Unbound | Ambiguous(list(string))` error, and reserved-keyword discipline (every keyword from the surface grammar is a reserved segment).
  - `Resolver` resolves Var(name) via context-then-namespace, lifts Surface_ast → Ast (de Bruijn for Lam *and* Let), and orchestrates the ingest pipeline: parse → resolve → typecheck → on Ill_typed reject; otherwise ingest, register typecheck aspect (Type_of or Type_with_holes), register has-holes aspect.
  - `Typecheck` is the permissive bidirectional checker. Hole accepts any expected type in checking; in synthesis it returns Int as a best-guess and propagates `has_holes=true`. Hole-derived ambiguity (e.g. App with a holey function position) is swallowed with a permissive best-guess type rather than rejected.
  - `Has_holes` is a pure DAG traversal cached as a derived aspect; sub-DAGs have stable has-holes status because of content addressing, so caching at every visited node is correct.
  - `Eval` is CBV; all primitives reduce concretely on values; pairs are CBV-evaluated; Hole anywhere on the head path produces Stuck.
  - `Pretty` provides `print_surface` (operator-precedence-aware Surface_ast.t → string with hole markers) and `print_named` (Hash.t → name-aware reconstruction, picks fresh names avoiding reserved keywords, collapses non-top closed children whose hash has a namespace name).
- **Interface.** Bonsai + js_of_ocaml in-browser app:
  - `Substrate.global` — the singleton Store/Att/Namespace bootstrapped with seed bindings (`math.add/sub/mul/inc`, `vector.add` for ambiguity demo, `string.greet`, `logic.implies`, `draft.todo` for the has-holes badge).
  - `State` — Bonsai model + action sum. `expanded_paths : list(string)` for the tree's open prefixes; `feedback : ingest_result` carries the recovered Surface_ast.t plus the ingest outcome (Ingested/Resolve_unbound/Resolve_ambiguous/Type_error).
  - `Feedback.compute` — pure substrate pipeline driven by `on_input` per keystroke. Ingest is idempotent.
  - `Recovered_view` — re-renders the post-recovery surface AST per keystroke with `<span class="hole-marker">` around holes. Always present after the first non-empty input (parse is total).
  - `Namespace_tree` — builds a recursive trie per render from the flat namespace, renders collapsible groups; leaves show the leaf segment, short hash, and ◌ badge if `has-holes:v1` cached `true`.
  - `Editor` — textarea + bind input + auto-evaluate toggle.
  - `Detail` — selected hash's names, body (name-aware), and aspect badges (typecheck, has-holes, eval).
  - `App` — three-pane CSS grid wrapping the above.

## In scope

- Single language with the grammar, type system, and primitives spelled out above.
- Hierarchical dot-delimited names; full-path + longest-segment-suffix resolution; `Ambiguous` distinct from `Unbound`.
- `has-holes:v1` derived aspect; `Has_holes(bool)` aspect value; `Type_with_holes(Ty.t)` aspect value; permissive bidirectional checker.
- Recovery via menhir incremental API + grammar-level partial-form productions; total lexer.
- Bonsai + js_of_ocaml UI with a recovered-AST panel and a hierarchical namespace browser.
- Substrate test suite: 33 tests covering round-trip, hole hash stability, alpha-equivalence (lambdas *and* lets), suffix resolution (resolves, ambiguous, segment-bounded, full-path), reserved-keyword bind rejection, dotted bind/rebind/unbind, has-holes propagation + caching, permissive typecheck, strict mismatch rejection, pair eval, primitive eval, let eval, recovery (truncated let/if/lambda + garbage), and parser totality (1000 + 500 QCheck trials).

## Out of scope

- HM let-polymorphism. Monomorphic `let`; if a real user need surfaces, escalate.
- Real unification in the checker. The non-unifying permissive version ships first; ref-cell unification is the planned escalation.
- `Ref(hash)` AST constructor — still inline-at-resolution, like every prior prototype. "Has-holes transitively" leans on the inlined DAG.
- Multi-language. p9 is single-language; the language tag byte is kept ('P') for hash-space hygiene against other prototypes that might share encoding bytes by accident.
- Translation between languages. Not relevant.
- Namespace branching, history, multiple namespaces. Carried over as deferred from `04-naming-layer.md`.
- Collaborative editing / Grove.
- Persistent storage. The Bonsai bundle is stateless across page reloads; bootstrap re-seeds.
- Caller-aware "update all callers" rebind. The substrate-level rebind dialog confirms one binding at a time.

## What "done" looks like

- `dune build && dune runtest` green. Substrate test suite: 33 tests (eval, typecheck, namespace, has-holes, recovery, hole hash stability, round-trip, parser totality).
- `scripts/build-web.sh` produces `public/p9.js` (~26 MB). Opening `public/index.html` in a browser shows the three-pane app.
- Manual UX checks (from the plan):
  - `\x: Int. x + 1` — recovered panel clean; type `Int -> Int`; has-holes false.
  - `\x: Int. ?` — recovered shows `\x: Int. ?` with the hole marker; type `Type_with_holes(Int -> Int)`; has-holes true.
  - `1 + true` — Type_error in feedback; nothing ingests.
  - With seeded `math.add` and `vector.add`, typing `add 1 2` shows `ambiguous name 'add' — could be: math.add, vector.add`.
  - `math.add 1 2` resolves; eval to 3.
  - With only `math.add` bound, bare `add 1 2` resolves via suffix; eval to 3.
  - `(\x: Int. x) "abc"` — Type_error.
  - `let p = (1, true) in fst p` — eval to 1.
  - Garbage `}}}` — recovered panel shows `?`; ingest path stores the bare hole.
  - Expanding `math.` in the tree, clicking `add` opens its detail with type and eval rows.

## Directory layout

```
prototypes/p9-typed-namespaces/
  dune-project
  p9_typed_namespaces.opam
  _opam/                            # symlink to p7-web-interface/_opam/_opam
  src/
    dune                            # menhir --table, menhirLib + digestif
    hash.re
    ty.re                           # Int|Bool|String|Arrow|Product, tag bytes 0x10..0x14
    surface_ast.re                  # carries names; prim_op enum
    ast.re                          # de Bruijn for Lam + Let; shift/subst/beta
    node.re                         # shallow DAG; tag 'P' + 13 constructor tags
    definition.re                   # alias = Node.t
    canonicalize.re                 # identity hook
    lexer.mll                       # total; '?' / unknown → HOLE; dotted IDENT
    parser.mly                      # rich grammar with HOLE atom + partial productions
    parse_recover.re                # incremental-API recovery (clone of p8's)
    pretty.re                       # surface printer + name-aware printer
    namespace.re                    # flat dotted map + suffix resolution + reserved keywords
    resolver.re                     # surface→internal; ingest pipeline (resolve→typecheck→ingest→aspects)
    typecheck.re                    # permissive bidirectional checker; three outcomes
    has_holes.re                    # DAG-traversal aspect
    eval.re                         # CBV; primitives; Stuck on Hole
    attachment.re                   # extends p6's aspect_value with Type_with_holes + Has_holes
    store.re                        # single-language ingest/reconstruct
  web/
    dune                            # bonsai + virtual_dom + core + js_of_ocaml
    substrate.ml                    # singleton Store/Att/Namespace
    bootstrap.ml                    # seeds math.*, vector.add, string.greet, draft.todo
    state.ml                        # model + actions
    feedback.ml                     # keystroke→ingest pipeline
    recovered_view.ml               # post-recovery surface AST panel
    namespace_tree.ml               # collapsible dotted-prefix tree
    editor.ml                       # textarea + feedback + bind
    detail.ml                       # selected hash detail
    aspects_view.ml                 # typecheck / has-holes / eval rows
    app.ml                          # three-pane layout + state machine
  bin/
    dune                            # js_of_ocaml mode
    p9_main.ml                      # Bonsai_web.Start.start App.component
  test/
    dune
    test_p9.re                      # 33 tests
  scripts/
    build-web.sh                    # dune build + cp to public/p9.js
  public/
    index.html
    styles.css                      # 4-pane CSS grid
    p9.js                           # built artifact
```

## Tech stack

OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.17, Menhir 3.0 (`--table` mode), menhirLib, ppx_deriving, digestif (BLAKE2B), js_of_ocaml ≥ 5.6, Bonsai v0.16/v0.17, Virtual_dom v0.16/v0.17, Core v0.16, alcotest, qcheck, qcheck-alcotest. `_opam` symlinks to `p7-web-interface/_opam/_opam` (the inner switch under p7's nested wrapper).
