# Phase 16 — Records / labeled modules

**Status:** Scope doc for the sixteenth prototype.
**Design context:** `../../design/11-label-sort.md` (labels as a first-class minted sort; records reference labels by hash; canonical by sorted label hash; records and modules are one shape), `../../design/12-type-abstraction.md` (abstract types, existential packages), `../../design/10-minted-identity.md` (mint for label distinctness), `../../design/03-content-addressing.md` §"Hashing types as well as terms" / field-order canonicalization, `../../design/04-naming-layer.md` (names resolve to hashes at edit time; names stay out of stored programs).
**Forks:** `p15-open-existentials` (verbatim, renamed `P16_substrate`).
**Prototype design lives here:** `docs/prototypes/p16-records/`.
**Implementation code lives at:** `prototypes/p16-records/`.

## Thesis

p15 opened existential packages into the namespace, but the package interface was a **positional `Product`** — `t * ((t -> t) * (t -> Int))`. That forced two compromises: the `open` gesture had to *guess* field names (the `providing empty, incr, get` list, or positional `fst`/`snd`), and the structural flattening over-splits a trailing product field. The recurring fix, flagged across p15, is **named fields**.

p16 folds the **label sort** (`11-label-sort.md`, first prototyped in p10) into the p12–p15 type-abstraction line: records reference labels by *hash*, field names live in the namespace, and an existential package's interface becomes a **record** instead of a positional product. The payoff lands on `open`:

```
mkCounter : Bool -> exists t. { empty: t, incr: t -> t, get: t -> Int }
```
`:open mkCounter true as Counter` now binds `Counter.empty` / `Counter.incr` / `Counter.get` with **names read from the record's labels** — no `providing`, no positional guessing, no over-split. The field names are recovered from the structure, not supplied by the user.

This is the "records as the real fix" follow-on. It does *not* reintroduce p10's mint-everything-by-default: terms and types stay structural (as in p12–p15); **only labels mint**. The lineage: p12 (abstract types) → p13 (System-F) → p14 (existentials) → p15 (open) → p16 (the interface gets names).

## Feature 1 — the label sort

