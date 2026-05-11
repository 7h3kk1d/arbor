# Phase 10 — Minted labels (records, tuples, lists, mint-everything; aesthetic surface syntax)

**Status:** Scope doc for the tenth prototype.
**Design context:** `../../design/10-minted-identity.md` (minted identity, mint marks), `../../design/11-label-sort.md` (label sort, records, modules), `../../design/03-content-addressing.md` (canonicalization), `../../design/04-naming-layer.md` (namespace; hierarchical paths), `../../design/06-architecture.md` (four-layer decomposition).
**Prototype design lives here:** `docs/prototypes/p10-minted-labels/`.
**Implementation code lives at:** `prototypes/p10-minted-labels/`.

## Thesis

p10 is the first prototype to exercise two new substrate-level ideas at once:

1. **Mint-everything-by-default identity** (`10-minted-identity.md`). Every term, type, and label definition mints a fresh mark on creation. There is no `unique` keyword and no structural-by-default escape hatch. Coincidentally-equal definitions (`let pi = 3.14` typed twice in the same session) produce *different* hashes. The prototype's job is to find out what the substrate actually feels like when minted is the default mode — what wins, what surprises, where structural-sharing is missed.
2. **Labels as a first-class minted sort** (`11-label-sort.md`). `Definition.t = Term | Type | Label`. Record types reference labels by hash; field names never appear in stored bytes. Renaming a label is a namespace operation; record-type hashes do not move. Two record types sharing a field name *resolve to the same label* (intentional sharing on demand), but a label can only be created by an explicit declaration site — record literals do not auto-mint.

On top of this substrate sit four user-visible language features the prototype needs to feel honest: **tuples** (positional, distinct from records), **records** (labeled, riding the label sort), **lists** (built-in monomorphic, literal syntax, primitive registry ops), and **everything p9 already had** (Int, Bool, String, Arrow, holes, hierarchical names, the Bonsai web UI).

The fifth thing on the table — and the experiment p10 is most concretely doing — is **surface syntax aesthetics**. p9's syntax is functional but blocky; p10 aims for a Unison-ish look: minimal punctuation, indent for continuation, `Capitalized` segments for namespaces and types and `lowercase` segments for terms and labels, no `let ... in ...` clutter at top level.

The lineage: p9 (typed namespace + holes + Bonsai UI + primitive registry) → p10 (fork, replace parser, add labels/records/tuples/lists, switch to minted-by-default).

### Questions this prototype should answer

1. **What breaks when minted-by-default is on?** Structural sharing collapses: every `let x = 1 + 1` is a distinct definition with a distinct hash, even across rebinds. Does the namespace tree look reasonable? Does the namespace browser get cluttered with versioned hashes? Do the existing aspect caches still pay off, or do they thrash?
2. **Does the labels-via-namespace story feel right?** A record literal `{ x = 1, y = 2 }` requires `x` and `y` to already be bound in the resolution scope (either via a prior `type Point = { x : Int, y : Int }` declaration or a standalone label declaration). What happens when they're not — is the error message tolerable, or does it force premature ceremony?
3. **Does intentional label sharing emerge naturally?** Declaring `Point = { x : Int, y : Int }` then `Vector = { x : Int, y : Int }` should reuse `Point`'s `x` and `y` labels (because the names resolve before a fresh mint), giving `Point` and `Vector` shared field identity for free. Does this happen the way users expect, or do they want a separate gesture?
4. **Does Unison-ish syntax with `Capitalized`/`lowercase` disambiguation read cleanly?** Two cases of interest: `Geom.origin.x` (namespace-path `Geom`, term `origin`, then field `x`) and `p.x` (value `p`, field `x`). The lexer/parser distinguishes by the case of the leading segment; does the user *feel* the distinction without thinking about it?
5. **Do tuples and records cohabit comfortably?** Tuple destructuring in `let`, record projection by `.`, no destructuring on records (only projection). Does the asymmetry trip people up?
6. **Does the recovered-AST panel stay legible with the new syntax?** p9's panel was clearest when recovery inserted holes. With layout-sensitive parsing, what does recovery look like mid-expression?

