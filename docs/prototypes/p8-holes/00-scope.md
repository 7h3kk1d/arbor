# Phase 8 — Untyped λ-calculus with holes and error-recovering parsing

**Status:** Scope doc for the eighth prototype.
**Design context:** `../../design/03-content-addressing.md` (canonicalization, non-goal of hashing partial programs), `../../design/06-architecture.md` (four-layer decomposition), `../../design/07-hazel-substrate.md` (typed-holes research lines, incompleteness as first-class AST constructs), `../../design/open-questions.md` §Content addressing ("Holes and incomplete programs" — which this prototype takes a first stab at).
**Prototype design lives here:** `docs/prototypes/p8-holes/`.
**Implementation code lives at:** `prototypes/p8-holes/`.

## Thesis

`docs/design/open-questions.md` asks under *Content addressing*:

> **Holes and incomplete programs.** If the substrate eventually hashes
> incomplete programs (Hazel-style editing), how do holes participate in
> the canonical form? Unique hole identities, subsumption-style matching,
> or something else?

`docs/design/03-content-addressing.md` points at the answer shape:

> We don't need generic "partial program" machinery in the substrate;
> each hole-aware language declares its own canonical form when it's
> introduced. Deferred until we add one.

p8 is the first prototype to *be* that hole-aware language, answering the
question for untyped λ-calculus:

1. **Recovery happens at the parser.** `Parse_recover.parse : string ->
   Surface_ast.t` is total. Every input, no matter how malformed, yields
   a Surface_ast.t. Errant subterms become `Hole` nodes; the lexer is also
   total (unknown bytes become `HOLE` tokens rather than raising).
2. **Holes are a new leaf in `Surface_ast.t` and `Ast.t`.** They participate
   in hashing via a new one-byte tag `'\x04'`. There is no hole payload —
   all holes are the same term. `\x. ?` and `\y. ?` produce identical
   content hashes, extending p4's α-equivalence-via-de-Bruijn story with
   "equivalence up to holes." Two users typing `?` in the same tree
   position collide on the same hash; this is a positive property (collaborative
   editing directly benefits) and the substrate-coherent choice.
3. **Menhir's incremental API drives recovery.** `src/dune` switches
   `menhir` to `--table` mode; `Parse_recover` drives
   `Parser.Incremental.main` via `MenhirInterpreter.offer` / `resume`. On
   `HandlingError`, recovery rewinds to the last `InputNeeded`, offers a
   synthetic `HOLE` in place of the offending token, and continues. If
   HOLE also doesn't fit at that position, the offending token is dropped
   and we try the next. An EOF-exhausted guard prevents unbounded recovery
   loops on genuinely unrecoverable inputs (e.g., unmatched trailing `(`
   where the parser has committed to expecting RPAREN).
3a. **Grammar-level fall-forward for truncated lambdas.** `parser.mly`
   also admits three *partial-lambda* productions (`BACKSLASH binder
   DOT`, `BACKSLASH binder`, `BACKSLASH`) that reduce to Hole-bearing
   `Lam`s. Because `FOLLOW(expr) = {EOF, RPAREN}`, these reductions
   only fire at end-of-expression lookaheads; mid-expression recovery
   still routes through the driver. The `binder` nonterminal accepts
   either `IDENT` or `HOLE` (synthesized name `"?"`, which the lexer
   cannot produce as an `IDENT`, so no `Var` can ever resolve to it).
   `\x. \` therefore parses as `Lam("x", Lam("?", Hole))` rather than
   bailing to bare `Hole`. See `decisions.md`.
4. **A headline property-based test anchors the totality claim.** 5000
   trials of `QCheck.string_printable` plus 2000 trials of
   `QCheck.string_small` (arbitrary bytes) assert `Parse_recover.parse`
   doesn't throw. Four other qcheck properties (clean-input correctness,
   prefix parsing, print-parse idempotence, prefix preservation under
   trailing garbage) guard against silent regressions.

p4-lambda-calculus is the lineage. p5 (multi-language), p6 (STLC), and
p7 (web interface) are orthogonal branches; p8 forks from p4 and ignores
them.

### Questions this prototype should answer

1. Does the menhir incremental API + single-token `HOLE` injection give
   intuitive recoveries on real REPL input, or does it produce surprising
   hole placements?
2. Is bare `Hole` (no payload) the right canonical form, or does the lack
   of hole identity bite when REPL users try to distinguish two
   placeholders in one term? (Answer so far: no friction. The REPL prints
   holes as `?` and users can tell them apart by position.)
3. Does the name-aware pretty-printer round-trip cleanly on terms with
   holes? `parse (pretty (parse s)) = parse s`? (Answer so far: yes — a
   qcheck property at 2000 trials confirms.)
4. What fraction of garbage inputs yield single-hole vs. many-hole parses
   in practice? Observe from the REPL's "(N holes inserted from
   recovery)" banner.
5. Does the totality PBT catch parser regressions, or is it too permissive
   to be useful? (Too early to tell — needs exposure over a few
   iterations.)

## Language

Untyped λ-calculus (p4's grammar, plus `HOLE`):

```
t ::= x                 -- variable (bound or definition name)
    | \x. t             -- lambda abstraction (ASCII backslash for λ)
    | t t               -- application, left-associative
    | (t)               -- grouping
    | ?                 -- hole (user-explicit OR recovery-inserted)
