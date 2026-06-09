# Phase 15 — Open existentials, namespace type extraction, lists

**Status:** Plan / scope doc for the fifteenth prototype (not yet built).
**Design context:** `../../design/12-type-abstraction.md` (open-vs-closed existential; module↔namespace threads), `../../design/01-language-model.md` (existentials / first-class modules).
**Forks:** `p14-existentials` (verbatim, rename `P15_substrate`).
**Prototype design lives here:** `docs/prototypes/p15-open-existentials/`.
**Implementation code lives at:** `prototypes/p15-open-existentials/` (to be created).

## Thesis

p14 added closed existentials (`unpack [t] x = e in body`, single expression scope). p15 adds the **open** form as a primitive editor gesture: opening an existential package binds *both* the package value and its hidden representation type **into the namespace**, so later, independent top-level definitions can use them. Plus **lists**, so collections of packages are expressible.

This is exactly OCaml's `module Counter = (val e) in <rest>`: `Counter` is the value, `Counter.t` the abstract type, scoped over the subsequent definitions, with the avoidance rule at the boundary (the final result can't mention `Counter.t`). Same limitation as OCaml/Haskell — a witness type can't escape its scope or be minted per-element from a runtime list (existentials aren't dependent types). What *does* work, matching OCaml: mapping the closed expression `unpack` over a list to observe each element at a concrete type.

## Feature 1 — primitive `open` (the headline)

The editor gesture, as primitive as possible — it binds exactly **two** things:

```
:open mkCounter true as Counter
```
→
- **`Counter.t`** = a fresh abstract type standing for the hidden witness (never unfoldable — the witness is genuinely gone), and
- **`Counter`** = the package value, at type `Counter.t * ((Counter.t -> Counter.t) * (Counter.t -> Int))`.

Use is positional: `fst Counter : Counter.t`, `fst (snd Counter) : Counter.t -> Counter.t`, … and pretty-printing shows `Counter.t` because the abstract type is named. Subsequent top-level defs freely reference `Counter.t` and `Counter`.

