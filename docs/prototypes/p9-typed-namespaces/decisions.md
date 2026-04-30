# p9 — decisions

ADR-lite log. Append-only; reversals get new entries.

---

### 2026-04-29 — single typed language with let, pairs, and primitives

**Decision:** the language is a fresh single sum: `Int | Bool | String | Arrow | Product` types; expressions include lambdas (annotated, monomorphic), `let` (monomorphic), `if`, pairs with `fst`/`snd`, and primitive operations (`+ - mul / mod && || not ++ ==`). No lc, no stlc, no Church-encoded booleans.

**Why:** previous prototypes either used p4's untyped lc (no types to test) or p6's lc+stlc pair (the type story is split). p9 wants to evaluate the substrate against one richer typed language with enough expressive surface (literals, pairs, primitives, conditionals) to demo realistic UX without the complexity of polymorphism.

**Considered alternatives:**

- *No lambdas, just `let` and primitives.* Too narrow — a definition can only be a literal expression, and library reuse via the namespace becomes uninteresting.
- *HM let-polymorphism.* More expressive (`let id = \x:T. x in (id 1, id true)` would work, etc.), but adds a real unification algorithm with generalization/instantiation — substantially more substrate code than the rest of the prototype combined. Saved as an open question if the monomorphic version proves too restrictive.

**Implications:** `mul` is a keyword (because `*` is the product-type constructor). `Eq` is the only "polymorphic-flavored" primitive; the checker synthesizes the LHS type and checks the RHS at the same type. `let x = e1 in e2` binds `x` at the synthesized type of `e1` — no generalization.

---

### 2026-04-29 — hierarchical names live in the editing layer, not the substrate schema

**Decision:** names remain opaque strings at the substrate level (per `docs/design/04-naming-layer.md`). Hierarchy is a convention: `math.add` is one string; `Namespace.resolve_query` knows how to split on `.` and match by whole-segment suffix. The namespace browser's tree is built lazily per render in the UI; the substrate stays flat.

**Why:** `04-naming-layer.md` Threads-under-exploration §Hierarchical paths is open. The cheaper of the three options listed ("editing-layer convention only") is what we want first — it keeps the substrate uncommitted to a hierarchy story while letting us evaluate dotted names + suffix resolution UX. If a substrate-aware structure later turns out to be load-bearing (e.g., for prefix-rebind or namespace branching at sub-paths), we can add it then.

**Implications:** prefix queries cost O(N) per render in the tree (fine at prototype scale). The `Ambiguous` resolution error must scan all entries (also O(N), same reason). Reserved-name discipline now applies per-segment: `let.x` is rejected because `let` is a reserved keyword segment.

---

### 2026-04-29 — longest-segment-suffix resolution with a distinct Ambiguous error

**Decision:** `Namespace.resolve_query name`:

1. Try the full string as a key (fast path).
2. Else split on `.`, scan all entries, find those whose dot-segments end with the query's segments. Zero → `Unbound`. One → `Ok hash`. Multiple → `Ambiguous(list(string))`.

The suffix must align on dot-segment boundaries: `add` matches `math.add`, but `dd` does not match `add`.

**Why:** Unison's bare-leaf style ("just write `factorial`, the editor figures out which one") is a major UX win when names are unambiguous. The substrate-level discipline (whole-segment matching, distinct ambiguity error) ensures that the editor can either resolve confidently or surface a clear list of candidates instead of silently picking one.

**Considered alternatives:**

- *Substring suffix matching (no segment alignment).* Confusing — `dd` would match `add`, `ult` would match `result`. Hard to keep meaningful.
- *Pick a tiebreaker on Ambiguous (e.g., shortest entry name).* Felt arbitrary; the user has no way to know the rule.

**Implications:** the editor must communicate `Ambiguous` errors well (the candidate list is part of the error). When users seed both `math.add` and `vector.add`, bare `add 1 2` must show both.

---

### 2026-04-29 — permissive bidirectional checker with three aspect outcomes

**Decision:** `Typecheck.check_top` returns one of:

- `Well_typed(ty)` — fully typed, no holes; cache `Type_of(ty)`.
- `Well_typed_with_holes(ty)` — best-guess type; some hole was encountered or downstream constraints couldn't be confirmed because of upstream holes; cache `Type_with_holes(ty)`.
- `Ill_typed(msg)` — hard mismatch (e.g. `1 + true`, `(\x: Int. x) "abc"`); reject ingest.

Hole rules:

- `check ctx Hole expected → Ok` (hole accepts any expected type), `has_holes := true`.
- `synth ctx Hole → Ok((Int, true))` — best-guess type Int when no context demands otherwise.

Whenever an upstream synth produced a holey type and a downstream check would normally raise a mismatch, the mismatch is swallowed and `Type_with_holes(...)` is returned with a permissive best-guess type. Hard mismatches (both sides fully concrete and disagreeing) still reject.

**Why:** holes are a UX-first construct — they exist to let the user save partially-finished work. Rejecting at ingest because we can't fully type a holey term defeats the purpose. But silently dropping the typecheck would lose information. The two-state cache (`Type_of` / `Type_with_holes`) lets the UI surface "this is what we know so far" plus a "best guess; contains holes" caveat.

**Considered alternatives:**

- *Skip typecheck when holes present; only cache typecheck on hole-free terms.* Simpler, but loses partial type info in exactly the case where the user most wants help.
- *Real unification (ref-cell unifier with no occurs check).* More accurate; the planned escalation if the non-unifying version turns out to reject too many sensible programs.

**Implications:** the type cached in `Type_with_holes(ty)` is "what we know so far," not a final type. Users may fill a hole in a way that produces a different type; the new term has a different hash, so the old cache is irrelevant. The UI must label this clearly (currently: "best guess; contains holes" suffix on the type display).

---

### 2026-04-29 — has-holes-transitively as a derived aspect via DAG traversal

**Decision:** `has-holes:v1` is a derived aspect computed by recursing on `Node.children` from the root hash; result is cached at every visited node as `Has_holes(bool)`.

**Why:** since p1-p6 store closed terms (no `Ref(hash)` AST constructor), "transitive dependencies" is just the stored DAG of child hashes. Traversing the DAG gives transitivity for free. Sub-DAGs have stable has-holes status because of content addressing, so caching at every node is correct.

**Implications:** the aspect cache grows with every keystroke (since every pre-bind ingest produces a new hash). At prototype scale it's fine; if it becomes a memory issue, the obvious mitigation is GC of unreferenced hashes.

If p9 were to introduce `Ref(hash)` later (per `04-naming-layer.md` long-term), the aspect would need to follow refs through the namespace too. That's a clean extension.

---

### 2026-04-29 — Let binds by de Bruijn index just like Lam

**Decision:** `Surface_ast.Let(name, rhs, body)` carries the name for parsing/printing. `Ast.Let(rhs, body)` drops the name; the body's binder is implicit at de Bruijn index 0. `Node.Let(rhs_hash, body_hash)` likewise has no name in its hash encoding.

**Why:** the substrate has used de Bruijn + α-equivalence-by-canonicalization for binders since p4. Extending that to Let keeps the substrate property uniform: `let x = 1 in x` and `let y = 1 in y` produce identical hashes. The resolver pushes Let's binder name onto the context for the body only (not the rhs); pretty-print regenerates fresh names from context.

**Implications:** Let and Lam have distinct tag bytes in the Node encoding, so `let x = e in body` is *not* hash-equivalent to `(\x:T. body) e` — they're semantically related (and the evaluator treats them similarly via β-reduction) but the substrate keeps them distinct because `Lam` carries a type annotation that `Let` doesn't, and the user-level intent differs.

---

### 2026-04-29 — recovered-AST panel re-renders on every keystroke

**Decision:** the editor's `on_input` runs `Parse_recover.parse` (cheap; bounded by `max_errors = 1024`) on every keystroke and feeds the resulting Surface_ast.t to the recovered-AST panel. The panel highlights `Hole` constructors with a distinct visual.

**Why:** this is the headline UX of p9 — when the parser inserts holes to recover from malformed input, the user sees exactly where they landed. Debouncing would defeat the purpose: the recovery tree shifts as the user types, and that's the value.

**Implications:** if the recovery turns out to be slow on long inputs, debounce ~50ms. At current scale (REPL-style snippets), it's instant.

