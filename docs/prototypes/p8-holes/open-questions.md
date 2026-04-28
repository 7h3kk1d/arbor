# p8-holes open questions

Running list. Graduates to `decisions.md` when resolved, or to
`docs/design/open-questions.md` if the finding generalizes beyond p8.

---

## Canonical form

- **Is bare `Hole` enough?** Every hole hashes identically, so α-
  equivalence extends trivially to "equivalence up to holes." This is
  the substrate-coherent answer for this prototype, but it erases
  positional distinctness: `\x. ? ?` is one hash regardless of whether
  the user thinks of the two `?`s as "the same placeholder" or "two
  different things they haven't written yet." If that erasure bites —
  e.g., when future hole-aware evaluation wants to plug different
  values into different positions — per-occurrence identity has to
  come from somewhere. Proposed: an **attachment** keyed on the
  containing term's hash, carrying structural paths to holes. The
  canonical Node form stays identity-free. (Migrates to
  `docs/design/open-questions.md` if we pursue it.)

- **Hole subsumption / matching.** Hazel uses matched-hole and
  unmatched-hole distinctions for elaboration. We have neither. A
  future typed-holes prototype (p9 if it happens) will revisit.

## Recovery quality

- **Unmatched-open-paren pathological case.** When a trailing `(` has
  no matching RPAREN before EOF, the parser commits to expecting RPAREN
  and recovery can't reach EOF. The EOF-exhausted guard bails out with
  a bare `Hole`, losing the valid prefix. An ambitious recovery could
  "un-commit" (treat the stray `(` as an HOLE-in-atom-position,
  restoring the prefix) but requires checkpointing more than one
  InputNeeded state. Worth exploring if the bail-out rate is
  noticeable in practice.

- **Recovery-quality heuristics.** Current strategy inserts at most
  one HOLE per error site, then drops offending tokens one at a time.
  Alternative strategies: coalesce runs of non-atom-starter tokens
  into a single HOLE (instead of inserting / dropping per token);
  prefer dropping to inserting when the offender is whitespace-ish.
  Measure first — is any actual input producing weirdly-placed holes?

- **Bail-out rate.** How often does `Parse_recover.parse` fall all the
  way back to `Surface_ast.Hole`? The `prop_recovery_preserves_prefix`
  property accepts full-fallback as valid, but if the rate is high
  we've effectively lost the preservation guarantee for qcheck-random
  garbage. Add telemetry to the REPL (`:stats` extension?) to observe.

## Recovery surface

- **Resolver-level recovery.** Currently `Unbound_name` stays an error.
  Future `--recover-names` opt-in that turns unbound names into
  `Ast.Hole`? Would let users write `\x. foo x` and have `foo`
  become a hole until later bound. Trade-off: masks typos.

- **Richer hole surface syntax.** Hazel uses `?` for empty holes and
  `<|e|>` for non-empty holes (holes carrying a would-be expression).
  p8 only has empty. A non-empty hole would require the payload we
  deliberately rejected; defer to a typed-holes prototype.

## Evaluator semantics

- **Hole as Value?** CBV currently treats `?` in argument position as
  Stuck, so `(\x. x) ?` doesn't reduce. Making Hole a Value would give
  `(\x. x) ?` → `?` and more generally `(\x. M) ?` → `M[x := ?]`. This
  is closer to Hazel's indeterminate-result semantics. Not this
  prototype, but worth a test-bed.

- **Hole elaboration in the cache.** The `lc:eval:v1` aspect caches
  evaluation results. For a term containing holes, the "value" might
  change once holes are filled. We currently cache `Stuck(host)` for
  anything rooted at a Hole, which is safe but uninformative. No
  change needed now, but a future hole-aware evaluator will need to
  decide whether hole-containing results are cacheable.

## Carried forward from p4

These remain relevant for any successor prototype built on p8:

- `Ref(hash)` as a first-class AST constructor (roadmap Phase 2's
  remaining scope; still deferred).
- Normal-order reduction and full normalization beyond WHNF.
- Interactive step traces.
- Primitive parenthesized-sequence for Church-style encodings.
