# p14 — Existentials

Forks p13 and adds closed existentials: `exists t. T`, `pack [W] e as E`, and `unpack [t] x = e in body`. A value can now hide its own representation, chosen by the producer. p13's `forall` makes a consumer generic; `exists` lets a producer pick the representation. The driving example is a factory whose branches pack different witnesses at one type:

```
mkCounter : Bool -> exists t. t * ((t -> t) * (t -> Int))
mkCounter = \fast: Bool.
  if fast then pack [Int]     (0,     (\x:Int. x+1,  \x:Int. x))      as exists t. t * ((t -> t) * (t -> Int))
          else pack [Int*Int] ((0,0), (\p:Int*Int. (fst p+1, snd p), \p:Int*Int. fst p)) as exists t. ...

unpack [t] c = mkCounter true in
  snd (snd c) ((fst (snd c)) ((fst (snd c)) (fst c)))   -- get (incr (incr empty)) : Int
```

Inside `unpack`, `t` is a fresh rigid type variable, so the feature reuses p13's `TVar`, shift and substitution machinery. Avoidance is enforced: the witness cannot escape the `unpack` body. The interface is a positional `Product`; records come in p16.

Design and scope: `docs/prototypes/p14-existentials/` (`00-scope.md`, `decisions.md`, `open-questions.md`).

## Status

Complete. `dune runtest` passes 10 tests: p13's 9 plus the `mkCounter` factory hiding its representation through `pack`/`unpack`.

## Changes from p13

- `tnode.re` adds `Exists(body)`.
- `node.re` adds `Pack` and `Unpack`.
- `typecheck.re` checks pack against the declared existential and enforces avoidance on unpack.
- The parser accepts `exists t. T`, `pack [W] e as E` and `unpack [t] x = e in body`.

Everything else carries over from p13. See `../p12-abstract-types/README.md` for the module list and REPL commands.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
dune exec ./bin/repl.exe      # :help walks through the Counter example
scripts/build-web.sh          # builds public/p14.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. The substrate library is `P14_substrate`; the stack is p12's.
