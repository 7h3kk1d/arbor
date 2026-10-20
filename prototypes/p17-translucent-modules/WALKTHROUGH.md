# p17 — a guided demo of type abstraction

A script for demonstrating the prototype, about 15 minutes top to bottom. Each act gives the reason for a feature, the steps to run, and what to point at.

Every act is about one idea: a type can hide its representation, and the type checker enforces that. The acts go from hiding one type's representation to handing someone a module whose representation they can never see. Types and record-field names are stored content, not strings inside programs.

## Run it

```sh
eval $(opam env --switch=. --set-switch)
scripts/build-web.sh        # builds public/p17.js
open public/index.html      # three panes, no server, state resets on reload
```

Panes:

- **Browser** (left): the namespace tree. Click a name to inspect it; click a module's ▸ to expand it.
- **Editor** (center): a work area that type-checks and evaluates what you type, live. Nothing is saved until you click **bind**.
- **Detail** (right): the selected definition's type, source and value.

Hidden types come in two kinds, with different badges:

- **opaque**: a representation exists but consumers cannot see it (`Counter.t`). It has an `edit` toggle so you can reveal it while authoring.
- **abstract**: there is no representation at all. Opening a module produces one (`Box.t`). There is nothing to reveal.

Signatures get a **sig** badge.

> Not covered here, though present in the namespace: the `*.Tests.*` bindings with their pass/fail tally, and `demo.nums`/lists.

---

## Act 1 — Hide the representation (`Counter`)

**Why.** A counter is an `Int` underneath. Exposed as `Int`, a caller could write `c * 2`, store `-7`, or pass any number where a counter belongs. An opaque type hides the representation. Callers get only the published operations.

**Do.**
1. In the work area, type:
   ```
   Counter.get (Counter.incr (Counter.incr Counter.empty))
   ```
   Feedback: `: Int` and `= 2`.
2. Try to cheat:
   ```
   Counter.empty + 1
   ```
   Feedback: `error: type error …`. `Counter.empty` is a `Counter.t` and `+` wants an `Int`.

**Point at.** Expand `Counter` in the browser and click `Counter.t`. Its badge is **opaque**, and the detail pane says *opaque type — opaque(…)*, representation hidden from consumers. The only way to make or change a counter is `empty`, `incr` and `get`.

---

## Act 2 — Someone has to write `incr` (editing context and sealing)

**Why.** `incr` must touch the `Int` (`x + 1`). You open the type's representation in an editing context while authoring. Any definition that used the representation is recorded as **sealed**. Definitions that only compose the public API are not. Afterwards the substrate can list which definitions saw the representation.

**Do.**
1. In the browser, click `Counter.t`'s **edit** toggle. It lights up.
2. Type `Counter.empty + 1` again. It now type-checks and shows `= 1`, because with the type open `Counter.t` is `Int`.
3. Enter `peek` in the **name** box and click **bind**. The badge reads **SEALED**.
4. Turn the **edit** toggle off. `Counter.empty + 1` is a type error again.

**Point at.** Click `Counter.t`. The detail pane lists its **unsealing definitions**: `empty`, `incr`, `get`, `decr`, and now `peek`. `bump2 = \c. incr (incr c)` is not listed; it uses only the public API, so it stayed an ordinary term. The list is derived by scanning the store, not stored.

---

## Act 3 — Same bits, different type (`Celsius` vs `Kelvin`)

**Why.** Celsius and Kelvin are both an `Int` number of degrees. Confusing units like these lost the Mars Climate Orbiter. Two opaque types over the same representation are still distinct, because each was minted separately. The only bridge is an explicit conversion.

**Do.**
1. Type the unit bug:
   ```
   Kelvin.value Celsius.freezing
   ```
   Feedback: `error`. `Kelvin.value` wants a `Kelvin.t`.
2. Convert first:
   ```
   Kelvin.value (Temp.c_to_k Celsius.freezing)
   ```
   Feedback: `= 273`.

**Point at.** Select `Temp.c_to_k`. Its type is `Celsius.t -> Kelvin.t`. Distinct mints turn representation confusion into a type error.

---

## Act 4 — Write it once, for any representation (`step`)

**Why.** `Tally.t` is another counter, represented as `Int * Int`. System-F lets you write a function once over the carrier type and apply it to both. Inside the function the carrier is a type variable, so the function cannot inspect the representation and behaves the same for every instantiation.

**Do.**
1. Step a `Counter`:
   ```
   Counter.get (step [Counter.t] (Counter.incr, Counter.decr) Counter.empty true)
   ```
   Feedback: `= 1`.
2. Apply the same `step` to `Tally`:
   ```
   Tally.get (step [Tally.t] (Tally.incr, Tally.decr) Tally.start false)
   ```
   Feedback: `= -1`.

**Point at.** Select `step`. Its type is `forall t. (t -> t) * (t -> t) -> t -> Bool -> t`. One definition serves both representations, and `step` never learns which one `t` is.

---

## Act 5 — A whole module as one value (`sig`, `struct`, `:>`, then open)

**Why.** Up to now an abstract type and its operations are separate definitions linked only by names. p17 bundles them. A **signature** (`sig { type t, empty: t, … }`) says what a module provides and which type members are hidden. A **struct** implements it. **Ascription** `M :> S` seals the struct at the signature. The result is one value that carries its own hidden type, so a factory can pick a representation at runtime and return a module without saying which.

