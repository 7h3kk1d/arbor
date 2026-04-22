# Open Questions — p1-arithmetic

Prototype-specific open items. Will grow as implementation surfaces them.

## Interface details

- ~~**Hash display prefix length.** 8 hex chars? 12? Short enough to type, long enough to be unambiguous in practice for the prototype's small stores. Revisit during implementation.~~ Resolved 2026-04-21: 12 chars. See `decisions.md`.
- ~~**Pretty-printer style.** Parenthesize aggressively for unambiguity, or minimally for readability? Leans minimal; parser should handle the minimal form.~~ Resolved 2026-04-21: minimal parens, property-tested roundtrip. See `decisions.md`.
- **Echo behavior when evaluating an already-stored hash.** `:eval <prefix>` currently prints only the evaluated value. Should it also echo the AST first? Small UX call; revisit once the REPL sees real use.
- ~~**Readline / arrow-key editing.** Phase 1 starts with `read_line` (no history, no editing). If the REPL feels bad, add `ocaml-linenoise` or similar.~~ Started with plain `read_line` 2026-04-21. Open to revisit if it chafes.

## Evaluator details

- ~~**Stuck terms.** In TAPL's semantics, `pred 0` and `iszero true` are stuck. In a big-step evaluator they surface as "no rule applies" — an exception, an error value, or a `Result`? REPL needs to print them cleanly; a `StuckAt(term)` tag is probably the right shape.~~ Resolved 2026-04-21: `Eval.result = Value(Ast.t) | Stuck(Ast.t)` carrying the innermost stuck term. Note: `pred 0` actually reduces to `0` per TAPL E-PRED-ZERO — it's *not* stuck. Real stuck examples are `succ true`, `pred false`, `iszero true`, `if 0 then ...`.

## AST / canonicalization

- ~~**Canonical form for arithmetic.** This language has no α-equivalence, no commutativity, no obvious rewrites. The AST as parsed should be what we hash. Confirm this is right and nothing needs normalizing.~~ Confirmed 2026-04-21. `Canonicalize.canonicalize` is the identity function; the module slot exists so callers can always canonicalize-before-hashing and future languages have a place to plug in.
- ~~**`ppx_deriving` derivations on the AST.** Which ones do we need from day one? Probably `eq`, `show`, `compare`; possibly `hash` for use elsewhere (though digestif produces our real hashes).~~ Resolved 2026-04-21: `eq`, `show`, `ord`. No `hash` — digestif is the real hash.

## Hashing

- ~~**Encoding format details.** Resolved to hand-rolled (see `decisions.md`), but the specifics — byte layout per constructor, how to encode nested children, endianness — are still to be chosen during implementation.~~ Resolved 2026-04-21: one tag byte per constructor (`0x01..0x07`) followed by children in source order, no length prefixes (the AST is unambiguous from tags alone). See `src/hash.re`.
- ~~**Hash representation type.** Newtype `Hash.t` wrapping a hex string? A fixed-size `bytes`? Affects comparison and printing.~~ Resolved 2026-04-21: `type t = string` (lowercase hex). Currently a plain alias — tighten to `private` if it earns its keep.
- **Hash algorithm.** We wrote BLAKE2B via digestif because digestif 1.3.0 doesn't expose BLAKE3; see `decisions.md` for the tradeoff. Open question: is the functional equivalence assumption load-bearing enough to warrant revisiting once a later prototype wants BLAKE3 specifically (the OCaml `blake3` package exists as a separate dep)?

## Testing

- ~~**Property-based test ideas.** Parse roundtrip is an obvious one. What else? Hash stability across shuffled test orders. Evaluator determinism. Stuck-state detection.~~ Implemented 2026-04-21 in `test/test_p1.re`: pretty-print/parse roundtrip, hash determinism across repeated invocations, hash equality on structurally equal ASTs, stuck-state detection, store register/lookup idempotence, prefix-lookup resolution. Add more as questions surface.

## Surfaced during implementation

- **Error reporting in the REPL.** Parse errors currently print a bare `!! parse error` with no position or context. Menhir's `MenhirLib` ships incremental parsing and error messages; worth the complexity if the REPL gets used for long sessions. Lexer errors do carry the offending character.
- **`:help` for unrecognized commands.** Unknown `:foo` commands hint at `:help` but don't suggest the closest match. Not worth Levenshtein for a seven-command REPL.
- **Persistence invariants.** The Store is idempotent (registering an equal definition is a no-op). This means hash collisions would silently conflate two definitions — not a concern for a cryptographic hash, but worth noting if we ever swap in a weaker one.