## Language

Concrete surface syntax (deliberately tighter than p9):

```
top ::= def
      | def ⏎ top                       -- newline-separated top-level

def ::= type Name = ty                  -- type definition (mints a fresh type)
      | name : ty = expr                -- term definition with annotation
      | name = expr                     -- term definition, type synthesized

expr ::= name                           -- variable / namespace path / label (case disambiguates)
       | n | true | false | "s"         -- literals
       | \name : ty -> expr             -- annotated lambda
       | expr expr                      -- application (left-assoc)
       | let name = expr in expr        -- let (monomorphic)
       | let (name, name, …) = expr in expr   -- tuple destructuring
       | if expr then expr else expr    -- conditional
       | (expr, expr, …)                -- tuple (n ≥ 2)
       | { name = expr, … }             -- record construction
       | { expr with name = expr, … }   -- record update (functional)
       | expr . name                    -- field projection or namespace lookup
       | expr . n                       -- positional tuple projection (n is integer)
       | [ expr, … ]                    -- list literal
       | expr + expr | expr - expr | expr mul expr | expr / expr | expr mod expr
       | expr && expr | expr || expr | not expr
       | expr ++ expr                   -- String concat
       | expr == expr
       | ( expr )
       | ?                              -- hole

ty ::= Name                             -- named type (resolved via namespace)
     | Int | Bool | String              -- base types
     | ty -> ty                         -- arrow (right-assoc)
     | ( ty, ty, … )                    -- tuple type (n ≥ 2)
     | { name : ty, … }                 -- inline record type (rare; usually named)
     | List ty                          -- monomorphic list element
     | ( ty )

name ::= lowercaseIdent  ('.' lowercaseIdent)*    -- terms and labels
       | (CapitalizedIdent '.')* lowercaseIdent   -- namespace-prefixed term/label
       | (CapitalizedIdent '.')* CapitalizedIdent -- namespace-prefixed type
```

Notes:

