# p16 — Records and labeled modules

Forks p15 and adds the label sort (`docs/design/11-label-sort.md`). A `Label` is a first-class minted definition. Record types and literals reference labels by hash and are canonical by sorted label hash. An existential package's interface can now be a record, so `open` recovers field names from the labels:

```
mk_counter : Bool -> exists t. { empty: t, incr: t -> t, get: t -> Int }
:open mk_counter true as Counter        -- binds Counter.empty / incr / get by label name
Counter.get (Counter.incr Counter.empty)
```

Only labels mint. Terms and types stay structural; p10's mint-everything does not return. Labels mint at the record-type declaration. Projection is a separate `e#x` operator. `Record_update` is deferred.

Design and scope: `docs/prototypes/p16-records/00-scope.md`. For a guided demo, see `WALKTHROUGH.md`.

## Status

Complete. `dune runtest` passes 17 tests: p15's 14 plus the label sort with record types, literals and projection; opening an existential with a record interface; and the surface syntax for declarations, literals and projection. In-browser bootstrap tests run on load.

## Changes from p15

- `label.re` and `Definition.Label`: a label is a bare mint, sort byte `L`.
- `tnode.re` adds `Record(list((label, ty)))`; `node.re` adds `Record_lit` and `Project_field`. Both are sorted by label hash, so `{x, y}` and `{y, x}` are one object. Field names never appear in stored bytes.
- `open_existential.re` recovers each field's label, so opening binds `N.<field>` by name. Positional `Product` interfaces still work with `providing`.
- Surface syntax: `{ x: T }` types, `{ x = e }` literals, `e#x` projection. A record-type declaration mints its field labels; a field name that is already bound reuses its label. Literals and projection need already-bound labels.
- The REPL renames `:abstract` to `:opaque` and adds `:type`.
- The web bootstrap reseeds `mk_counter` and `mk_calendar` with record interfaces, and the open form pre-fills field names from labels.

Everything else carries over from p15. See `../p12-abstract-types/README.md` for the base module list.

## Running

```sh
eval $(opam env --switch=. --set-switch)
dune build && dune runtest
dune exec ./bin/repl.exe      # :help walks through the examples
scripts/build-web.sh          # builds public/p16.js (~26 MB)
open public/index.html        # no server; state resets on reload
```

`_opam` symlinks to `../p7-web-interface/_opam/_opam`. The substrate library is `P16_substrate`; the stack is p12's.
