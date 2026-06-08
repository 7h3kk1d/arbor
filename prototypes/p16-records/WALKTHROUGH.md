# p16 walkthrough — the substrate's features, by example

p16 (records / labeled modules) is the head of the type-abstraction line
(p12 abstract types → p13 System-F → p14 existentials → p15 open → p16 records).
The builtins are a curated tour: **one clean example per feature**. This doc has
two parts — a **reading tour** of the builtins (Part 1), and a **hands-on UX
session** you can run in the web app (Part 2).

## Running it

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest          # 17 substrate tests
scripts/build-web.sh                # builds public/p16.js
open public/index.html              # the three-pane web app (no server)
# or the REPL:
dune exec ./bin/repl.exe            # :help for commands
```

The web app has three panes: **browser** (left — the namespace tree, a filter,
and a live tests tally), **editor** (center — a live work area + a "new type"
form), and **detail** (right — the selected definition's type, source, value,
and test toggle).

---

## Part 1 — the builtins, feature by feature

Open the app and read the left pane top-to-bottom. Each kind-badge tells you the
sort: **term**, **type**, **abstract** (an opaque type with an `edit` toggle),
**sealed** (a term that opened an abstract type's representation), or **label**
(a record-field identity — labels are a first-class minted sort).

### 1. Three content-addressed sorts: term, type, label
Everything in the tree is content-addressed. A *type* is a definition like a
term; a *label* (record field identity) is a third sort. You'll see bare label
entries (`x`, `y`, `empty`, `incr`, `get`, `origin`, …) — these are the minted
field labels, bound to their names in the namespace. Renaming a label never moves
the hash of any record that references it.

### 2. Abstract types & editor-enforced opacity — `Counter`
`Counter.t` is an **abstract type** (badge: abstract) whose representation is
`Int`, hidden. `Counter.empty` / `incr` / `get` / `decr` are its operations —
**sealed** terms (they were authored with `Counter.t`'s representation open). A
consumer like `bump2 = \c. incr (incr c)` composes the abstract API and never
touches the representation, so it stays an **ordinary** (unsealed) term —
*minimal sealing*. Click `Counter.t` and the detail pane lists every definition
that unseals it (derived, not stored). Opacity isn't enforced at ingest; it lives
in the editor's *editing context* (the `edit` toggle).

### 3. Distinct abstract types + ops that span both — `Celsius` / `Kelvin` / `Temp`
`Celsius.t` and `Kelvin.t` are *both* abstract over `Int`, yet **distinct** —
minted separately, so the type system keeps them apart. `Temp.c_to_k :
Celsius.t -> Kelvin.t` and `Temp.k_to_c` convert between them (each opened *both*
types); `Temp.round_trip` composes them at the abstract level.

### 4. System-F polymorphism — `Tally` + `step`
`Tally.t` is a *second* counter, represented as `Int * Int` instead of `Int`.
`step : forall t. (t -> t) * (t -> t) -> t -> Bool -> t` is **one polymorphic
functor** applied to either: `step [Counter.t] …` and `step [Tally.t] …` (see the
`Functor.Tests`). Inside `step`, `t` is opaque (parametricity) — the same code,
two different representations.

### 5. Existentials + open — `mkCounter` → `Box`
`mkCounter : Bool -> exists t. { empty: t, incr: t -> t, get: t -> Int }` is a
**factory** that *hides its own representation* (the `true` branch packs `Int`,
the `false` branch packs `Int * Int`) behind one `∃` type. Its interface is a
**record**. `Box` is `mkCounter true` **opened into the namespace**: `Box.t` (the
extracted abstract type), `Box` (the value), and `Box.empty` / `incr` / `get` —
their names **recovered from the record's labels**, no positional guessing. This
is OCaml's `module Box = (val mkCounter true)`.

### 6. n-ary open + the safety payoff — `mkCalendar` → `Cal`
`mkCalendar : Int -> exists date. exists span. { … }` hides **two** distinct
abstract types. Opening peels both at once: `Cal.date`, `Cal.span`, and the
operations `origin` / `after` / `shift` / `between` / `lengthOf`. The point is
the discipline two distinct types buy — `Cal.shift : Cal.date -> Cal.span ->
Cal.date`, so adding two dates (`Cal.shift Cal.origin Cal.origin`) or measuring a
date as a duration (`Cal.lengthOf Cal.origin`) is a **type error**.

### 7. Records as plain data + `#`-projection — `Geom`
`Geom.Point = { x: Int, y: Int }` is a record *type* (declaring it minted the
labels `x`, `y`). `Geom.origin` / `Geom.example` are record *values*;
`Geom.taxicab = \p: Geom.Point. p#x + p#y` projects fields with `#`.

