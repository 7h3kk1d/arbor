# Phase 5 — Multi-language substrate with arith→lc translation

**Status:** Scope doc for the fifth prototype.
**Design context:** `../../design/05-translation.md` (translators as derived aspects), `../../design/03-content-addressing.md` (no cross-language references), `../../design/02-definitions-and-derived-data.md` (procedure identity), `../../design/06-architecture.md` (four-layer decomposition).
**Prototype design lives here:** `docs/prototypes/p5-multi-language/`.
**Implementation code lives at:** `prototypes/p5-multi-language/`.

## Thesis

p1–p3 built the substrate around a single language (untyped arithmetic). p4 replaced that language with the untyped λ-calculus and validated α-equivalence via de-Bruijn canonicalization. Each prototype so far has held **one** language.

p5 is the first prototype where **two languages coexist in one Store** and the first that exercises `docs/design/05-translation.md`:

- `Definition.t` stops being a type alias and becomes a sum: `Arith(Arith_node.t) | Lc(Lc_node.t)`. Adding a third language is one new variant here plus per-language modules; Store, Attachment, and Namespace shapes do not change.
- A one-byte **language tag** ('A' for arith, 'L' for lc) is prepended to every node encoding, giving arith and lc hashes disjoint spaces. Cheap insurance on top of the already-distinct node encodings.
- **Store** enforces "no cross-language references" (`03-content-addressing.md`) at registration time: an arith parent whose child hash resolves to an lc definition raises `Language_mismatch`.
- **Resolver** gains an edit-time language guard: a name that resolves to a hash of the wrong language surfaces as `Language_mismatch(name, expected, actual)`, keeping the rule visible at parse time, not just at ingest.
- A hand-written **Church-encoding translator** (`arith-to-lc-church:translate:v1`) takes an arith definition and produces an lc definition with the usual Church interpretation: booleans `λt.λf. t/f`, numerals `λs.λz. sⁿ z`, Kleene predecessor, and the `c t e` idiom for `if`. The translation is recorded as a derived aspect on the **source** (arith) hash; its value is the target (lc) hash. Second runs return the cached target without re-ingesting.
- The **REPL** gains `:lang`, `:translate`, `:translations`, and language-labelled listings (`[arith]` / `[lc]`) on `:list`, `:names`, `:dag`, `:lookup`, `:show`, and `:stats`.

### Questions this prototype should answer

1. Does the `Definition.t = Arith | Lc` sum leak abstractions, or does Store stay language-agnostic at the table level while each language gets its own ingest/reconstruct/eval? (Answer so far: clean split — no dispatch plumbing in Store beyond the sum pattern-match.)
2. Is `(source_hash, "translation-to-lc", translator-identity) → target_hash` the right aspect shape for translations? Does the existing Attachment API carry it without changes beyond one new `aspect_value` variant?
3. Does the REPL's mode-based language switch (`:lang arith` / `:lang lc`) read naturally, or does per-command language selection scale better?
4. Is language-labelled output useful in practice? (Early answer: yes — `:list closed` becomes significantly more informative when ~85% of stored rows are intermediate Church-combinator subterms from translation targets.)
5. How faithfully does the CBV-WHNF lc evaluator agree with arith evaluation after translation? Testing this required a deep β-normalizer in the test harness (Lc_eval stops under binders; Church numerals and Kleene predecessor only match the canonical form under full β-normal form).

## Languages

Two languages, each carrying the surface syntax of its predecessor prototype verbatim:

**Arithmetic** (`:lang arith`, from p1–p3):

```
t ::= true | false | 0 | succ t | pred t | iszero t | if t then t else t | (t) | name
```

**λ-calculus** (`:lang lc`, from p4):

```
t ::= x | \x. t | t t | (t)
```

Namespace reserved keywords = union = arith's eight (`true false succ pred iszero if then else`). No lc keywords.

## Architecture mapping

Four layers unchanged in shape; the sum type propagates:

- **Store.** `Hashtbl.t(Hash.t, Definition.t)`. Public API grows: `ingest_arith` / `ingest_lc`, `reconstruct_arith` / `reconstruct_lc`, `register_arith_node` / `register_lc_node`, `language_of`. Cross-language parent→child references rejected at register time.
- **Attachment.** One new `aspect_value` variant: `Translation_target(Hash.t)`. Three registered descriptors: `arith:eval`, `lc:eval`, `translation-to-lc`. `by_value` and the new `entries_for` both work unchanged.
- **Language.** Two per-language module families: `Arith_ast`, `Arith_surface_ast`, `Arith_node`, `Arith_parser` (Menhir), `Arith_lexer` (ocamllex), `Arith_canonicalize`, `Arith_eval`, `Arith_pretty`; and `Lc_*` symmetric. Plus `Resolver` with two entry points and a language-guard error, `Arith_to_lc_church` (the translator), and a dispatching `Pretty` façade.
- **Interface.** REPL at `bin/main.re` with the added commands and a `current_lang: ref(string)` threaded through.

Translators living in Layer 3 (Language) is the one non-upward dependency accepted per `06-architecture.md:51`. The Church translator reads from Store (reconstruct the arith source), writes to Store (ingest the lc target), and writes to Attachment (record the aspect) — clean enough at prototype scale.

## In scope

