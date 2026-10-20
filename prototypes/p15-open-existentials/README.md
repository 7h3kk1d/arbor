# p15 — Open existentials, namespace type extraction, lists

Forks p14 and adds two things: a primitive `open` gesture and lists. Opening an existential package binds both the value and its hidden type into the namespace, so later top-level definitions can use them. This is OCaml's `module Counter = (val e) in <rest>`:

```
:open mkCounter true as Counter providing empty, incr, get
-- binds Counter.t (the extracted abstract type), Counter (the package tuple,
--   typed Counter.t * ((Counter.t -> Counter.t) * (Counter.t -> Int))),
--   and Counter.empty / incr / get as positional projections
Counter.get (Counter.incr (Counter.incr Counter.empty))   -- : Int = 2
```

`open` binds two names, `Counter.t` and `Counter`. `providing f1, f2, …` is optional sugar that names positional `Product` projections. The extracted type is a minted `Abstract(mint)`: it has no witness, never unfolds, and every open mints a fresh one.

Design and scope: `docs/prototypes/p15-open-existentials/00-scope.md`.

## Status

Complete. `dune runtest` passes 14 tests: p14's 10 plus generative open with lists, n-ary open, the calendar example (distinct `date`/`span` types reject illegal operations), and `[| … |]` literals.

## Changes from p14

- `tnode.re` adds `Abstract(mint)` and `List(elem)`.
- `node.re` adds `Open{pkg, mint}` (a generative open), `Nil(elem-ty)`, `Cons` and `Fold` (a right fold).
- `open_existential.re` implements the gesture. **N-ary open**: a package hiding several types is a nest of unary existentials, peeled by composing `Open` nodes (`Open{Open{pkg, m1}, m2}`), one mint per level. `:open <expr> as <Name> [t1, t2] [providing f1, …]` names the extracted types.
- Surface syntax: `nil [T]`, `cons h t`, `fold l z f`, and `[| 1, 2, 3 |]` literals, whose element type is inferred from the head.

The web UI was reworked: one editor with an optional bind annotation, a collapsible namespace tree with inline test pass/fail badges, a sticky tally and a whole-namespace filter, and a structure-driven "open as module" form.

Everything else carries over from p14. See `../p12-abstract-types/README.md` for the base module list and REPL commands.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
dune exec ./bin/repl.exe      # :help walks through the examples
scripts/build-web.sh          # builds public/p15.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. The substrate library is `P15_substrate`; the stack is p12's.
