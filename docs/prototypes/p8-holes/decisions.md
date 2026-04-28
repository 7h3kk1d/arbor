# p8-holes decisions

Append-only ADR-lite log. Reversals get new entries.

---

## 2026-04-24 — Fork from p4, not p5/p6/p7

**Decision.** p8 forks `prototypes/p4-lambda-calculus/` wholesale. p5's
multi-language pair, p6's STLC + translators, and p7's web interface are
orthogonal branches; none of them are needed to exercise the hole
question.

**Rationale.** Disposable-prototype discipline (`CLAUDE.md` §Conventions):
each prototype answers specific questions. p8's question is about
incomplete programs, not types, not multi-language, not UI. Forking from
p4 gives the minimum surface area — one AST, one evaluator, one REPL —
and lets every change focus on the hole layer. Carrying STLC or the web
interface forward would dilute the focus without adding evidence.

**Alternatives considered.** (a) Fork p6, keep STLC alongside: useful
once we want *typed* holes, but p8 is explicitly about *untyped* holes;
the type question is orthogonal and belongs to a later prototype. (b)
Fork p7 and layer holes into the web UI: the web layer adds noise for
this question; interface work belongs in a later iteration once the
substrate-level hole story stabilizes.

---

## 2026-04-24 — Bare `Hole` (no payload) as canonical form

**Decision.** `Surface_ast.Hole`, `Ast.Hole`, `Node.Hole` carry no
payload. Tag byte `'\x04'` is the entire encoding. All holes hash
identically; `\x. ?` and `\y. ?` ingest to the same hash, extending
p4's α-equivalence-via-de-Bruijn story with "equivalence up to holes."

**Rationale.** This is the first prototype-level answer to
`docs/design/open-questions.md`'s "Holes and incomplete programs"
entry, and it needs to be substrate-coherent.
- Unique hole IDs (`Hole(int)` per occurrence) break cross-session hash
  determinism: the same text produces different hashes depending on a
  counter that's meaningless outside the parse. That's incompatible
  with content addressing's whole point.
- Source-text payloads (`Hole(string)`) weld hashes to incidental lexer
  output — two programs that are "broken the same way" would differ
  because one used `}{` as garbage and the other used `@#`.
- Source-range payloads (`Hole(start, end)`) perturb every hash when a
  trivial whitespace edit shifts downstream offsets.

Bare `Hole` is the defensible, Hazel-adjacent position for this
prototype. If identity-bearing metadata is later wanted (e.g., for
Hazel-style hole closures or per-occurrence provenance), it belongs in
an **attachment** — keyed by some combination of the containing term's
hash and a positional identifier — not in the canonical Node form.

**Alternatives considered.** Listed above. Note also that `docs/design/
03-content-addressing.md`'s non-goals section explicitly allows
per-language hole encodings, which gives p8 room to revise this choice
later without substrate fallout.

---

## 2026-04-24 — Menhir incremental API with HOLE-injection recovery

**Decision.** `src/dune` compiles `parser.mly` with `--table`, enabling
`Parser.Incremental.main` and `Parser.MenhirInterpreter`. A new
`src/parse_recover.re` module drives the incremental parser manually.
On `HandlingError`, recovery rewinds to the last `InputNeeded`, offers a
synthetic `HOLE` in place of the offending token, and continues; if
HOLE also doesn't fit at that position the offender is dropped and the
next queued token is tried.

**Rationale.** The user's plan explicitly required using menhir's
recovery mechanisms, ruling out a hand-rolled recursive-descent rewrite.
Of menhir's options:
- **Grammar-level `error` productions** are the older, deprecated path.
  They couple recovery to the grammar and can't inspect *why* recovery
  fired. Ruled out.
- **Incremental API** is the modern idiomatic surface. It keeps recovery
  code out of `parser.mly` (the grammar remains the plain p4 grammar
  plus a `HOLE` atom rule) and gives us programmatic control — including
  the ability to detect "HOLE doesn't fit here, drop instead."

