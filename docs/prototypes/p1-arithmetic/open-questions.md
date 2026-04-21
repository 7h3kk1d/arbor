# Open Questions — p1-arithmetic

Prototype-specific open items. Will grow as implementation surfaces them.

## Interface details

- **Hash display prefix length.** 8 hex chars? 12? Short enough to type, long enough to be unambiguous in practice for the prototype's small stores. Revisit during implementation.
- **Pretty-printer style.** Parenthesize aggressively for unambiguity, or minimally for readability? Leans minimal; parser should handle the minimal form.
- **Echo behavior when evaluating an already-stored hash.** Does `:eval <hash>` print the AST and then the value, or just the value? Small UX call.
- **Readline / arrow-key editing.** Phase 1 starts with `read_line` (no history, no editing). If the REPL feels bad, add `ocaml-linenoise` or similar.

## Evaluator details

- **Stuck terms.** In TAPL's semantics, `pred 0` and `iszero true` are stuck. In a big-step evaluator they surface as "no rule applies" — an exception, an error value, or a `Result`? REPL needs to print them cleanly; a `StuckAt(term)` tag is probably the right shape.

## AST / canonicalization

- **Canonical form for arithmetic.** This language has no α-equivalence, no commutativity, no obvious rewrites. The AST as parsed should be what we hash. Confirm this is right and nothing needs normalizing.
- **`ppx_deriving` derivations on the AST.** Which ones do we need from day one? Probably `eq`, `show`, `compare`; possibly `hash` for use elsewhere (though digestif produces our real hashes).

## Hashing

- **Encoding format details.** Resolved to hand-rolled (see `decisions.md`), but the specifics — byte layout per constructor, how to encode nested children, endianness — are still to be chosen during implementation.
- **Hash representation type.** Newtype `Hash.t` wrapping a hex string? A fixed-size `bytes`? Affects comparison and printing.

## Testing

- **Property-based test ideas.** Parse roundtrip is an obvious one. What else? Hash stability across shuffled test orders. Evaluator determinism. Stuck-state detection.