---

### 2026-04-30 — extend p9 in place with content-addressed types and named-type bindings

**Decision:** rather than fork to a new prototype, p9 gains:

- `Definition.t = Term(Node.t) | Type(Ty.t)` — types are first-class definitions in the same Store, disambiguated from terms by `Ty.hash`'s leading `'T'` byte vs. terms' `'P'`.
- `Node.Lam(Hash.t, Hash.t)` — the type-annotation slot becomes a content-addressed reference to a `Definition.Type`. Encoder writes the type's hash bytes; structurally-equal types share a hash, and the namespace can give them names.
- `Surface_ty.t` — a parser-side type AST with `Named(string)` and `Hole` that the Resolver expands to `Ty.t` (kind-checked against the namespace).
- `Attachment.Type_of(Hash.t)` and `Type_with_holes(Hash.t)` — typecheck-aspect values reference types by hash; readers reconstruct the `Ty.t` via `Store.lookup_type`.
- A second parser entry point (`main_ty`) and a generic recovery driver that services both term and type panes.
- A UI mode toggle (term ↔ type) on a single editor pane, each with its own buffer and bind-as input.

**Why:** the `docs/design/03-content-addressing.md` thread "Hashing types as well as terms" reads as bigger than it is — p6 already hashes types (for procedure-id encoding), `Ty.canonicalize` is identity for the monomorphic types in p9, and the namespace already binds opaque strings to hashes. The minimal substrate change is a sum, a hash redirection in `Node.Lam`, and a kind-checking pass in the resolver. Doing it inside p9 lets the existing UI affordances (browser, detail, recovered-AST panel) carry forward and demonstrates the type-aliasing identity end-to-end without a fresh prototype.

**Considered alternatives:**

- *Fork to p10.* The "fresh tree per prototype" CLAUDE.md convention. Rejected because p9's holes/typecheck/UI machinery is what gives the alias demo its surface; reproducing it would be churn.
- *Keep `Type_of(Ty.t)` inline.* Smaller diff, but loses the "the substrate uniformly content-addresses things named in the namespace" property that the thread is testing. We chose to migrate at the same time so the prototype actually answers the question.
- *Introduce a dedicated `type Vector = ...` declaration form in the term parser.* The single-pane mode toggle keeps the term grammar unchanged; the type editor is its own start symbol. If users find the toggle awkward, an inline declaration form is a strict UX layer on top with no substrate consequence.

**Implications:**

- **Hash break.** Every existing Lam hash from prior p9 sessions is invalidated — the encoder now writes the type's hash rather than its inline bytes. Bootstrap reseeds; no migration story (consistent with the substrate's bootstrap-phase posture).
- **Kind-mismatch errors.** Term names in type position and type names in term position both raise `Resolver.Kind_mismatch`. The browser shows a `T` badge on type leaves so the distinction is visible.
- **Aspect store dedup.** Two terms with the same type now share a `Type_of` aspect-value (the type-hash). Reverse queries by type are unchanged externally but cheaper internally because the aspect-value space collapses.
- **Bootstrap extension.** `alias.IntPair`, `alias.IntEndo`, `alias.BinOp` seed the namespace; `math.apply = \f: IntEndo. \x: Int. f x` exercises the named-alias resolution end-to-end.
- **Hole-in-type-position behavior unchanged.** `Surface_ty.Hole` resolves to `Ty.Int` as it did pre-types. Promotion to a real `Ty.Unknown` remains a separate open question.

---

### 2026-04-29 — `_opam` symlinks to p7's nested switch (not p3's)

**Decision:** `prototypes/p9-typed-namespaces/_opam → ../p7-web-interface/_opam/_opam`.

**Why:** p9 needs Bonsai/Core/Virtual_dom/js_of_ocaml. p3-p6's switch only has digestif, menhir, alcotest, qcheck. p7's switch is the only one with the web stack. Note that p7's `_opam` is itself a directory containing a nested `_opam/` subdirectory (the actual switch); the symlink targets the inner one to land where opam expects to find `.opam-switch/`.

**Implications:** any change to p7's switch (package updates, compiler bumps) will affect p9 too. That's acceptable for now since both prototypes are pinned to the same Bonsai/Core stack.