Because the grammar admits `HOLE` as an `atom`, a single injected HOLE
can unblock any position where an atom/app_expr/expr was expected. The
offender-drop fallback catches the residual cases (`\` expecting an
IDENT, where HOLE is syntactically invalid).

**Alternatives considered.** (a) Menhir's `--error-recovery` / grammar
`error` token. Rejected as deprecated and grammar-coupled. (b) A layered
approach where menhir parses well-formed sub-expressions and a
hand-rolled wrapper segments the input at recovery points. Rejected as
throwing away menhir's LALR work. (c) Pre-process tokens (insert
synthetic RPAREN for every unmatched LPAREN, etc.). Rejected as a hack
that doesn't generalize.

**Consequence.** For inputs where recovery genuinely can't make
progress — classically, an unmatched trailing `(` that commits the
parser to expecting RPAREN — the recovery loop would spin, inserting
HOLEs that unblock only to hit the same failure. An **EOF-exhausted
guard** bails out with a bare `Hole` when the offending token is EOF and
HOLE doesn't fit. The totality invariant holds; the property test
accommodates this by accepting "full fallback to `Hole`" as a valid
outcome.

---

## 2026-04-24 — Lexer is total; unknown byte → HOLE

**Decision.** `src/lexer.mll` no longer raises `Lex_error`. `?` is
lexed as `HOLE` (the explicit user-entered hole). Any unrecognized byte
is also lexed as `HOLE` and the position is advanced one byte.

**Rationale.** For `Parse_recover.parse` to be total, the tokenizer has
to be total too. Funneling unknown bytes into `HOLE` (rather than
introducing a separate `BAD` token or skipping silently) keeps the
recovery vocabulary single-channel: every recovery problem is "HOLE
landed in the wrong place," handled by the same incremental-API code
path.

**Alternatives considered.** (a) Silently skip unknown bytes: loses
information about what the user typed. The recovery layer can't know
that garbage was present, so the hole-count banner would misreport. (b)
Emit a separate `BAD` token with the offending byte. More principled
but doubles the recovery vocabulary for no practical gain in this
prototype. Revisit if debugging reveals cases where the distinction
would have helped.

---

## 2026-04-24 — Recovery ends at the parser; resolver keeps its errors

**Decision.** `Resolver.resolve` passes `Surface_ast.Hole` through as
`Ast.Hole`. Unbound names continue to surface as `Unbound_name` errors;
there is no "unbound name → Hole" recovery.

**Rationale.** Conflating "syntax broke" (user typed `\x:. x`) with
"name not yet bound" (user typed `id x` before binding `id`) would mask
typos. Unbound-name recovery is a separate feature — easy to add later
behind an opt-in switch if it proves ergonomically necessary — and
deserves its own consideration.

**Alternatives considered.** (a) Turn every `Unbound_name` into an
`Ast.Hole`: too aggressive; the REPL loses the signal that a name was
intended. (b) Add a `--recover-names` flag to the REPL: reasonable
future extension; deferred until we see the ergonomic need.

---

## 2026-04-24 — Evaluator treats `Hole` as a stuck value

**Decision.** `eval_node` on `Node.Hole` returns `Stuck(host)` — the
same treatment as a free `Var` at the head of reduction. CBV evaluation
of an application whose head or argument reduces to Stuck yields Stuck
for the whole thing.

**Rationale.** Holes are opaque leaves with no reduction rule. The p4
evaluator already handles this pattern via `Stuck(host)` for free
variables; reusing the same result variant requires no changes to the
`aspect_value` sum or the cache protocol. Three lines of change
total.

**Consequence.** `(\x. x) ?` does not reduce to `?`; CBV sees `?` as a
Stuck argument and bubbles Stuck through the application. An alternate
semantics (treat Hole as a valid "value" for β-reduction, so
`(\x. M) ?` reduces to `M[x := ?]`) would give more informative output
for holes-in-argument positions. It's worth considering in a follow-up
prototype focused on hole-aware evaluation; p8's scope is parsing.

**Alternatives considered.** (a) Hole as a Value: changes the β-rule
slightly and produces more useful REPL output, but conflates "I have no
content" with "I have content you should care about." (b) A new `Hole`
result variant in `Eval.result`: requires extending `aspect_value` and
the cache semantics. Unnecessary for a parsing-focused prototype.

---

## 2026-04-24 — Fall-forward via partial-lambda grammar productions

**Decision.** `parser.mly` gains three partial-lambda productions
alongside the full `BACKSLASH binder DOT expr` rule:

```
expr: BACKSLASH binder DOT expr   (* full *)
    | BACKSLASH binder DOT        (* missing body *)
    | BACKSLASH binder            (* missing dot + body *)
    | BACKSLASH                   (* dangling *)
    | app_expr
binder: IDENT | HOLE { "?" }
```

Truncated lambdas now reduce to Hole-bearing `Lam`s instead of
bailing out to a bare `Surface_ast.Hole` via the EOF-exhausted guard.
`\x. \` parses as `Lam("x", Lam("?", Hole))` rather than `Hole`.

**Rationale.** The previous recovery strategy was exclusively
*backward-falling*: when no path forward existed, the driver
discarded the whole partial structure and returned a bare `Hole`. A
user writing `\x. \` in the REPL (mid-edit) got no signal that they
had started two lambdas; the hash was the bare-hole hash. Declarative
grammar productions give the parser a legitimate way to complete
truncated lambdas, preserving the valid-so-far structure.

Two mechanical properties of LALR(1) make this safe without
introducing shift-reduce conflicts or changing mid-expression
recovery behavior:
- `FOLLOW(expr) = {EOF, RPAREN}`. LALR reductions fire only on
  lookaheads in FOLLOW, so the partial rules take effect only at
  end-of-expression positions. Mid-stream garbage (`\x:. y`) still
  produces `HandlingError` at the same states as before and routes
  through the driver's drop-offender path; the new rules do not
  shortcut past the driver.
- The shift-reduce resolution for `BACKSLASH` + IDENT-lookahead
  and kin is shift by default, which is what we want — prefer the
  full form when more input is available.

**Binder name for the dangling case.** A synthesized binder needs a
string; any choice that the lexer can produce as an IDENT risks
colliding with a user-typed variable reference. We pick `"?"` — the
HOLE character is not a valid `IDENT` under the lexer regex
`[a-zA-Z_][a-zA-Z0-9_]*`, so no `Var(name)` can ever resolve to the
synthesized binder. The binder is "dead by construction."

The grammar also accepts HOLE explicitly in binder position
(`binder: IDENT | HOLE { "?" }`), so a user-typed `\?. body` parses
to the same `Lam("?", body)` shape. The pretty-printer emits `\?. ?`
for `Lam("?", Hole)`, which round-trips through the HOLE-binder rule.
A user who types `\?. body` is opting into the hole-binder form
explicitly, mirroring the "user-typed `?` vs. recovery-inserted `?`"
indistinguishability from the bare-Hole canonical-form decision.

**Consequence for hashing.** Binder names erase at resolution
(Surface_ast.Lam(_, body) → Ast.Lam(body')), so `\?. ?`, `\x. ?`, and
`\` all ingest to the same hash. "Equivalence up to holes" and
α-equivalence compose. The EOF-exhausted guard in `parse_recover.re`
is retained for cases the grammar still can't absorb (most notably
unmatched open paren — the parser commits to expecting RPAREN and no
reduction path completes). Those still return bare `Hole`, as noted
in `open-questions.md` under *Unmatched-open-paren pathological case*.

**Alternatives considered.** (a) Driver-level stack-walking that
inspects the LR state and synthesizes completion tokens at
`EOF-exhausted`. Rejected: couples recovery to generated state
numbers and re-does work LALR can do declaratively. (b) Synthesize
binder name `"_"`. Rejected: `_` is a valid IDENT, so a user's `\_. _`
would resolve the inner `_` to the dangling-binder's slot, polluting
semantics. (c) `%on_error_reduce expr`. Rejected: unnecessary given
that FOLLOW(expr) already restricts the partial rules to the desired
lookaheads, and adding it would risk eager reduction on inputs like
`\x:. asdf` where mid-expression recovery is preferred.

---

## 2026-04-24 — Pretty-printer renders holes as `?`

**Decision.** Both the raw and name-aware pretty-printers render
`Node.Hole` / `Surface_ast.Hole` as the single character `?`. No
parenthesization needed; `?` is syntactically an atom.

**Rationale.** Round-trip is guaranteed by construction: the lexer
tokenizes `?` as `HOLE`, the grammar admits `HOLE` as `atom`, and the
parse-then-print cycle preserves Hole nodes. `?` is the conventional
Hazel/Agda hole glyph and is a single byte; `{?}` and `[?]` would add
surface-layer noise with no extra information.

**Alternatives considered.** (a) `{?}` for visual emphasis: adds two
characters and a grammar extension (`LBRACE` / `RBRACE` tokens) for no
benefit. (b) `?<id>` per-occurrence labels: contradicts the bare-Hole
canonical form.
