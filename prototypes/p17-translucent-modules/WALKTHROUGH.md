# p17 — a guided demo of type abstraction

This is a **script for demonstrating the prototype** — follow it top to bottom
(~15 minutes). Each act says *why the feature exists* (the bug it prevents or the
thing it makes possible), then the *exact steps* to run, then *what to point at*.

The one idea behind everything here: **a type can hide how it's represented, and
the type system enforces that hiding.** The acts build up from "hide one type's
representation" to "hand someone a whole module whose representation they can
never see." It's all on a content-addressed substrate, so types and even
record-field names are content, not strings baked into your programs.

## Run it

```sh
eval $(opam env --switch=. --set-switch)
scripts/build-web.sh        # builds public/p17.js
open public/index.html      # three panes, no server, state resets on reload
```

Panes: **browser** (left — the namespace tree; click a name to inspect it, click
a module's ▸ to expand), **editor** (center — a *work area* that type-checks and
evaluates whatever you type, live), **detail** (right — the selected definition's
type, source, and value). You author by typing in the work area; nothing is saved
until you click **bind**.

Two kinds of hidden type appear, with distinct badges: **opaque** (the
representation exists but is hidden from consumers — `Counter.t`; it carries an
`edit` toggle, because *you* can reveal it to author against it) and **abstract**
(no representation at all — what you get from opening a module, e.g. `Box.t`;
there's nothing to reveal). Signatures get a **sig** badge.

> Skipped in this tour (present in the namespace, not part of the story): the
> `*.Tests.*` bindings + the pass/fail tally, and `demo.nums`/lists.

---

## Act 1 — Hide the representation (`Counter`)

**Why.** A "counter" is just an `Int` underneath. If you expose it *as* `Int`,
nothing stops a caller from writing `c * 2`, or stuffing in `-7`, or passing any
old number where a counter belongs. An **opaque type** makes the representation a
secret: callers get only the operations you publish, and the type checker holds
them to it.

**Do.**
1. In the work area, type:
   ```
   Counter.get (Counter.incr (Counter.incr Counter.empty))
   ```
   → live feedback `: Int` and `= 2`. This is the *published API* doing its job.
2. Now try to cheat — type:
   ```
   Counter.empty + 1
   ```
   → `error: type error …`. `Counter.empty` is a `Counter.t`, and `+` wants an
   `Int`. The `Int` representation is **not** reachable from out here.

**Point at.** Expand `Counter` in the browser and click `Counter.t`: its badge is
**opaque**, and the detail pane says *opaque type — opaque(…)* with its
**representation hidden from consumers**. The takeaway: the only way to get or
change a counter is through `empty` / `incr` / `get` — exactly the invariant you
wanted.

---

## Act 2 — But someone has to write `incr` (the editing context + sealing)

**Why.** `incr` obviously *does* need to touch the `Int` (`x + 1`). So who's
allowed? The answer is an **editing context**: you explicitly "open" the opaque
type's representation while authoring, and any definition that actually used it is
recorded as **sealed**. Definitions that only compose the public API are *not*
sealed. This is opacity enforced by the editor, not bundled into the type — and
the substrate can tell you, after the fact, exactly which definitions saw the
secret.

**Do.**
1. In the browser, find `Counter.t` and click its **edit** toggle (it lights up —
   the representation is now open *for you, while you author*).
2. Back in the work area, type `Counter.empty + 1` again → it now type-checks and
   shows `= 1` (with the type open, `Counter.t` *is* `Int`).
3. Put `peek` in the **name** box and click **bind**. The result badge reads
   **SEALED** — this definition needed the secret, so it's marked.
4. Click `Counter.t`'s **edit** toggle off. Type `Counter.empty + 1` once more →
   it's a type error again.

**Point at.** Click `Counter.t` in the browser: the detail pane lists its
**unsealing definitions** — `empty`, `incr`, `get`, `decr`, and now your `peek`.
Notice `bump2` is *not* in that list: `bump2 = \c. incr (incr c)` only uses the
public API, so it stayed an ordinary term. That list is *derived* by scanning, not
stored. Takeaway: "who can see the representation" is a first-class, auditable
question.

---

## Act 3 — Same bits, different type (`Celsius` vs `Kelvin`)

**Why.** This is the famous-bug act. Celsius and Kelvin are *both* "an `Int`
number of degrees" — identical representations — but confusing them crashed a Mars
orbiter. Two **opaque** types over the **same** representation are still
**distinct** (each was minted separately), so the checker refuses to let you mix
them. The only bridge is an explicit conversion.

**Do.**
1. Type the unit bug:
   ```
   Kelvin.value Celsius.freezing
   ```
   → `error`. `Kelvin.value` wants a `Kelvin.t`; `Celsius.freezing` is a
   `Celsius.t`. Same `Int` underneath, but the types keep them apart.
2. Go through the proper conversion:
   ```
   Kelvin.value (Temp.c_to_k Celsius.freezing)
   ```
   → `= 273`.

**Point at.** `Temp.c_to_k`'s type is `Celsius.t -> Kelvin.t` (select it in the
browser). Conversions are the *only* way across, and they're explicit. Takeaway:
"distinct by mint" turns a whole class of representation-confusion bugs into type
errors.

