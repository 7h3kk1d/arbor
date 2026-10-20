# p16 — a guided demo of type abstraction

A script for demonstrating the prototype, about 15 minutes top to bottom. Each act gives the reason for a feature, the steps to run, and what to point at.

Every act is about one idea: a type can hide its representation, and the type checker enforces that. The acts go from hiding one type's representation to handing someone a module whose representation they can never see. Types and record-field names are stored content, not strings inside programs.

## Run it

```sh
eval $(opam env --switch=. --set-switch)
scripts/build-web.sh        # builds public/p16.js
open public/index.html      # three panes, no server, state resets on reload
```

Panes:

- **Browser** (left): the namespace tree. Click a name to inspect it; click a module's ▸ to expand it.
- **Editor** (center): a work area that type-checks and evaluates what you type, live. Nothing is saved until you click **bind**.
- **Detail** (right): the selected definition's type, source and value.

Hidden types come in two kinds, with different badges:

- **opaque**: a representation exists but consumers cannot see it (`Counter.t`). It has an `edit` toggle so you can reveal it while authoring.
- **abstract**: there is no representation at all. Opening an existential produces one (`Box.t`). There is nothing to reveal.

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

## Act 5 — A value that hides its own representation (`mk_counter`, then open a module)

**Why.** So far the declarer of a type fixes its representation. A factory may want to choose, say "fast" or "compact", and return a working module without saying which. That is an existential type. `open` turns the package into a module in your namespace: an abstract type you can name but not inspect, plus its operations, named from the record interface.

> The namespace already contains `Box` and `Cal`, opened on load. Below you open fresh ones to watch the form work.

**Do.**
1. Type:
   ```
   mk_counter true
   ```
   The type is `exists t. { … }`. An **open as module** form appears with one type input (default `t`) and field inputs pre-filled from the record's labels (`empty`, `incr`, `get`).
2. Enter `Fast` as the module name and click **open as module**. Expand `Fast` in the browser: `Fast.t` (abstract), `Fast.empty`, `Fast.incr`, `Fast.get`.
3. Use it:
   ```
   Fast.get (Fast.incr (Fast.incr Fast.empty))
   ```
   Feedback: `= 2`.
4. Type `mk_counter false` and open it as `Compact`. This branch packs an `Int * Int`, yet `Compact.get (Compact.incr Compact.empty)` gives `= 1`. From outside you cannot tell which representation you got, and `Fast.t` and `Compact.t` are different abstract types.

**Point at.** This is OCaml's `module Fast = (val mk_counter true)`. The field names came from the record interface's labels. With a plain tuple you would have to remember that the second field is `incr`.

> A second entry point: click `mk_counter` in the browser. The detail pane recognizes the existential and offers an inline **open** box.

---

## Act 6 — A module with two abstract types (`mk_calendar`)

**Why.** A calendar has dates and durations. You can shift a date by a duration, and subtract two dates to get a duration, but adding two dates is meaningless. With two distinct abstract types, adding two dates is a type error.

**Do.**
1. Type `mk_calendar 0`. The open form shows two type inputs. Name them `date` and `span`, name the module `Clock`, and click **open as module**. (`Cal` is the same module opened on load.)
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

**Point at.** Select `Clock.shift` and `Clock.between`: `date -> span -> date` and `date -> date -> span`. Two hidden types, kept distinct behind one named interface, rule out the bug.

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
5. A value can hide its own representation and be opened as a module with named operations (`mk_counter`).
6. A module can hide several types, and keeping them distinct catches bugs (`mk_calendar`).

The opaque and abstract types, the module values and the labels are all definitions in one store, referenced by hash. Names live only in the namespace.
