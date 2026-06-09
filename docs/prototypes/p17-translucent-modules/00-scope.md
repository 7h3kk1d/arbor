# p17 — Translucent modules: scope

**Thesis.** Fuse p16's expression-level records with the p14/p15 existential
line into **first-class translucent modules** (Harper–Lillibridge / Leroy; the
"signature = binder + label-record" leaning of `design/12-type-abstraction.md`,
now exercised): a `Sig` type node whose labeled components are opaque type
members, manifest (transparent) type members, and value members; structs as
the implementing terms; ascription `M :> S` as the sealing gesture (the new
pack); a single **n-ary** top-level open; and a **scoped local open** that
answers p15's open question ("open is top-level only").

Forks `p16-records`. Code: `prototypes/p17-translucent-modules/`.

## Substrate deltas

Removed (tags retired, not reused): `Tnode.Exists` (0x17), `Node.Pack` (0x0f),
`Node.Unpack` (0x10), p15's unary `Node.Open` (0x11), and the `pack` /
`unpack` / `exists` surface forms.

Kept **alongside, unchanged** (decided 2026-06-09): p12's unbundled machinery —
`Opaque{mint, witness}`, `Seal`, the editing context's open set,
`Store.unsealers`, the checker's `opens` parameter. The two abstraction
mechanisms coexist; sigs never unfold, so the open set is orthogonal to all of
the new rules.

Added:

- **`Tnode.Sig(list((label, sig_comp)))`** (tag 0x1b), `sig_comp = Sopaque |
  Smanifest(ty) | Sval(ty)`.
- **`Node.Struct`** (0x17, members `Mtype(ty-hash) | Mval(term)`),
  **`Node.Ascribe{impl, sg}`** (0x18), **`Node.Open{pkg, mints}`** (0x19,
  n-ary, mints in the hashed bytes), **`Node.Open_local{k, scrut, body}`**
  (0x1a, no mint).
- `src/open_module.re` (replaces `open_existential.re` — no peeling; the shape
  is read off the sig, fully labeled).

## The rank rule (the load-bearing representation decision)

Only `Sopaque` components bind. With k opaques, the opaque at **rank i in
label-hash sort order is `TVar(i)`** inside every manifest/value payload; all
payloads sit under all k binders at once (enclosing `Forall` vars at indices
≥ k). Because the index is the *sorted rank*, not the declaration position,
component order carries no information — the encoder canonicalizes by sorting
(opaques, manifests, values; each group by label hash) without touching any
payload, and `Sig` stays order-insensitive exactly like `Record`.
`Tnode.opaque_ranks` is the single source of truth; resolver, checker,
open-module, and pretty all derive the rank order from it (tested: the same
two-opaque sig declared in both textual orders hashes identically).

Manifest components do **not** bind: surface references to them are inlined at
resolution, but the component is retained in the node (it is part of the sig's
identity, what ascription's "manifest must match exactly" checks, and what
top-level open binds as `N.u`). Manifests may reference opaque components.

## Ascription (`M :> S`) — the new pack