- `Definition.t = Term(Node.t) | Type(Tnode.t) | Label(Label.t)` — labels join terms and types as a stored sort (the p9 / p10 maneuver a third time). A `Label.t` is essentially a mint mark; its canonical encoding is `tag-byte + mint` (same shape as a primitive's tag). Ported from p10's `Label` module.
- **Minted by default** (`11` §"Minted by default"): declaring a label draws a fresh mint, so two unrelated `x` fields stay distinct. The namespace binds human-readable names to label hashes (existing name → hash shape, no schema change).
- **Intentional sharing on demand:** a field name that *resolves* to an existing label (in scope) reuses that label hash; only an unresolved name mints. This is `11` §"Intentional label sharing" — the substrate hook for eventual row-typing / module-subtyping.

## Feature 2 — records (the general shape)

- **Type:** `Tnode.Record(list((label_hash, field_type_hash)))`, **canonical by sorted label hash** (`11` §"Field order" / `03`), so `{x,y}` and `{y,x}` hash equal and renames never perturb the canonical form.
- **Literal:** `Node.Record_lit(list((label_hash, value)))`, canonicalized the same way.
- **Projection:** `Node.Project_field(record, label_hash)`.
- Pretty-printing reverse-looks-up label hashes through the namespace (short-hash fallback), exactly as term/type refs already do.
- `Record_update` (`{ p with x = … }`) is **deferred** — not needed for the module story; add only if cheap.

## Feature 3 — modules as records (the headline)

A *module* is a record whose fields are the operations (`11` §"Records and modules: same shape"). Existential packages quantify over records:

```
exists t. { empty: t, incr: t -> t, get: t -> Int }
```
`pack`/`unpack` and `open` are unchanged in shape; only the bundled interface goes from `Product` to `Record`. `open`'s field extraction reads the record type's `(label, field_type)` pairs: for each, it builds a `Project_field(pkg, label)` node and binds it under the label's namespace name (`N.<label-name>`). **n-ary open** still peels nested `exists` (composing `Open` as in p15); the final record's labels name the operation fields. Abstract-type names (the `exists` binders) still have no label and are named by the user / defaulted to `t`, `u`, ….

The web "open as module" form keeps the per-abstract-type inputs but **pre-fills the field names from the labels** (read-only or editable) — the structure now carries the names, closing the loop the p15 form left to the user.

## Substrate additions (precise)

- `Definition.t` gains `Label(Label.t)`; new `src/label.re` (port p10). Definition leading byte for labels (p10 used a distinct kind byte).
- Types (`Tnode`): `Record(list((Hash.t, Hash.t)))` — next free tag byte after `List` (0x19) → 0x1a. Canonicalize field list by sorted label hash in the smart constructor / `mk_type`.
- Terms (`Node`): `Record_lit(list((Hash.t, Hash.t)))` and `Project_field(Hash.t, Hash.t)` — next free term tags. Record-lit canonical bytes sort by label hash.
- Typecheck: `Record` structural in `equal_ty` / `shift_ty` / `subst_ty` / `normalize_type` / `occurs_tvar` (recurse into field types; type vars can appear in field types, e.g. `{ get: t -> Int }`). `synth(Record_lit)` → `Record` of the field types; `synth(Project_field(r, l))` → look up `l` in `r`'s record type, error if absent.
- Eval: record literals evaluate fields to values (`VRecord`); projection selects by label hash.
- `open_existential.re`: `field_types` / `inspect` / `open_package` learn the `Record` case — flatten by labels instead of the right-nested product spine, and return label hashes alongside field hashes so the caller binds by label name.

## Surface syntax (sketch; finalize at implementation)

- Record **type**: `{ x: Int, y: Int }`.
- Record **literal**: `{ x = e1, y = e2 }`.
- **Projection:** `p#x` — a distinct `#` operator (decision 2), avoiding the collision with p15's greedy dotted IDENTs (`Counter.empty` is one token). `x` resolves through the namespace to a label hash.
- Label **declaration / minting:** a record-**type** declaration is the mint site (decision 1).

## Design decisions (confirmed)

1. **Labels mint at the record-type declaration.** `type Point = { x: Int, y: Int }` mints any field labels not already resolvable in scope and binds them; field names that already resolve reuse the existing label (intentional sharing). The package interface is written once, and that's the mint site — least ceremony for the module story. (`11` pushes fancier auto-mint affordances to the editor; this is the substrate gesture under them.)
2. **Projection is a distinct operator `p#x`.** Avoids the dotted-name collision (p15 lexes `Counter.empty` as one IDENT). `#` is a new token; `p#x` is postfix on an atom; `x` resolves to a label hash → `Project_field(p, x)`. Surface projection ships in p16 (not deferred).
3. **`Record_update` deferred** — not needed for the module story; add only if cheap.

## Build order

1. **Fork** p15 → p16 (`cp -R`, clean `_build`/bundle/`_opam`, sed `p15→p16` / `P15_substrate→P16_substrate` / `p15-open-existentials→p16-records`, rename `test_p15`→`test_p16`, README). Green baseline.
2. **Label sort + record substrate:** port `Label`, add `Definition.Label`, `Tnode.Record`, `Node.Record_lit`/`Project_field` with sorted-label canonicalization → build → programmatic test (a `{x,y}` record literal type-checks, projects, and `{x,y}` = `{y,x}` by hash).
3. **Existentials over records + open:** teach `open_existential` the `Record` interface; `open` binds fields by label name. REPL: `mkCounter` with a record interface, opened with auto-named fields.
4. **Surface + resolver:** record type / literal syntax; label minting per decision 1.
5. **Web + bootstrap:** reseed `mkCounter` / `mkScale` / `mkCalendar` with record interfaces; the open form pre-fills field names from labels.
6. Commit per stage.

## Test plan

- Label distinctness: two `type … { x: … }` in unrelated places mint distinct `x` labels (distinct hashes); a shared-label gesture reuses one.
- Record canonicalization: `{ x: Int, y: Bool }` = `{ y: Bool, x: Int }` by hash; record literals likewise.
- Projection: `synth(Project_field(r, x))` = the field type; projecting an absent label is rejected.
- Module open: `open` a record-interfaced existential binds `N.<label>` for each field, names recovered from labels (no `providing`); round-trip a counter.
- n-ary + records: `mkCalendar`-style two-abstract-type module with a record interface opens to `Cal.date` / `Cal.span` + named ops; the date/span safety property still holds.

## Out of scope (deferred)

- **`Record_update`** (`{ p with x = … }`), **variants/constructors** as labels (`11` §"Variants" — same sort, later), **row polymorphism / module subtyping** (the label-sharing hook is in place; the type-language feature is not), and **mint-everything-by-default** (labels mint; terms/types stay structural). Surface projection `p#x` *is* in scope (decision 2).