- **Capitalized vs lowercase**: `Capitalized` segments are namespace path or type name; `lowercase` is term or label. The parser routes `Foo.Bar.baz` as namespace-path-`Foo.Bar` + name-`baz`; `Foo.Bar.Baz` as type lookup; `foo.baz` is value-`foo` projected at field-`baz`. The lexer emits `UIDENT` vs `LIDENT`; the grammar uses the distinction structurally.
- **Field projection vs namespace lookup uses the same `.`**: disambiguated by the case of the leading segment at parse time, then by the type of the result at resolution time.
- **`mul` keyword for Int multiplication** — same reason as p9 (`*` is reserved for tuple-type punctuation in the future; currently we use `,` for tuples).
- **Lambdas use `->`** instead of p9's `.` separator (e.g., `\x : Int -> x + 1`). Reads better; aligns with arrow type syntax.
- **Layout sensitivity** is *light*: newlines end an expression unless the next non-empty line is more indented than the current expression's start column. Continuation lines are passed through to the parser as if they were a single line. No off-side rule, no virtual braces, no `do`/`where` blocks.
- **Record literal syntax `{ x = 1, y = 2 }`** requires `x` and `y` to resolve to labels in the current namespace context. The Resolver does namespace lookup; if `x` is unbound, the ingest fails with a "label not declared" error. There is no auto-minting from literals in this prototype — minting is always an explicit gesture (type declaration mints its labels; future versions may add a standalone `label x : Int` declaration but it's not in v1).
- **Record update `{ p with x = 10 }`** evaluates `p`, copies its label/value pairs, and replaces the labels listed. Typecheck requires `p` to be a record type containing each updated label.
- **List literals `[1, 2, 3]`** are sugar for `list.cons 1 (list.cons 2 (list.cons 3 list.nil))`. Empty list `[]` requires a type annotation in context (`[] : List Int`).
- **Tuple projection `t.0`, `t.1`, …** uses integer literal after `.`. Parser accepts integer after a `.` token; resolver routes to `Fst/Snd`-style projection on the tuple AST.
- **No constructor syntax for record types**. Just `{ x = 1, y = 2 } : Point` to ascribe — the typechecker verifies the labels-and-types match `Point`'s definition.

### Minted identity in p10

- Every `type N = …` declaration mints a fresh `Type` definition. Two identical `type Foo = Int` declarations produce two distinct types with distinct hashes.
- Every term definition (`name = expr` at top level) mints a fresh `Term` definition. Two identical `pi = 3.14` produce two distinct terms.
- Every label introduced by a record type declaration mints a fresh `Label` definition, *unless the label name already resolves in the current namespace*, in which case the existing label hash is reused (intentional sharing).
- The mint mark itself is a 16-byte random value drawn at ingest. Re-running the same source through a fresh session produces different hashes; the bootstrap re-seeds on every page load (p9 already accepted state-resets-on-reload, this is the same posture).
- Mint marks are encoded inside the canonical node bytes: `Term` and `Type` nodes prepend their mark; `Label` nodes are essentially just the mark plus a `tag-byte`.

### Sorts and node encodings

`Definition.t` extends p9:

```
Definition.t = Term(Node.t) | Type(Ty.t) | Label(Label.t)
```

- **`Term` and `Type`** parallel p9, but every encoding includes the 16-byte mint mark immediately after the language tag byte. p9's hash space is intentionally incompatible.
- **`Label`** is new: tag byte `'L'`, then the mint mark, then nothing else. (Future versions may attach a declared-type aspect — that's an aspect, not content.)

`Ty.t` extends:

```
Ty.t = Int | Bool | String | Arrow(t, t)
     | Tuple(list(t))           -- new: n-ary positional
     | Record(list((Hash.t, t)))  -- new: labels by hash, sorted by hash
     | List(t)                  -- new: monomorphic element
     | Named(Hash.t)             -- new: reference to a stored Type definition
```

Tag bytes:

- `0x10` Int, `0x11` Bool, `0x12` String, `0x13` Arrow (unchanged from p9)
- `0x14` Tuple (replaces p9's binary Product)
- `0x15` Record — encoded as `0x15 || varint(n) || (label_hash || ty_encoded){n}`, with the pairs **sorted by label_hash** for canonical form
- `0x16` List
- `0x17` Named

Record field order in the canonical form is **sorted by label hash** (per `11-label-sort.md` §"Field order in the canonical form"). Surface order is preserved in the pretty-printer via a Surface_ast annotation.

## Architecture mapping

Four layers, deltas from p9:

- **Store.** `Definition.t` gains `Label`. `Node.t` gains `Tuple(list(Hash.t))` and `Record(list((Hash.t, Hash.t)))` and `List_lit(list(Hash.t))` and `Record_update(Hash.t, list((Hash.t, Hash.t)))` and `Project_field(Hash.t, Hash.t)` and `Project_index(Hash.t, int)`. `Pair`/`Fst`/`Snd` are dropped (subsumed by tuple-of-2). Language tag byte changes to `'Q'` to mark hash-space incompatibility with p9.
- **Attachment.** Aspect-value sum extends p9's; types referenced from aspect values are by `Hash.t` not inline. The `Type_of` and `Type_with_holes` values become `Hash.t` (as in p9's late extension) — a Type definition is reachable in the Store.
- **Language.**
  - `Lexer` distinguishes `UIDENT` (capitalized) and `LIDENT` (lowercase), emits `INT_DOT` token for `.0` style positional projection (lookahead-tricky; reuse menhir's standard trick), and recognizes `with` as a keyword inside `{ … }`.
  - `Layout` is a thin pre-lexer pass that inserts a `NEWLINE_SEP` token after end-of-line when the next non-blank line is at or before the current expression's indent column. Continuation lines pass through unchanged. Implementation is ~80 lines of OCaml.
  - `Parser` is rebuilt around the new surface — record/tuple/list literals, record update, `with` keyword, capitalized-vs-lowercase routing, `->` lambdas. Grammar-level partial-form productions are reworked: truncated `let`, `if`, `\name : ty ->` (the `->` half), and `{ name = ?` (mid-record-literal).
  - `Parse_recover` carries p9's incremental-API rewind-and-inject-HOLE driver — parameterize over the start symbol so type-pane and term-pane share the driver.
  - `Namespace` keeps the flat `Hashtbl` model. `resolve_query` is unchanged in mechanics; the case-distinction routing is done at parse-time, so what hits the namespace is already disambiguated.
  - `Resolver` ingests record/tuple/list AST. Record literals call into namespace to resolve label names; failure to resolve is an ingest error. Type declarations mint labels for un-resolved field names (the one place auto-mint happens — and only at type-declaration sites, not record-literal sites).
  - `Typecheck` extends p9's bidirectional checker with rules for `Tuple`, `Record`, `List_lit`, `Record_update`, `Project_field`, `Project_index`. Record sub-typing is **not** in scope (exact-label-set required for ascription); list element type checks under uniformity (all elements must check against the same type). Empty list `[]` requires expected type from context, else `Ill_typed("cannot infer list element type")`.
  - `Has_holes` is unchanged in shape; gains traversal cases for the new constructors.
  - `Eval` is CBV; tuples and lists are eager; field projection looks up by label hash; record update copies and replaces. Primitive registry serves `list.head`, `list.tail`, `list.cons`, `list.nil`, `list.length`, `list.map`, `list.filter`, `list.range`, `list.concat`, plus p9's `string.*` and `int.*`.
  - `Pretty` is rewritten around the new surface — record/tuple/list literals, `with` syntax, Unison-ish layout (newline + indent for long lines). Reverse-lookup of label hashes through the namespace; hash fallback when unbound.
  - `Mint` (new) — single source of 16-byte fresh marks. Pluggable for tests (deterministic counter mode) vs runtime (random).
- **Interface.** Bonsai + jsoo, three panes — structurally p9's. Deltas:
  - **Namespace tree** gains a third badge: `L` for labels (alongside p9's `T` for types; terms still get no kind-badge).
  - **Editor pane** keeps the term/type mode toggle, since labels in v1 are introduced only via type declarations — no label-mode pane.
  - **Detail view** gains record-shape rendering: for a `Type` whose body is `Record`, show the labels by name (via reverse lookup) with their field types. For a `Label`, show the hash, bound names, and any types that reference it (reverse-query by aspect).
  - **Recovered-AST panel** is kept; gains hole-marker support for partial record/tuple/list literals.
  - Bootstrap seeds: `Geom.Point` (`{ x : Int, y : Int }`), `Geom.Vector` (`{ x : Int, y : Int }` — reuses Point's `x` and `y` labels by namespace resolution), `Geom.origin`, `Geom.translate`, `Math.add/sub/mul/inc`, `List.range`, `List.sum`, `String.greet`, `Draft.todo` (holey).

## In scope

- Mint-by-default for every Term, Type, and Label definition. No `unique` keyword, no structural definitions.
- Three sorts: `Term`, `Type`, `Label`. Three node-encoding tag bytes for the type-language; labels get their own `'L'` language tag.
- Records (labeled, with content-addressed labels), tuples (n-ary positional), lists (monomorphic, literal + primitive ops).
- Record construction, projection by `.`, functional update `{ p with x = 10 }`.
- Tuple construction, tuple destructuring in `let`, positional projection `t.0`.
- Surface syntax: Unison-ish, light indent-sensitivity, capitalized-vs-lowercase routing.
- Holes integrate with new constructors; permissive bidirectional checker.
- Bonsai + jsoo three-pane UI with namespace tree, editor, recovered AST, detail view.
- Primitive registry serves list ops + p9's string/int ops.
- Substrate test suite covering: round-trip on all new constructors, mint-distinctness (same source ingested twice produces distinct hashes), label-sharing (Point and Vector use the same `x` hash), record canonicalization (field-order permutation produces equal hashes), tuple hash stability, list literal encoding, hole-in-record recovery, recovered-AST panel correctness on partial records/tuples/lists, primitive eval, layout sensitivity edge cases.

## Out of scope

- **Standalone `label x : Int` declaration form.** Labels are minted only by type declarations in v1. If the bootstrap or testing surfaces a need, add it in a v2.
- **Row polymorphism, record subtyping, structural module subtyping.** Doc 11 names these as destinations; not this prototype.
- **General sums / variants.** Deferred to a successor prototype (per the user's earlier answer). The label-sort will be exercised on constructors then.
- **Parametric polymorphism / generics.** `List` is the only parametric type and is a built-in special case. No general type variables.
- **Marks-survive-edits.** Doc 10's "second identity axis" reading is out of scope — mint marks are per-definition, an edit produces a new definition with a new mark.
- **HM let-polymorphism.** Monomorphic `let`.
- **Real unification in the checker.** Permissive non-unifying checker only.
- **`Ref(hash)` AST constructor.** Still inline-at-resolution.
- **Translation between languages.** p10 is single-language; tag byte `'Q'` reserves a fresh hash-space slot.
- **Namespace branching, history, multiple namespaces.** Deferred.
- **Persistent storage.** Bonsai bundle is stateless across page reloads; bootstrap re-seeds. (This is *especially* visible under mint-by-default — same source, different hashes on each reload, by design.)
- **Editor affordances for minting**: auto-mint shortcuts and pre-commit marking (doc 11) are out of scope for v1.
- **Multiple labels with identical names but distinct hashes.** Not supported in v1: once a label name is bound in the namespace, subsequent declarations reuse it. The "freshly mint a same-named label in a different namespace path" UX is open and can come later.

## What "done" looks like

- `dune build && dune runtest` green. Substrate test suite covering the cases listed under In scope, with ≥35 tests including parser-totality QCheck trials.
- `scripts/build-web.sh` produces `public/p10.js`. Opening `public/index.html` in a browser shows the three-pane app with seeded namespace.
- Manual UX checks:
  - `type Point = { x : Int, y : Int }` ingests; `Geom.Point` appears in the namespace tree with `T` badge; under it `Geom.Point.x` and `Geom.Point.y` appear with `L` badge.
  - `type Vector = { x : Int, y : Int }` ingests; `Geom.Vector.x` and `Geom.Point.x` resolve to *the same* label hash (reverse-query shows both record types using that label).
  - `origin : Point = { x = 0, y = 0 }` ingests; detail view shows the record body and the resolved label names.
  - `origin.x` resolves to `0`.
  - `{ origin with x = 10 }` produces a `Point` with `x = 10, y = 0`.
  - `let (a, b) = (1, "hi") in a` evaluates to `1`.
  - `[1, 2, 3]` ingests; type `List Int`. `List.range 0 5` evaluates to `[0, 1, 2, 3, 4]`. `List.sum [1, 2, 3]` evaluates to `6`.
  - Ingesting `pi = 3.14` twice in the same session produces two distinct hashes; the namespace tree shows the latest binding but both definitions remain reachable by hash.
  - Bare `{ x = 1, y = 2 }` with no `x`/`y` previously declared as labels — ingest fails with "label `x` not declared; introduce it with a record type first."
  - `{ x = 1, y = ? }` after Point is declared — type is `Type_with_holes(Point)`; has-holes true; namespace badge shows ◌.
  - Garbage `{ { {` — recovered panel shows partial record literal with holes; ingest path produces a holey term.
  - Layout: a record literal wrapped over multiple lines parses as one expression when continuation lines are indented past the opening `{`.

## Directory layout

```
prototypes/p10-minted-labels/
  dune-project
  p10_minted_labels.opam
  _opam/                            # symlink to p7-web-interface/_opam/_opam
  src/
    dune                            # menhir --table, menhirLib + digestif
    hash.re
    mint.re                         # 16-byte fresh marks; deterministic + random
    ty.re                           # Int|Bool|String|Arrow|Tuple|Record|List|Named, tag bytes 0x10..0x17
    label.re                        # Label definitions
    surface_ast.re                  # carries names; gains Record/Tuple/List/Update/Project_field/Project_index
    ast.re                          # de Bruijn for Lam + Let; record fields by label_hash
    node.re                         # shallow DAG; tag 'Q' + ~16 constructor tags; mint marks
    definition.re                   # Term | Type | Label
    canonicalize.re                 # sort record fields by label_hash
    layout.re                       # light layout pre-pass (newline / continuation)
    lexer.mll                       # UIDENT vs LIDENT; INT_DOT; total
    parser.mly                      # rich grammar with HOLE atoms + partial productions
    parse_recover.re                # incremental-API recovery
    pretty.re                       # surface printer + name-aware printer
    namespace.re                    # flat dotted map + suffix resolution + reserved keywords
    resolver.re                     # surface→internal; mints types/labels at declaration sites; ingest pipeline
    typecheck.re                    # permissive bidirectional checker; record/tuple/list rules
    has_holes.re                    # DAG-traversal aspect
    eval.re                         # CBV; primitives; Stuck on Hole
    attachment.re                   # aspect-value sum
    store.re                        # single-language ingest/reconstruct; mint-aware
  web/
    dune
    substrate.ml                    # singleton Store/Att/Namespace; Mint generator
    bootstrap.ml                    # seeds Geom.Point, Geom.Vector, Math.*, List.*, String.greet, Draft.todo
    state.ml                        # model + actions
    feedback.ml                     # keystroke→ingest pipeline
    recovered_view.ml               # post-recovery surface AST panel
    namespace_tree.ml               # tree with T / L badges
    editor.ml                       # textarea + bind input + term/type toggle
    detail.ml                       # selected hash detail; record + label rendering
    aspects_view.ml                 # typecheck / has-holes / eval rows
    app.ml                          # three-pane layout
  bin/
    dune
    p10_main.ml                     # Bonsai_web.Start.start App.component
  test/
    dune
    test_p10.re                     # ≥35 tests
  scripts/
    build-web.sh                    # dune build + cp to public/p10.js
  public/
    index.html
    styles.css
    p10.js
```

## Tech stack

OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.17, Menhir 3.0 (`--table` mode), menhirLib, ppx_deriving, digestif (BLAKE2B, the `digestif.ocaml` variant — required under jsoo), js_of_ocaml ≥ 5.6, Bonsai v0.16/v0.17, Virtual_dom v0.16/v0.17, Core v0.16, alcotest, qcheck, qcheck-alcotest. `_opam` symlinks to `p7-web-interface/_opam/_opam`.

## Open threads to track

These start in `open-questions.md` for the prototype and may bubble up to the substrate docs as findings:

- Mint-mark generation: 16-byte random works for a stateless web demo; what's the right answer for a persistent or reproducible context? (`docs/design/10-minted-identity.md` open sub-question.)
- Standalone `label x : Int` declaration form: do users want it once they hit the "I need a fresh label that's not part of a record yet" wall?
- Label-sharing UX: when two type declarations reuse a name implicitly, do users notice they're sharing? Would a confirmation prompt help, or just clutter?
- Recovered-AST panel for partial record/tuple/list literals: which incomplete forms produce useful holes and which produce noise?
- Does namespace-cluttering under mint-by-default need a "latest only" view, or is the tree's collapsibility enough?