A term node, checked by: witness each opaque component of S with the impl's
type member (manifest equation, or the impl's own opaque for re-ascription);
manifest components match exactly; value component types match exactly after
the witnesses are substituted (`retarget`, a parallel telescope-to-telescope
substitution); impl members not named in S are ignored (**width subtyping** =
private members; no depth subtyping). The result type is the sig itself.

**No mint.** Ascription is purely structural — two identical ascriptions share
a hash, which is what lets a factory's two branches (different witnesses)
unify at the one sig type, exactly as p14's `pack` did. Distinctness enters
only at *open*. Forgetting (re-ascribing an already-sealed module to a
narrower or more-opaque sig) falls out of the same rule and is tested.

## Open, top-level (n-ary, generative)

`Open{pkg, mints}` requires one mint per opaque component and types as the sig
with every opaque made manifest at a fresh witness-less `Abstract(mint)` (and
every other payload retargeted). One node opens a module hiding any number of
types — p15's nested-unary-`Open` composition is gone. The editor gesture
(`web/ops.ml`, REPL `:open e as N`) binds `N.t` per opaque, `N.u` per manifest
(**to its equation** — the translucency payoff), `N`, and `N.f` per value
member, every name recovered from the sig's own labels: nothing to fill in.
Mints are in the hashed bytes ⇒ re-opening mints fresh, incompatible types
(generative, as p15).

## Local open (the p15 question, answered)

`open e as M in body` is a term form, **scoped like p14's unpack, not
generative** (decided 2026-06-09): the k opaque components become k de Bruijn
type variables over the body, the module value is one term binder, avoidance
is an occurs-check (the body's result type may not mention the hidden types),
and **no mint is drawn** — α-equivalent re-authorings share a hash, preserving
the substrate's structural identity. Rationale for no-mint: a hidden type that
escaped scope would otherwise exist with no global name to refer to it by;
and consuming a package is elimination, not an abstraction-authoring gesture.
Inside the body, `M#f`, `M.f` (sugar), and `M.t`-in-annotations all resolve;
the binder type is the sig with opaques manifest at the body's own TVars
(indices align verbatim, by the rank rule).

Resolution cost: the resolver now threads `tctx` (the term binders' types) so
a local open can read its scrutinee's sig at resolve time. Pleasant side
effect: `[| x |]` under a binder now infers its element type (p16 rejected
it).

## Findings

- **n-ary-in-one-node is strictly simpler than peeling.** p15's
  strip/flatten/peel logic (~110 lines) became a direct read of the sig shape;
  the labels make every binding pre-named, types included.
- **The rank rule reconciles binding with order-insensitivity.** The feared
  tension between "telescopes are order-significant" and "records sort by
  label hash" dissolves once the binder index is itself derived from the sort.
- **Ascription-as-pack needs no mint anywhere.** All generativity lives in
  open; sealing is hash-shared and re-runnable. This cleanly splits p14's pack
  (structural) from p15's open (generative) along the authoring/computation
  axis of design/12.
- **Width at ascription subsumes `private`** exactly as design/12 predicted:
  the unascribed struct binding stays around for internal tests; the sealed
  binding simply lacks the member.

## Limits / open questions

- **Module-producing functors are blocked by avoidance.** `\m: S. open m as c
  in (struct { … c.t … }) :> S'` cannot return anything mentioning `c.t`. This
  is *sound* here (the witness varies at runtime with the argument), but it
  means "a functor that opens its argument and returns a module over the
  freshly-opened type" — the second half of p15's open question — remains
  inexpressible. A relaxation worth exploring: when the scrutinee is a closed
  (binder-independent) expression, escape would be sound; that would let local
  open subsume the top-level gesture entirely.
- **Opened types are frozen.** A witness-less `Abstract` never unfolds, so no
  new operation can be authored against an already-opened module's type; the
  only extension path through the module system is edit-struct → re-ascribe →
  re-open = a fresh, incompatible type (an all-or-nothing version bump). The
  kept `Opaque`/`Seal` path remains the extension-in-place story; whether the
  module path should acquire one (witness-carrying abstracts? a "re-open with
  witness" gesture?) is open.
- **Sig vs Record.** Structs evaluate to the same `VRecord` values as records,
  and a 0-opaque sig is nearly a record with type members. Whether the two
  should unify (records as the degenerate sig) is open; p17 kept them separate
  (data vs module) to keep record equality exact and sig matching width-aware
  at ascription only.
- **Pretty round-trip.** A local-open body re-renders with generated type-var
  names (`t`, `u`, …) rather than the sig's labels — the printer has no typing
  context. Same class of limitation as p16's `<sealed: …>`.
- **First-class sig values and functors over sigs** work via System-F (`\m:
  CounterSig. …`, the seeded `total`), but quantifying over a *signature*
  (F-omega territory) is untouched.
