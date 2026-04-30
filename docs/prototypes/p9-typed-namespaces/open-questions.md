# p9 — open questions

Running list. Each item is something we're not resolving in p9 itself but is worth tracking.

---

### Permissive checker — does the non-unifying version reject too many sensible programs?

The current synthesis-then-check pattern can't propagate constraints across siblings. `if ? then 1 else true` — the two branches have different types but neither is a hole, so the `check el (synth th)` step rejects it. Should the prototype escalate to a ref-cell unifier (no occurs check needed since types are first-order), or is "ambiguous branches reject" a feature rather than a bug?

Watch for: real-feeling examples where the user had to fully spell out a type to get past the checker.

---

### Holes in type annotations — should `\x: ?. body` mean something more than `\x: Int. body`?

Currently `parser.mly` admits `HOLE` as a `ty_atom` that defaults to `Ty.Int`. The lambda body is `Hole` anyway in any path that hits this (since the user truncated the annotation), so the choice doesn't affect well-formed-with-holes status — but it does affect the hash. Two terms `\x: ?. ?` and `\x: ?. ?` typed at different moments should hash the same; they do, because both default to Int. But if the user later resolves the hole differently, the hash changes accordingly.

A more principled answer: introduce `Ty.Unknown` as a real placeholder type that participates in hashing and forces a re-ingest when the hole resolves. Probably waits for the unification escalation above.

---

### Suffix resolution — is bare-leaf-mostly-fine UX, or does the ambiguity rate creep up?

Bootstrap deliberately seeds `math.add` and `vector.add` to demo `Ambiguous`. In real use, how often does a leaf collide once a user has 50+ bindings? Need to use the prototype to find out. If collisions are common, the editor could add a "suffix-resolved with N siblings" warning to chips.

---

### Recovered-AST panel: signal vs. noise

If recovery happens often (incomplete inputs as users type) the panel updates rapidly and might create visual noise. If it rarely fires (most keystrokes leave a clean parse), the panel is a wallflower. Watch which it is in practice. If too noisy, debounce ~100ms.

---

### `Type_with_holes(ty)` semantics — is "best guess" actionable enough?

The cached type may be wrong if the user fills holes in a way that contradicts the guess. The new term has a new hash so the old cache is fine, but the user looking at a stale "best guess" before re-saving may be misled. Two angles:

- UI label: currently "(best guess; contains holes)". Is that clear enough? If users misread it as a final type, escalate.
- Should the aspect store distinct "we synthesized Int from a hole" from "we synthesized Int from a literal"? Probably no — the has-holes badge already communicates the uncertainty.

---

### `let`-without-`in` recovery — how often does it happen?

The grammar admits `LET binder EQ rhs` (no IN body) → `Let(x, rhs, Hole)`. Useful when the user's typing `let x = ` and pauses. Watch the recovered-AST panel during real use to see if this actually fires usefully or if the user's typing pattern usually completes the `in` quickly enough.

---

### Has-holes aspect cache — when does it grow too large?

Every pre-bind ingest creates a new hash; the aspect cache adds one entry per visited node. After 100 keystrokes on a 50-node term, that's 5,000 entries. Real use would be 10x worse. If memory shows up as an issue, GC unreferenced hashes (none reachable from any namespace binding) periodically.

---

### `Ref(hash)` and "follow named-term dependencies" — does p9's inline-at-resolution lose anything we'd want?

`04-naming-layer.md` long-term has stored programs carrying `Ref(hash)` rather than inlined subtrees. The has-holes-transitively aspect would then follow refs through the namespace explicitly, which is closer to how users *think* about dependencies. p9 dodges this by relying on inlined subtrees being equivalent. Watch for cases where users find the inline behavior surprising — e.g. a holey definition being inlined into ten places, all of which then show ◌ even though the user thinks of them as independent.

---

### Web bundle size

`public/p9.js` is ~26 MB compiled. Acceptable for a prototype but obviously not what we'd ship. Watch where the bulk comes from (Core's deps, js_of_ocaml's runtime) — this is shared with p7 and would be a substrate-wide concern when we're ready to think about actual deployment.

---

### Bonsai version pinning

p9 inherits p7's switch, which is on Bonsai v0.16. v0.17+ has a different API. If/when we bump, p7 and p9 move together.