- `Definition.t` sum type + `language` / `hash` helpers.
- Per-language Node encoding with a one-byte language tag prefix.
- Store's arith and lc ingest/reconstruct + cross-language reference rejection + `language_of`.
- Namespace carried from p4 with reserved list = union of both languages' keywords.
- Attachment carried from p4 plus `Translation_target(Hash.t)` and `entries_for(~aspect, ~procedure)`.
- Resolver with `~expected_language` guard and two entry points.
- Two evaluators (`arith:eval:v1`, `lc:eval:v1`), each with its own descriptor.
- `arith-to-lc-church:translate:v1` translator: Church booleans, Church numerals, SUCC combinator, Kleene PRED combinator, ISZERO combinator, `if` → plain application. Derived aspect `translation-to-lc`; cache-hit on second call; language check on source.
- Two pretty-printers dispatched by Definition language, plus a `language_tag` helper that produces a fixed-width `[arith]` / `[lc]   ` label for listings.
- REPL commands: all of p4's plus `:lang`, `:translate`, `:translations`. Listings (`:list`, `:list raw`, `:list closed`, `:names`, `:dag`, `:lookup`, `:show`) prepend the language tag. `:stats` splits definition counts per language and shows the translation-cache count. `:eval` dispatches on the Definition's language.
- Bootstrap script at `scripts/bootstrap.repl` that binds arith and lc definitions and translates a handful to demonstrate the cache.
- CLI flags: `--load`, `--no-repl`, `--step-limit`, `--lang [arith|lc]`.
- Tests: disjoint hash spaces, cross-language-reference rejection, per-language ingest/reconstruct roundtrip, α-equivalence still works, resolver language-mismatch errors, per-language eval correctness, translator ground cases (Church zero, one, two, booleans, iszero, pred, if), translation cache hit, aspect recording, non-arith source rejection, translations listing, and a qcheck property: `deep_normalize(translate(t)) = deep_normalize(translate(arith_eval(t)))` on small arith asts.

## Out of scope

- Reverse translation (`lc → arith`). Not generally decidable, not attempted.
- Lc → arith aspects or bidirectional-consistency checks.
- A second arith → lc translator (e.g., Scott encoding). The design supports it cleanly (different procedure identity, different cache slot) but we ship one.
- Automatic re-translation on source change; orphan-translation GC (deferred by `05-translation.md:§Staleness`).
- `Ref(hash)` as a first-class AST constructor in either language. p3/p4 both inlined at resolution; p5 inherits that stance.
- Declarative subset-embedding generator from `05-translation.md:§Subset translations`.
- Structural-embedding, verified-translator, Cambria-lens, or LLM-assisted machinery.
- Automatically binding a name for a translation target. Users `:bind` manually.
- Third, fourth, etc. languages. Two is enough to validate the sum-type shape.
- STLC, typed arithmetic, or any kinds machinery. Phase 6+.

## What "done" looks like

- `dune build && dune runtest` green. Current suite: 27 tests (disjoint hashes, cross-language rejection, roundtrips, α-equivalence, resolver language mismatch, arith + lc eval, 11 ground Church translation cases, 4 translation-cache cases, 1 qcheck property).
- Bootstrap script loads cleanly: `dune exec bin/main.exe -- --load scripts/bootstrap.repl --no-repl` exits 0, shows binds and translations.
- REPL session:
  - `:lang arith`, `:bind one succ 0`, `:bind two succ one`, `:translate two` → prints `translated #... -[arith-to-lc-church:v1]-> #...`.
  - `:translate two` a second time → prints `cached #... -[arith-to-lc-church:v1]-> #...`.
  - `:lang lc`, `:bind c_two \f. \x. f (f x)` — `c_two` and the deep-normalized form of the translator's output for `succ one` have the same hash (both are β-normal Church two).
  - `:list closed` shows both `[arith]` and `[lc]` rows with aligned hash columns and named-child collapsing in bodies.
  - `:names` shows each binding prefixed with its hash's language.
  - `:stats` shows `definitions: N (arith M, lc K)` and `translations: T`.

## Directory layout

```
prototypes/p5-multi-language/
  dune-project
  p5_multi_language.opam
  _opam/                         # local opam switch (symlinked to p3's)
  src/
    dune
    hash.re                      # unchanged from p3/p4
    definition.re                # NEW — Arith | Lc sum
    arith_ast.re                 # port of p3's ast.re
    arith_surface_ast.re         # port of p3's surface_ast.re
    arith_node.re                # p3's node.re + 'A' language tag byte
    arith_parser.mly             # p3's parser, tokens renamed A_*
    arith_lexer.mll              # p3's lexer, outputs A_* tokens
    arith_canonicalize.re        # identity (as p3)
    arith_eval.re                # port of p3's eval.re (arith:eval:v1)
    arith_pretty.re              # port of p3's pretty.re
    lc_ast.re                    # port of p4's ast.re
    lc_surface_ast.re            # port of p4's surface_ast.re
    lc_node.re                   # p4's node.re + 'L' language tag byte
    lc_parser.mly                # p4's parser, tokens renamed L_*
    lc_lexer.mll                 # p4's lexer, outputs L_* tokens
    lc_canonicalize.re           # identity (as p4)
    lc_eval.re                   # port of p4's eval.re (lc:eval:v1)
    lc_pretty.re                 # port of p4's pretty.re
    store.re                     # multi-language sum + language_of + rejection
    namespace.re                 # p4's Namespace + union reserved list
    attachment.re                # p4's Attachment + Translation_target
    resolver.re                  # resolve_arith + resolve_lc + language guard
    arith_to_lc_church.re        # NEW — the translator
    pretty.re                    # dispatching façade + language_tag helper
  bin/
    dune
    main.re                      # REPL: :lang / :translate / :translations +
                                 #  language-labelled listings
  test/
    dune
    test_p5.re                   # alcotest + qcheck suite
  scripts/
    bootstrap.repl               # arith + lc bindings + a few :translate calls
```

## Tech stack

Same as p3/p4: OCaml ≥ 5.2, Reason ≥ 3.12, dune ≥ 3.16, Menhir, ppx_deriving, digestif (BLAKE2B), alcotest, qcheck. Opam switch symlinked to `../p3-naming-layer/_opam/` during bringup, consistent with p4's decision.