---

## Act 4 — Write it once, for any representation (`step`)

**Why.** You wrote counter code against `Counter.t`. Now there's `Tally.t` — the
same idea, but represented as `Int * Int` instead of `Int`. Do you copy-paste your
code? No: **System-F polymorphism** lets you write a function once, abstract over
the carrier type, and apply it to either. And because the carrier is abstract
*inside* the function, the function physically cannot inspect the representation —
so it's guaranteed to work the same for every instantiation (parametricity).

**Do.**
1. Step a `Counter` up:
   ```
   Counter.get (step [Counter.t] (Counter.incr, Counter.decr) Counter.empty true)
   ```
   → `= 1`.
2. The *same* `step`, applied to the `Int * Int`-backed `Tally`:
   ```
   Tally.get (step [Tally.t] (Tally.incr, Tally.decr) Tally.start false)
   ```
   → `= -1`.

**Point at.** Select `step`: its type is `forall t. (t -> t) * (t -> t) -> t ->
Bool -> t`. One definition, two representations, zero duplication — and `step`
never learns whether `t` is an `Int` or an `Int * Int`.

---

## Act 5 — A whole module as one value (`sig` / `struct` / `:>` → open)

**Why.** So far the abstract type and its operations are loose definitions tied
together only by names. p17 bundles them: a **signature** (`sig { type t,
empty: t, … }`) says what a module provides — including which type members are
hidden — a **struct** implements it, and **ascription `M :> S`** seals the
struct at the signature. The result is *one value* that carries its own hidden
type. A *factory* can then choose the representation at runtime — "fast" vs
"compact" — and hand back a working module without telling you which it picked.

**Do.**
1. Select `CounterSig` in the browser. The detail pane lists its components:
   `type t` (opaque), then `empty` / `incr` / `get` typed over `t`.
2. Select `counter.impl` — the implementation struct. Its type is the **fully
   manifest** sig: `sig { type t = Int, … }`, nothing hidden, and it has an
   extra member `raw` that `CounterSig` never mentions.
3. Select `counter.sealed` (`counter.impl :> CounterSig`) — same value, but its
   type is now `CounterSig`: `t` is hidden and **`raw` is gone** (width
   subtyping = private members).
4. Type just:
   ```
   mk_counter true
   ```
   Its type is `CounterSig`, and an **open as module** block appears that
   *previews every binding the open would create* — `.t`, `.empty`, `.incr`,
   `.get`, names recovered from the sig's own labels. Nothing to fill in.
5. Put `Fast` in the name box and click **open as module**. Expand `Fast` in
   the browser: `Fast.t` (abstract), `Fast.empty` / `Fast.incr` / `Fast.get`.
   ```
   Fast.get (Fast.incr (Fast.incr Fast.empty))
   ```
   → `= 2`.
6. Open `mk_counter false` as `Compact` — it's backed by `Int * Int`, but
   behaves identically, and `Fast.t` / `Compact.t` are *different* abstract
   types. (So are `Box.t` and `Box2.t`, both pre-opened from the *same*
   expression on load — opening is **generative**. Try `Box.get Box2.empty`.)

**Point at.** This is OCaml's `module Fast = (val mk_counter true)` — but
`mk_counter` needed no `pack`: each branch is just a struct ascribed at the one
`CounterSig`. Ascription mints nothing (re-author it and the hash is the same);
**opening** is what mints the fresh abstract type.

---

## Act 6 — A module hiding *two* types, and the bug it catches (`mk_calendar`)

**Why — the money moment.** Real modules hide more than one type. A calendar has
**dates** and **durations**, and they are *not* interchangeable: you can shift a
date by a duration, and subtract two dates to get a duration, but "date + date" is
nonsense. In p15 this took *nested* existentials peeled one level at a time; in
p17 it's just **one sig with two opaque components** — and one open gesture.

