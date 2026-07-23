# p17 — Translucent modules

Forks **p16** (records) and fuses its expression-level records with the
p14/p15 existential line into **first-class translucent modules**
(Harper–Lillibridge / Leroy, per `docs/design/12-type-abstraction.md`'s
"signature = binder + label-record"):

- **`Sig`** — a first-class type node: a labeled bundle of **opaque** type
  components (`type t`), **manifest** ones (`type u = Int`), and value
  components. Replaces nested unary `Exists`.
- **`struct { … }`** — the implementation: type members + term members in one
  value; its type is a fully-manifest sig.
- **`M :> S`** — ascription, the new `pack`: purely structural (no mint; two
  identical ascriptions share a hash), with **width subtyping** (impl members
  not named in `S` are private) and exact matching elsewhere.
- **n-ary `open`** — ONE `Open{pkg, mints}` node mints one fresh witness-less
  `Abstract` per opaque component; every binding's name is recovered from the
  sig's own labels (`N.t`, `N`, `N.empty`, …). Manifest members bind to their
  equations — the translucency payoff.
- **local `open e as M in body`** — the p15 open-question answered: a term
  form, **scoped like p14's unpack** (k de Bruijn type vars over the body,
  occurs-check avoidance, NO mint, so alpha-equivalence survives). `M#f`,
  `M.f`, and `M.t`-annotations all resolve in the body.

```
CounterSig = sig { type t, empty: t, incr: t -> t, get: t -> Int }
mk_counter : Bool -> CounterSig            -- two struct branches, two witnesses, ONE sig
:open mk_counter true as Box               -- binds Box.t (fresh Abstract) + Box.empty/incr/get
total : CounterSig -> Int
      = \m: CounterSig. open m as c in c#get (c#incr (c#incr c#empty))
```

Dropped: `Exists`, `Pack`, `Unpack`, p15's unary `Open` (tags retired, not
reused). Kept **alongside**: p12's unbundled machinery — `Opaque{mint,witness}`,
`Seal`, the editing context's open set, `Store.unsealers` — the two abstraction
mechanisms coexist (sigs never unfold, so the open set is orthogonal).

Design and scope: `docs/prototypes/p17-translucent-modules/00-scope.md`.

## Status

Implemented end to end:

1. **`Tnode.Sig`** with the **rank rule**: only opaque components bind; the
   opaque at rank *i* in label-hash sort order is `TVar(i)`, so component order
   carries no information and `Sig` stays order-insensitive like `Record`
   (`Tnode.opaque_ranks` is the single source of truth).
2. **`Node.Struct` / `Ascribe` / `Open` (n-ary) / `Open_local`** — checker
   rules in `src/typecheck.re` (`retarget` moves payloads between telescopes);
   `src/open_module.re` replaces `open_existential.re` (no peeling).
3. **Surface syntax** — `sig { … }`, `struct { … }`, `e :> S`,
   `open e as M in body`; a sig declaration is a **mint site** for its
   component labels (struct members reuse them — ascription matches by label).
4. **Resolver `tctx`** — binder *types* are tracked through resolution so a
   local open knows its scrutinee's sig (side effect: `[| x |]` under a binder
   now infers its element type, which p16 rejected).
5. **Web + bootstrap** — `CounterSig`/`counter.impl` (with a private `raw`
   member — width), `mk_counter`/`Box`/`Box2` (generativity), `CalSig` (ONE sig
   hiding TWO types), `VecSig`/`Vec` (manifest `scalar` stays usable), `total`
   (local open). The "open as module" form previews every binding from the
   sig's labels — nothing to fill in beyond the module name. The namespace
   browser has a **definition-sort filter** (all / terms / types / labels) next
   to the substring filter, and the detail pane's name chips carry an **✕ that
   deletes the name** (`Namespace.unbind`; REPL `:unbind <name>`) — a
   namespace-only edit, the content-addressed definition stays.

Everything else — opaque types + seals + minimal sealing, System-F (`∀`),
records, lists, evaluator, REPL, Bonsai web, the `test` aspect — carries over
from p16. 18 substrate tests green (plus the in-browser bootstrap tests).

Substrate core (the `src/` library, `P17_substrate`):

- `src/hash.re` — BLAKE2B content hashes.
- `src/mint.re` — deterministic minted marks (counter-sourced, reproducible).
- `src/tnode.re` — `… | Opaque | TVar | Forall | Abstract | List | Record | Sig(sig_comp)`; `opaque_ranks`.
- `src/node.re` — `… | Seal | TyLam/TyApp | Struct | Ascribe | Open(n-ary) | Open_local`; de Bruijn.
- `src/definition.re` — `Term | Type | Label`; leading-byte disambiguation.
- `src/typecheck.re` — opacity-parameterized checker; sig telescopes (cutoff+k); `retarget`; ascription matching; avoidance.
- `src/open_module.re` — `inspect` (sig shape, fully labeled) + `open_package` (one n-ary Open + labeled projections).
- `src/resolver.re` — names → hashes; rank-rule sig resolution; `mtypes` (locally-manifest type names); `tctx` (binder types).
- `src/store.re` / `src/editing_context.re` — unchanged from p16 (kept).

Run the REPL:

```sh
eval $(opam env --switch=. --set-switch)
dune exec ./bin/repl.exe      # :help; try the CounterSig example above
```

Run the web app:

```sh
eval $(opam env --switch=. --set-switch)
scripts/build-web.sh          # builds public/p17.js (~27 MB)
open public/index.html        # no server; state resets on reload
```

Known limits (recorded in the scope doc):

- A witness-less opened `Abstract` can never be unfolded — new operations on an
  already-opened module require edit-struct → re-ascribe → re-open (a fresh,
  incompatible type). The unbundled `Opaque`/`Seal` path remains the
  extension-in-place story.
- A module-producing functor that opens its argument and *returns* a module
  mentioning the freshly-opened type is blocked by avoidance (sound: the
  witness varies at runtime).
- Pretty-printing a local-open body regenerates type-variable names (the sig's
  labels aren't recoverable without typing context).

## Build

```sh
eval $(opam env --switch=. --set-switch)   # shared switch (symlinked to p7's)
dune build && dune runtest
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. Tech stack: OCaml ≥ 5.2,
Reason, dune ≥ 3.17, Menhir 3.0, ppx_deriving, digestif (BLAKE2B), alcotest /
qcheck for the substrate; Bonsai / Virtual_dom / Core / js_of_ocaml (v0.17) for
the web layer only. The `src/` substrate library is wrapped (`P17_substrate`);
the REPL and tests `open P17_substrate`.