```

- `?` renders identically whether the user typed it or recovery inserted
  it. Distinguishing them is the REPL's job (it counts `?` chars in the
  input and reports the delta as "inserted from recovery").
- Identifier lexing unchanged: `[a-zA-Z_][a-zA-Z0-9_]*`.
- Unknown bytes in the input become `HOLE` tokens at the lexer layer.
  This keeps tokenization total and funnels all recovery through a
  single token.

## Architecture mapping

Four layers, structurally parallel to p4 with the deltas called out:

- **Store.** Shape unchanged. `Node.t` gains a fourth constructor `Hole`,
  with tag byte `'\x04'` (no payload). `Store.ingest` / `Store.reconstruct`
  gain the trivial passthrough arm.
- **Attachment.** Shape unchanged. Eval cache is still the only registered
  descriptor. A `Hole` at the head of evaluation becomes `Stuck(host)` —
  a pre-existing variant.
- **Language.** New `Parse_recover` module driving menhir's incremental
  API. `Ast.t` and `Surface_ast.t` gain a `Hole` constructor; `shift`,
  `subst`, `max_free_index`, `is_closed` get a passthrough arm. `Pretty`
  renders `Hole` as `?` in both the raw and name-aware printers.
  `Resolver.resolve` passes `Surface_ast.Hole` through as `Ast.Hole`;
  unbound names still surface as `Unbound_name` errors (recovery ends at
  the parser by design — conflating "syntax broke" with "name not bound
  yet" would mask typos).
- **Interface.** REPL mirrors p4's command set exactly. `parse` no longer
  has a `ParseError` variant — parsing is total. When a parse contains
  recovery-inserted holes (total holes minus explicit `?` chars), the
  REPL prints `(parsed; N holes inserted from recovery)` followed by
  `  tree: <pretty-printed surface>` before the hash, so the user can
  see what recovery produced. `:help` gains a Syntax line for `?`.

## In scope

- Total lexer: `?` → HOLE, unknown byte → HOLE, no exceptions.
- `HOLE` token and `atom: HOLE` production in `parser.mly`. Menhir
  switched to `--table` mode; `menhirLib` added to library deps.
- `Parse_recover.parse : string -> Surface_ast.t` — incremental API
  driver with HOLE-injection recovery, offender-drop fallback, and
  EOF-exhausted bail-out.
- `Surface_ast.t | Hole` and `Ast.t | Hole`, with `Ast.shift`/`subst`/
  `max_free_index`/`is_closed` extended to pass `Hole` through.
- `Node.t | Hole`, tag byte `'\x04'`, constant BLAKE2B digest. `Store`
  ingest/reconstruct extended.
- `Resolver.resolve`: `Hole → Ast.Hole`. Unbound names remain errors.
- `Eval.eval`: `Node.Hole` at the head → `Stuck(host)`. Three-line
  change.
- `Pretty`: `Hole` renders as `?` in raw, atom, app-left, and surface
  printers.
- REPL: `Parse_recover` integration; `parse_result` simplified (no
  `ParseError`); recovery-count banner; `:help` Syntax entry for `?`.
- Tests: carry forward p4's entire suite (parser, shift/subst, ingest,
  α-equivalence, eval, resolver, namespace, no-silent-breakage, pretty,
  hash-display). Add:
  - `totality`: 5000 trials of printable-string parsing; 2000 trials of
    arbitrary-byte parsing.
  - `recovery-properties`: clean-input parse (zero holes, round-trips),
    prefix parsing, print-parse idempotence, well-formed prefix survives
    trailing garbage (or full fallback to `Hole`).
  - `alpha-with-holes`: `\x. ? == \y. ?`, bare `?` hash stable.
  - `recovery-scenarios`: 15 explicit alcotest cases pinning recovery
    behavior on empty input, whitespace-only, single `?`, `? ?`,
    `\x. ?`, `\x. ? y`, unmatched `(`/`)`, dangling `\`, `\ .`, lone
    `.`, assorted gibberish, `(x) y`, `(x) ?`.
  - `pretty-with-holes`: `\x. ?` round-trips through print + parse.

## Out of scope

- `Ref(hash)` in either surface or AST — still inline-at-resolution.
- Hole subsumption / unification (Hazel's matched-hole logic).
- Typed holes, hole environments, hole closures.
- Per-occurrence hole provenance / source ranges (deferred to a future
  attachment).
- Error-recovering resolver (unbound names remain errors).
- Translation between languages — p8 is single-language.
- Web interface — p7's concern, not p8's.
- Collaborative editing / Grove.
- Fancier recovery heuristics beyond "inject HOLE, drop on stall."
- Special-casing Hole as a "value" for β-reduction (currently CBV
  evaluation treats Hole-in-argument-position as Stuck, so `(\x. x) ?`
  becomes Stuck rather than reducing to `?`). Acceptable for this
  prototype; call-by-name or lazy semantics are not the focus.

## What "done" looks like

- `dune build && dune runtest` green. Current suite: 72 tests (carried
  from p4's 46 plus 26 new scenarios + properties, including five
  fall-forward pin-downs).
- REPL session:
  - `\x. x` parses clean, hashes identically to p4's `\x. x`.
  - `\x. ?` parses clean (no recovery banner); prints back as `\x. ?`;
    evaluates to itself (Lam is a value).
  - `\x. \` parses to `Lam("x", Lam("?", Hole))` via grammar-level
    fall-forward, with "(parsed; 1 hole inserted from recovery)"
    banner; the recovery banner prints the pretty-printed tree.
    Unmatched open paren (e.g., `\x. (\`) still falls all the way
    back to bare `Hole` — the parser commits to expecting RPAREN and
    no grammar rule completes that path.
  - `(\x. x) ?` evaluates to `⟂ stuck at: (\x. x) ?` — CBV semantics
    bubble Stuck through application.
  - `\x:. asdf }{` (mixed gibberish) parses with 2 recovered holes;
    resolves fail with `Unbound_name: asdf` (correctly — recovery did
    its job at the parser, resolution is the user's problem now).
  - `:hash-of \x. ?` and `:hash-of \y. ?` print identical hashes (they
    ingest to the same Ast because holes and α-equivalence compose).
  - 10k-trial property tests never throw.

## Directory layout

```
prototypes/p8-holes/
  dune-project
  p8_holes.opam                # regenerated by dune
  _opam/                       # symlink to p3-naming-layer's switch
  src/
    dune                       # menhir --table, menhirLib library
    ast.re                     # de Bruijn + Hole arm in shift/subst
    surface_ast.re             # + Hole constructor + count_holes helper
    node.re                    # + Hole constructor, tag_hole = '\x04'
    parser.mly                 # + HOLE token, atom: HOLE rule
    lexer.mll                  # total: '?' → HOLE, unknown → HOLE
    parse_recover.re           # incremental-API recovery driver
    canonicalize.re            # identity (Hole is already canonical)
    hash.re                    # unchanged from p4
    store.re                   # + Hole ingest/reconstruct arm
    attachment.re              # unchanged from p4
    definition.re              # alias for Node.t
    eval.re                    # + Hole → Stuck(host)
    namespace.re               # unchanged from p4
    resolver.re                # Hole passes through; unbound still errors
    pretty.re                  # renders Hole as '?'
  bin/
    dune
    main.re                    # Parse_recover integration, recovery banner
  test/
    dune
    test_p8.re                 # p4 suite + p8-specific properties
  scripts/
    bootstrap.repl             # unchanged from p4
```

## Tech stack

Same as p4: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir 3.0
(`--table` mode), menhirLib, ppx_deriving, digestif (BLAKE2B), alcotest,
qcheck. `_opam` symlinks to p3's switch (via the existing p4 chain).