**Mechanism (small):**
- `Tnode.Abstract(Mint.t)` — a minted abstract type *constant* (Skolem): no witness, never unfolds, content-addressed by its mint. (Distinct from p12's `Opaque{mint, witness}`, which has a witness openable in an editing context.)
- `Node.Open{pkg, mint}` — generative open. `synth`: `pkg : ∃t. T` ⇒ result `T[t := Abstract(mint)]`. `eval`: `eval(pkg)` (witness erased; pack already erases).
- Editor gesture `:open <e> as N`: typecheck `e : ∃t.T`; mint `m`; ingest `at = Abstract(m)`, bind `N.t → at`; ingest `Open{pkg=<resolved e>, mint=m}` (type `T[at]`), bind `N → it`. One mint shared by both, so `N : T[N.t]`.

Generative: re-opening mints a fresh `Abstract` (a new type), as existentials require.

**Optional sugar (not load-bearing):** `:open <e> as N providing f1, f2, …` additionally binds `N.f1 → fst N`, `N.f2 → fst (snd N)`, … (positional projections). This is really a general tuple-destructure gesture; add only if cheap.

## Feature 2 — lists

- `Tnode.List(elem)`.
- `Node.Nil(elem_ty)` : `List(E)`; `Node.Cons(h, t)` : `h:E`, `t:List(E)` ⇒ `List(E)`; `Node.Fold(lst, z, f)` : `lst:List(E)`, `z:Acc`, `f:E -> Acc -> Acc` ⇒ `Acc` (right fold). No general recursion in the language, so `Fold` is the bounded-iteration primitive (`map`/`length`/`sum` derive from it).
- Eval values: `VNil`, `VCons(v, v)`; `Fold` folds `f` over the list.

The map-and-observe program (matches OCaml) then is:
```
fold counters (nil [Int]) (\pkg: (exists t. ...). \acc: List Int.
  cons (unpack [t] c = pkg in snd (snd c) ((fst (snd c)) ((fst (snd c)) (fst c)))) acc)
```
→ `List Int` — open each package (closed `unpack`, concrete result), collect observations.

## Substrate additions (precise)

Types (`Tnode`): `Abstract(Mint.t)` (tag 0x18), `List(Hash.t)` (tag 0x19).
Terms (`Node`): `Open{pkg, mint}` (0x11), `Nil(Hash.t)` (0x12), `Cons(t,t)` (0x13), `Fold(t,t,t)` (0x14).
Typecheck:
- `Abstract`: rigid in `whnf`/`equal_ty` (hash equality); closed in `shift_ty`/`subst_ty`/`normalize_type`; `occurs_tvar` false.
- `List(E)`: structural in `equal_ty`/`shift_ty`/`subst_ty`/`normalize_type`/`occurs_tvar` (recurse into `E`).
- `Open`: as above. `Nil`/`Cons`/`Fold`: as above.
Eval: `Open → eval pkg`; `Nil → VNil`; `Cons → VCons`; `Fold → fold`. Add `VNil`/`VCons` to the value type.
Pretty: `Abstract` → name or `abstract(m_xxxx)`; `List E` → `List <E>`; `nil [E]` / `cons h t` / `fold l z f`; `Open` → `open <pkg>` (rarely shown — usually behind a name).
Every `Tnode`/`Node` match site must add the new constructors (encode, whnf/equal_ty/shift/subst/normalize/occurs, synth, normalize_term, eval, pretty).

## Surface syntax (sketch; exact tokens at implementation)

- `:open <e> as N` — REPL/editor command (and a web affordance). Not a term form; it mutates the namespace.
- List type `List T`; `nil [T]`, `cons h t`, `fold l z f` (keywords `nil`/`cons`/`fold`/`List`). Note `[ ]` already lexes for type application — `nil [T]` reuses it as a dedicated production.
- List *literals* `[| e1, …, en |]` (added after the initial scope): a distinct `[| |]` delimiter avoids the reduce/reduce clash with type application `e [T]` (which `[a, b]` would have triggered on the `f [x]` / `f [Counter.t]` cases). Pure surface sugar — resolves to the same canonical `cons`-spine as the explicit form (identical hash), and the pretty-printer round-trips any `cons`-spine ending in `nil` back to `[| … |]`. The trailing `nil` needs a concrete element type for content-addressing, so the resolver **synthesizes the element type from the head element** and pins it. This is the one inference in the otherwise-explicit language: it works when the head's type is knowable in the empty context (literals, refs, primitives over them), and degrades to a clear error ("annotate or use cons/nil") when the head is a locally-bound variable (`\x: Int. [| x |]`). Empty lists have no head, so they stay `nil [T]`.
- Existential / closed unpack / pack already exist from p14.

## Build order

1. **Fork** p14 → p15 (`cp -R`, clean `_build`/bundle/opam, sed `p14→p15` / `P14_substrate→P15_substrate` / `p14-existentials→p15-open-existentials`, rename `test_p14`→`test_p15`, README). Build green baseline.
2. **Substrate:** `Abstract` + `Open` (the headline) → build → programmatic test (open the factory, check `Counter.t` distinct, `fst Counter` etc. type/eval). Then `List` + `Nil`/`Cons`/`Fold` → build → test (sum/map).
3. **Surface + resolver + REPL:** `:open … as N` gesture (mints, binds `N.t` + `N`); list keywords. Verify in REPL (open mkCounter, use `Counter.t`/`Counter`; fold-map-observe a list of packages).
4. **Web + bootstrap:** seed an opened counter and a small list demo; scratch/define accept the new syntax.
5. Commit per stage as p13/p14 did.

## Test plan

- Open the p14 factory: `:open mkCounter true as C` → `C.t` is a fresh abstract type, `C : C.t * …`; `C.get (C.incr (C.incr (fst-ish C.empty)))` (positional) evaluates to 2; a second `:open mkCounter false as D` gives `D.t ≠ C.t`.
- Avoidance still holds (closed `unpack` body returning `t` is rejected).
- Lists: `fold` to sum `[1,2,3]` = 6; map-and-observe a list of packages → `List Int` all 2.
- Abstract-type distinctness: two opens mint distinct `Abstract`s.

## Out of scope (deferred)

- **Records / named fields** (port of p10's label-sort). Open binds the tuple positionally; the `providing …` sugar gives names without records. Records are a clean follow-on (p16?).
- **Per-element top-level open over a runtime list** — minting a top-level abstract type per list element. Not expressible (avoidance / no dependent types) — same as OCaml.
- **General recursion** — only `Fold` (bounded). No `fix`.
- **Open existentials in the Montagu–Rémy sense** (witness escaping via cleverer avoidance). Closed `unpack` + top-level generative `open` only.

## Decisions to record (in decisions.md when forked)

- `open` is a primitive editor gesture binding `N.t` + `N` (the tuple); field naming is optional sugar.
- The extracted type is a minted `Abstract` constant (no witness, never unfolds) — generative per open.
- Interface stays a positional `Product`; records deferred.
- `Fold` is the list eliminator (no general recursion).

## n-ary open — a module hiding several abstract types (added 2026-06-05)

A module with *two* abstract types is two existential quantifiers: `exists c. exists k. T` (a "functor returning a module with two abstract types" — e.g. a temperature module hiding both `celsius` and `kelvin`, with conversions between them). The substrate already represents this by nesting; **no substrate change** was needed.

`open` was generalized to peel *every* leading `exists`, minting one fresh `Abstract` per level. The key observation: the unary `Open` node composes — `Open{Open{pkg, m1}, m2}` type-checks to `T[c:=m1][k:=m2]` — so n-ary open is iteration over the existing node, not a new construct. `src/open_existential.re` houses:

- `inspect(node) : option((arity, field_type_hashes))` — counts leading `exists` and flattens the right-nested operations product into its leaf types (with the bound vars left as `TVar`s), for driving a form. None if not an existential.
- `open_package(node) : opened` — mints the abstracts (outer-first), nests the `Open` nodes, and projects the operations product positionally; returns `{type_hashes, module_hash, field_hashes}`, name-free (the interface layer names them).

Naming: the REPL gesture is `:open <e> as N [t1, t2] providing f1, f2, …` (the `[…]` names the abstract types positionally; absent → defaults to the tyvar names `t`, `u`, `s`, …). The web populates **one input per abstract type and one per operation field, derived from the structure** (each field labeled with its type) rather than a comma-separated string — `inspect`'s shape rides on the live `Typed` feedback.

Field count comes from flattening the *right-nested* operations product, matching how positional projection works. Limitation (shared with the positional-Product choice): a field whose value is itself a pair is over-split into two fields — the records/labels follow-on (p16?) is the real fix.