**Do.**
1. Select `CounterSig`. The detail pane lists `type t` (opaque), then `empty`, `incr` and `get` typed over `t`.
2. Select `counter.impl`, the implementing struct. Its type is the fully manifest sig `sig { type t = Int, … }`. It also has a member `raw` that `CounterSig` does not mention.
3. Select `counter.sealed` (`counter.impl :> CounterSig`). Same value, but its type is `CounterSig`: `t` is hidden and `raw` is gone. Width subtyping makes unlisted members private.
4. Type:
   ```
   mk_counter true
   ```
   Its type is `CounterSig`. An **open as module** block previews every binding the open would create (`.t`, `.empty`, `.incr`, `.get`), with names taken from the sig's labels. There is nothing to fill in.
5. Enter `Fast` as the name and click **open as module**. Expand `Fast`: `Fast.t` (abstract), `Fast.empty`, `Fast.incr`, `Fast.get`.
   ```
   Fast.get (Fast.incr (Fast.incr Fast.empty))
   ```
   Feedback: `= 2`.
6. Open `mk_counter false` as `Compact`. It is backed by `Int * Int` but behaves the same, and `Fast.t` and `Compact.t` are different abstract types. `Box.t` and `Box2.t` were both opened from the same expression on load and are also different, because opening is generative. Try `Box.get Box2.empty`.

**Point at.** This is OCaml's `module Fast = (val mk_counter true)`. `mk_counter` needs no `pack`: each branch is a struct ascribed at `CounterSig`. Ascription mints nothing, so re-authoring it gives the same hash. Opening is what mints the fresh abstract type.

---

## Act 6 — A module hiding two types (`mk_calendar`)

**Why.** A calendar has dates and durations. You can shift a date by a duration, and subtract two dates to get a duration, but adding two dates is meaningless. p15 needed nested existentials peeled one level at a time. In p17 it is one sig with two opaque components and one open.

**Do.**
1. Type `mk_calendar 0`. The open preview shows both hidden types, `date` and `span`, named from their labels, plus the five operations. Name the module `Clock` and click **open as module**.
2. Shift the origin forward 30 days and measure the gap:
   ```
   Clock.length_of (Clock.between Clock.origin (Clock.shift Clock.origin (Clock.after 30)))
   ```
   Feedback: `= 30`.
3. Add two dates:
   ```
   Clock.shift Clock.origin Clock.origin
   ```
   Feedback: `error`. `Clock.shift` is `Clock.date -> Clock.span -> Clock.date`, and its second argument is a `Clock.date`.

**Point at.** Select `CalSig`: `type date` and `type span` in one signature. Keeping them distinct rules out the bug.

---

## Act 7 — Translucency: hide some types, expose others (`VecSig`)

**Why.** Hiding is per component. A vector module hides its vector representation, but its scalar type is part of the contract: callers must be able to write `smul 3 v` with a plain `3`. A **manifest** component (`type scalar = Int`) stays transparent through sealing and opening. An opaque one (`type v`) does not.

**Do.**
1. Select `VecSig`: `type scalar = Int` (manifest, equation shown) next to `type v` (opaque).
2. Expand `Vec`. `Vec.scalar` is bound to `Int` itself; `Vec.v` is an abstract type.
3. Type:
   ```
   Vec.norm1 (Vec.smul 3 Vec.one)
   ```
   Feedback: `= 6`. The literal `3` goes straight to `smul` because `scalar` is still `Int`. Nothing you write can inspect a `Vec.v`.

---

## Act 8 — Open inside a function

**Why.** Until now opening was a top-level gesture. p15 listed opening an existential inside a function body as an open question. p17 adds the term form `open e as M in body`. It is scoped like `unpack`: the hidden types exist only inside the body, the checker rejects any attempt to leak one, and no fresh type is minted. Re-authoring the same function gives the same hash.

**Do.**
1. Select `total` and read its source:
   ```
   \m: CounterSig. open m as c in c#get (c#incr (c#incr c#empty))
   ```
   It consumes any counter module, whatever its representation.
2. Type:
   ```
   total (mk_counter true) + total (mk_counter false)
   ```
   Feedback: `= 4`. Both witnesses, one consumer, no top-level open.
3. Try to leak the hidden type:
   ```
   \m: CounterSig. open m as c in c#empty
   ```
   Feedback: `error: … a hidden type would escape its scope`. The body's result may not mention `c`'s hidden type, since nothing outside the body could consume it.

**Point at.** `total`'s type is `CounterSig -> Int`; the open leaves no trace in it. Inside the body you can also write `c.empty` for `c#empty`, and annotate with `c.t` (for example `\x: c.t. …`).

---

## Aside — records are also plain data

Type:
```
Geom.taxicab { x = 10, y = 20 }
```
Feedback: `= 30`. `Geom.taxicab` is `\p: Geom.Point. p#x + p#y`; `#` projects a field. The field names `x` and `y` are labels, minted when `Geom.Point` was declared. Renaming a label leaves every record that uses it at the same hash, and two record types that mention the same `x` share one label.

---

## Summary

1. A hidden representation is enforced by the checker (`Counter`).
2. Touching the representation is explicit and auditable (editing context and sealing).
3. Two types can share a representation and stay distinct (`Celsius`/`Kelvin`).
4. Abstracting over the representation lets you write code once (`step`).
5. A module is one value: a struct sealed at a signature with `:>`, private members dropped, opened with every name taken from the sig's labels (`mk_counter`).
6. One signature can hide several types, and keeping them distinct catches bugs (`mk_calendar`).
7. Hiding is per component: manifest type members stay transparent through sealing and opening (`VecSig`).
8. Opening also works inside a function body, scoped, escape-checked and without a mint (`total`).

The opaque and abstract types, the module values and the labels are all definitions in one store, referenced by hash. Names live only in the namespace.