### 8. Lists — `demo.nums`
`demo.nums : List Int = [| 1, 2, 3 |]` (literal syntax, distinct from `[T]` type
application). `fold` is the only eliminator (a right fold; no general recursion).
See `Lists.Tests.sum`.

### 9. Tests — the tally
Any `Bool`-typed binding tagged with the `test` aspect shows a live pass/fail
badge inline and feeds the sticky tally at the bottom of the browser. The tests
are evaluated live (no caching). `Counter.Tests.oops` **fails on purpose** —
that's why the tally is red — to show what a failing test looks like.
`Counter.Tests.Internal.*` are *internal* tests that unseal the representation
(they compare an abstract value to its `Int`), so they're themselves sealed.

---

## Part 2 — a guided UX session

Do these in order in the web app. Each step calls out the feature it exercises.

1. **Live evaluation (abstract API).** In the editor's **work** area, type:
   ```
   Counter.get (bump2 Counter.empty)
   ```
   The feedback shows `: Int` and `= 2`, live, with no binding. You're using the
   abstract `Counter` API; the representation never surfaces.

2. **Bind with an inferred type.** Keep an expression in the work area, e.g.
   `Counter.incr (Counter.incr Counter.empty)`, put `twice` in the **name** box,
   leave **type** blank, and click **bind**. It's bound with the type
   *synthesized* (`Counter.t`) — the annotation is optional. Find `twice` in the
   tree (use the filter box: type `twice`).

3. **Open an existential (the headline).** Clear the work area and type:
   ```
   mkCounter true
   ```
   Because it's an `exists …`, an **open as module** block appears, with **one
   type input** (default `t`) and **three field inputs pre-filled from the
   labels** (`empty`, `incr`, `get`). Put `My` in the name box and click **open
   as module**. Now `My.t`, `My.empty`, `My.incr`, `My.get` are in the tree.
   Type in the work area: `My.get (My.incr My.empty)` → `= 1`. (No `providing`
   list — the field names came from the record's labels.)

4. **n-ary open + a type error you *want*.** Type `mkCalendar 0` in the work
   area; the open block now shows **two** type inputs. Name the first `date` and
   the second `span` (the field inputs pre-fill), name the module `K`, and open.
   Then type:
   ```
   K.shift K.origin K.origin
   ```
   The feedback goes red: a date isn't a span. Now `K.lengthOf (K.between
   K.origin (K.shift K.origin (K.after 30)))` → `= 30`.

5. **Open from the detail pane.** Click `mkCalendar` in the tree. The detail pane
   recognizes it as an existential and offers an inline **open** box — the second
   entry point to the same gesture.

6. **Editing context → minimal sealing.** In the browser, find `Counter.t` and
   click its **edit** toggle (it lights up: the representation is now open for
   authoring). Back in the work area, type:
   ```
   Counter.empty + 1
   ```
   It now type-checks (`Counter.t` unfolds to `Int` while open) and shows `= 1`.
   Name it `peek`, click **bind** — the result badge says **SEALED**, because it
   genuinely needed the representation. Click `Counter.t`'s **edit** toggle again
   to close. (Without the toggle, that same expression is a type error — opacity.)

7. **Records as data + projection.** Type:
   ```
   Geom.taxicab { x = 10, y = 20 }
   ```
   → `= 30`. The record literal's `x` / `y` resolve to the labels declared by
   `Geom.Point`; `taxicab` projects them with `#`.

8. **Lists.** Type `fold [| 4, 5, 6 |] 0 (\x: Int. \acc: Int. x + acc)` → `= 15`.

9. **The tests tally.** Look at the bottom of the browser: it reads red, e.g.
   `tests: N passing / M` with one failing. Type `Tests` in the filter to show
   only test bindings (across all modules). Click `Counter.Tests.oops` — the
   detail pane shows it evaluating to `false`. Everything else is green.

10. **Make your own test.** Bind a `Bool` expression (step 2's flow), e.g. name
    `mytest`, expr `Counter.get Counter.empty == 0`. Select it in the detail pane
    and click **mark as test** — it joins the tally as a passing test. Edit it to
    `== 1` and re-bind to watch a test you authored go red.

That covers every feature: content addressing & the three sorts, abstract types +
editor-enforced opacity + minimal sealing, distinct types + spanning ops, System-F
polymorphism, existentials, open (unary and n-ary) with record interfaces, records
as data + projection, lists, and the test aspect.
