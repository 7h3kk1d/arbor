# p17 — Translucent modules

Forks p16 and combines its records with the p14/p15 existential line into first-class translucent modules (Harper–Lillibridge, Leroy). A signature is a binder plus a label record, as in `docs/design/12-type-abstraction.md`.

- **`Sig`** is a type node: a labeled bundle of opaque type components (`type t`), manifest ones (`type u = Int`) and value components. It replaces nested unary `Exists`.
- **`struct { … }`** holds type members and term members in one value. Its type is a fully manifest sig.
- **`M :> S`** (ascription) replaces `pack`. It is structural and mints nothing, so two identical ascriptions share a hash. Impl members not named in `S` become private (width subtyping); everything else must match exactly.
- **N-ary `open`** is one `Open{pkg, mints}` node that mints a fresh witness-less `Abstract` per opaque component. Every binding is named from the sig's labels (`N.t`, `N`, `N.empty`, …). Manifest members bind to their equations, which is what makes the module translucent.
- **Local `open e as M in body`** is a term form, scoped like p14's `unpack`: k de Bruijn type variables over the body, occurs-check avoidance, and no mint, so α-equivalence holds. `M#f`, `M.f` and `M.t` annotations resolve in the body. This answers an open question from p15.

```
CounterSig = sig { type t, empty: t, incr: t -> t, get: t -> Int }
mk_counter : Bool -> CounterSig            -- two struct branches, two witnesses, one sig
:open mk_counter true as Box               -- binds Box.t (fresh Abstract) + Box.empty/incr/get
total : CounterSig -> Int
      = \m: CounterSig. open m as c in c#get (c#incr (c#incr c#empty))
```

Removed: `Exists`, `Pack`, `Unpack` and p15's unary `Open`. Their tags are retired, not reused. Kept: p12's `Opaque{mint, witness}`, `Seal`, the editing context's open set and `Store.unsealers`. The two mechanisms coexist because sigs never unfold, so the open set does not affect them.

Design and scope: `docs/prototypes/p17-translucent-modules/00-scope.md`. For a guided demo, see `WALKTHROUGH.md`.

## Status

Complete. `dune runtest` passes 19 tests; in-browser bootstrap tests run on load.

1. **`Tnode.Sig` and the rank rule.** Only opaque components bind. The opaque at rank *i* in label-hash order is `TVar(i)`, so component order carries no information and `Sig` is order-insensitive like `Record`. `Tnode.opaque_ranks` is the single source of truth.
2. **`Node.Struct`, `Ascribe`, n-ary `Open`, `Open_local`.** Checker rules are in `src/typecheck.re`; `retarget` moves payloads between telescopes. `src/open_module.re` replaces `open_existential.re` and does no peeling.
3. **Surface syntax:** `sig { … }`, `struct { … }`, `e :> S`, `open e as M in body`. A sig declaration mints its component labels; struct members reuse them, since ascription matches by label.
4. **Resolver `tctx`.** Binder types are tracked during resolution so a local open knows its scrutinee's sig. As a side effect, `[| x |]` under a binder now infers its element type, which p16 rejected.
5. **Web and bootstrap.** Seeded examples: `CounterSig`/`counter.impl` (with a private `raw` member), `mk_counter` → `Box` and `Box2` (generativity), `CalSig` (one sig hiding two types), `VecSig`/`Vec` (the manifest `scalar` stays `Int`), and `total` (local open). The "open as module" form previews every binding from the sig's labels; only the module name is needed. The namespace browser has a sort filter (all / terms / types / labels) beside the substring filter. Name chips in the detail pane have an ✕ that unbinds the name (`Namespace.unbind`; REPL `:unbind <name>`). The definition stays in the store.

Opaque types, seals and minimal sealing, System-F, records, lists, the evaluator, REPL and the `test` aspect carry over from p16.

## Substrate (`src/`, library `P17_substrate`)

- `hash.re` — BLAKE2B content hashes.
- `mint.re` — deterministic minted marks, counter-sourced.
- `tnode.re` — `… | Opaque | TVar | Forall | Abstract | List | Record | Sig(sig_comp)`; `opaque_ranks`.
- `node.re` — `… | Seal | TyLam/TyApp | Struct | Ascribe | Open (n-ary) | Open_local`; de Bruijn.
- `definition.re` — `Term | Type | Label`, told apart by leading byte.
- `typecheck.re` — the opacity-parameterized checker; sig telescopes (cutoff + k); `retarget`; ascription matching; avoidance.
- `open_module.re` — `inspect` (the sig's shape, fully labeled) and `open_package` (one n-ary `Open` plus labeled projections).
- `resolver.re` — name resolution; rank-rule sig resolution; `mtypes` (type names manifest inside a struct); `tctx` (binder types).
- `store.re`, `editing_context.re` — unchanged from p16.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
dune exec ./bin/repl.exe      # :help; try the CounterSig example above
scripts/build-web.sh          # builds public/p17.js (~27 MB)
open public/index.html        # no server; state resets on reload
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Stack: OCaml ≥ 5.2, Reason, dune ≥ 3.17, Menhir 3.0, ppx_deriving, digestif (BLAKE2B), alcotest, qcheck; Bonsai, Virtual_dom, Core and js_of_ocaml (v0.17) for the web layer only.

## Known limits

Recorded in the scope doc.

- An opened `Abstract` has no witness and never unfolds. Adding an operation to an opened module means editing the struct, re-ascribing and re-opening, which yields a new, incompatible type. Extension in place stays on the `Opaque`/`Seal` path.
- A functor that opens its argument and returns a module mentioning the opened type is blocked by avoidance. This is sound: the witness varies at runtime.
- Pretty-printing a local-open body regenerates type-variable names, because the sig's labels are not recoverable without typing context.