**Do.**
1. Type `mk_calendar 0`. The open preview shows **both** hidden types — `date`
   and `span`, named from their labels — plus the five operations. Name the
   module `Clock` and click **open as module**.
2. Do something sensible — shift the origin forward 30 days and measure the gap:
   ```
   Clock.length_of (Clock.between Clock.origin (Clock.shift Clock.origin (Clock.after 30)))
   ```
   → `= 30`.
3. Now do the nonsense — add two dates:
   ```
   Clock.shift Clock.origin Clock.origin
   ```
   → `error`. `Clock.shift` is `Clock.date -> Clock.span -> Clock.date`; you handed
   it a `Clock.date` where a `Clock.span` belongs.

**Point at.** Select `CalSig`: `type date, type span` side by side in one
signature. The distinction between the two hidden types is doing real work —
the "add two dates" bug can't even be written.

---

## Act 7 — Translucency: hide *some* types, expose others (`VecSig`)

**Why.** Hiding is per-component. A vector module wants its vector
representation hidden — but its *scalar* type is part of the contract: callers
must be able to write `smul 3 v` with a plain `3`. A **manifest** component
(`type scalar = Int`) stays transparent through sealing *and* opening; an
opaque one (`type v`) does not. That mix is what "translucent" means.

**Do.**
1. Select `VecSig`: `type scalar = Int` (manifest, equation shown) next to
   `type v` (opaque).
2. Expand `Vec` in the browser — `Vec.scalar` is bound to **`Int` itself**,
   while `Vec.v` is an abstract type.
3. Type:
   ```
   Vec.norm1 (Vec.smul 3 Vec.one)
   ```
   → `= 6`. The literal `3` feeds `smul` directly because `scalar` never
   stopped being `Int`; meanwhile nothing you write can inspect a `Vec.v`.

---

## Act 8 — Open *inside* a function (the p15 question, answered)

**Why.** Until now, opening was a top-level gesture — p15 recorded "no way to
open an existential inside a function body" as an open question. p17 adds the
term form: `open e as M in body`. It is **scoped like unpack**: the hidden
types live only inside the body (the checker rejects any attempt to leak one),
and *no* fresh type is minted — re-authoring the identical function gives the
identical hash, like every other term.

**Do.**
1. Select `total` and read its source:
   ```
   \m: CounterSig. open m as c in c#get (c#incr (c#incr c#empty))
   ```
   One function that consumes *any* counter module — whichever representation
   it secretly carries.
2. Type:
   ```
   total (mk_counter true) + total (mk_counter false)
   ```
   → `= 4`. Both witnesses, one consumer, no top-level open anywhere.
3. Try to leak the hidden type — type:
   ```
   \m: CounterSig. open m as c in c#empty
   ```
   → `error: … a hidden type would escape its scope`. The body's result may not
   mention `c`'s hidden type: outside the body, nothing could ever consume it.

**Point at.** `total`'s type is just `CounterSig -> Int` — the open left no
trace. Inside the body you can also write `c.empty` for `c#empty`, and annotate
with `c.t` (e.g. `\x: c.t. …`).

---

## Aside — records are also just data

Records aren't only module interfaces; they're ordinary values. Type:
```
Geom.taxicab { x = 10, y = 20 }
```
→ `= 30`. `Geom.taxicab` is `\p: Geom.Point. p#x + p#y` — `#` projects a field.
The field names `x` / `y` are content-addressed **labels** (declaring
`Geom.Point` minted them): rename a label and every record that uses it keeps its
hash, and two record types that mention the same `x` share one label.

---

## The through-line

1. Hide a representation, and the checker enforces it (`Counter`).
2. Touching the representation is an explicit, auditable act (editing context →
   sealing).
3. Two types can share a representation yet stay distinct (`Celsius`/`Kelvin`).
4. Abstract over the representation to write code once (`step`, System-F).
5. A whole module is one value: a struct sealed at a signature (`:>`), private
   members dropped, opened with every name recovered from the sig's labels
   (`mk_counter`).
6. One signature can hide several types at once, and their distinctness catches
   real bugs (`mk_calendar`).
7. Hiding is per-component: manifest type members stay transparent through
   sealing and opening (`VecSig`).
8. Opening works *inside* a function body too — scoped, escape-checked, and
   mint-free (`total`).

Everything above is content-addressed: the opaque and abstract types, the module
values, and the record-field labels are all definitions in one store, referenced
by hash, with human-readable names living only in the namespace.
