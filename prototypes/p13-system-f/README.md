# p13 — System-F over abstract types

Forks p12 and adds predicative System-F: type variables, type abstraction (`/\t. e`), type application (`e [T]`) and `forall` types. A program can now be generic over the carrier type and apply to a counter with any representation:

```
step : forall t. ((t -> t) * (t -> t)) -> t -> Bool -> t
step = /\t. \ops: (t -> t) * (t -> t). \x: t. \b: Bool. if b then (fst ops) x else (snd ops) x

step [Counter.t] (Counter.incr, Counter.decr) Counter.empty true   -- : Counter.t
```

An ML functor's argument becomes a type argument plus a value bundle, here a `Product`. Existentials and F-omega are out of scope.

Design and scope: `docs/prototypes/p13-system-f/` (`00-scope.md`, `decisions.md`, `open-questions.md`).

## Status

Complete. `step` type-checks, renders and evaluates in the REPL, and the web inputs accept the same syntax. There is no dedicated web affordance for polymorphism. `dune runtest` passes 9 tests: p12's 8 plus one polymorphic `step` applied to two representations.

## Changes from p12

- `tnode.re` adds `TVar(int)` and `Forall(body)`.
- `node.re` adds `TyLam` and `TyApp`.
- `typecheck.re` adds type shifting and substitution, and checks the new forms. Evaluation erases types.
- The parser accepts `/\t. e`, `e [T]` and `forall t. T`.

Everything else carries over from p12: `Opaque` and `Seal`, the opacity-parameterized checker, the editing context with minimal sealing, the REPL commands, and the three-pane web app. See `../p12-abstract-types/README.md` for the module list.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
dune exec ./bin/repl.exe      # :help walks through the Counter example
scripts/build-web.sh          # builds public/p13.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. The substrate library is `P13_substrate`; the stack is p12's.
