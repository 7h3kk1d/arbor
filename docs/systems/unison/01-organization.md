# Unison — How the codebase is organized

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope.

Everything below is `[source-verified @ db60ce2]` unless tagged otherwise. Paths are relative to the Unison repository root.

## The shape at a glance

39 packages, ~161,000 lines of Haskell, built with Stack and **hpack** — `package.yaml` is the source of truth and `*.cabal` is generated, so read the former. `stack.yaml` is the authoritative package list; there is no top-level `cabal.project`. `hie.yaml` maps every source directory to its component and is the most useful machine-readable index of the tree.

| Package | Lines | Role |
|---|---:|---|
| `unison-cli` | 42,902 | UCM: commands, handlers, transcripts, LSP, MCP, Share client |
| `parser-typechecker` | 37,182 | Typechecker, kind inference, coverage, printers, `Codebase` abstraction, SQLite impl, builtins |
| `unison-runtime` | 29,201 | ANF → MCode → interpreter, builtins, FFI, standalone serialization |
| `codebase2/*` | 12,011 | The V2 storage stack (see below) |
| `unison-share-api` | 10,371 | Local HTTP server + Share sync protocol types |
| `unison-core` (`unison-core1`) | 9,952 | V1 language core: `Term`, `Type`, `ABT`, `Name`, `Reference` |
| `lib/*` | 9,217 | 20 leaf utility packages, no upward dependencies |
| `unison-syntax` | 3,929 | Lexer, megaparsec base, name/hash-qualified-name syntax |
| `unison-merge` | 3,323 | Pure three-way namespace merge |
| `unison-hashing-v2` | 1,345 | The frozen hashing format |

Two structural facts dominate everything else.

## Fact 1 — there are two parallel type universes

This is the single most important thing to know before reading any Unison module, and nothing in the repository announces it up front.

- **`Unison.*`** — the "V1" types, in `unison-core/` (which confusingly builds the package `unison-core1`) and `parser-typechecker/`. Rich, eager, annotated: `Unison.Term` carries source annotations, `Unison.Codebase.Branch.Branch0` carries five derived cache fields recomputed on every setter (`parser-typechecker/src/Unison/Codebase/Branch/Type.hs:94-98`). These are the types the typechecker, parser, and printers work in.
- **`U.Codebase.*`** — the "V2" types, in `codebase2/`. Lean, lazy, storage-shaped: `U.Codebase.Branch.Type.Branch` wraps every field in a monad so children load on demand, with the header comment *"A re-imagining of `Unison.Codebase.Branch` which is less eager in what it loads"* (`codebase2/codebase/U/Codebase/Branch/Type.hs:36-37`).

The directory-to-package naming is genuinely inverted and worth stating once: **`codebase2/core/` builds `unison-core`, while `unison-core/` builds `unison-core1`.**

`parser-typechecker` owns the bridge, `Unison.Codebase.SqliteCodebase.Conversions`. The cost of the split is a large, dull, hand-written conversion layer; the benefit is that the storage layer never sees an annotation, a source position, or a derived cache, and cannot accidentally hash one.

**The observation for arbor.** `../../design/06-architecture.md` posits four layers with upward-only dependencies, and `../../../CLAUDE.md` §"Conventions" commits to *"typed values at substrate APIs — no byte arrays at public boundaries."* Unison satisfies both, but pays for it in a way arbor's prototypes have not yet had to: at scale, "typed values at the boundary" means **two sets of types and a conversion layer between them**, because the types convenient for a typechecker and the types convenient for a store diverge. arbor's `Definition.t` is currently one sum serving both roles. p17's `Node.t`/`Tnode.t` are already the storage-shaped ones and the resolver already does surface→core conversion; the V1/V2 split is what that becomes when annotations, laziness, and a wire format arrive.

## Fact 2 — the hashing format is frozen, and the freeze is structural

`unison-hashing-v2/` exists for one purpose: to hold types **that must never change**. The contract is stated in prose at `lib/unison-hashing/src/Unison/Hashing/ContentAddressable.hs:8-38`:

> The base instances of this class should only live in dedicated 'hashing packages' such as `unison-hashing-v2`, whose types and implementations should never change. … we must make sure that the implementation of `namespaceToHashingNamespace` never changes the fields in the corresponding `HashingNamespace`, even as features are added to or removed from `Namespace`.

Reinforced at `unison-hashing-v2/src/Unison/Hashing/V2/Tokenizable.hs:56-58`: *"Be very careful when adding or altering instances of this typeclass, changing the hash of a value is a major breaking change and requires a complete codebase migration."*

So there are **four** representations of a term, not two:

| Layer | Where | Changes? |
|---|---|---|
| V1 in-memory | `unison-core/`, `parser-typechecker/` | Freely |
| V2 in-memory | `codebase2/codebase/` | Freely |
| **Hashing** | `unison-hashing-v2/` | **Never** |
| On-disk serialization | `codebase2/codebase-sqlite/U/Codebase/Sqlite/Serialization.hs` | Freely, per-blob format id |

with two conversion bridges into the frozen one: `parser-typechecker/src/Unison/Hashing/V2/Convert.hs` (*"Converts V1 types to the V2 hashing types"*) and `codebase2/codebase-sqlite-hashing-v2/src/Unison/Hashing/V2/Convert2.hs` (*"Converts V2 types to the V2 hashing types"*).

**The hashed representation is not the stored representation, and this is enforced by the package graph.** `codebase2/codebase-sqlite/package.yaml` lists no dependency on `unison-hashing-v2` at all. Instead the storage layer takes hashing as a *value*: `HashHandle`, a record of hashing functions (`codebase2/codebase-sqlite/U/Codebase/Sqlite/HashHandle.hs:73-106`), instantiated in the separate wiring package `codebase2/codebase-sqlite-hashing-v2/`. Storage literally cannot call the hasher except through the handle it was given.

The two formats stay honest about each other via a checkable round-trip: `verifyTermFormatHash` (`codebase2/codebase-sqlite-hashing-v2/src/U/Codebase/Term/Hashing.hs:26-45`) takes an on-disk blob, reconstructs the letrec, converts to hashing types, re-hashes, and asserts equality.

**The observation for arbor.** `../../design/06-architecture.md` says serialization is internal to the layers that need it. Unison shows what that principle costs and buys when taken seriously: a dedicated frozen package, a dependency-injection seam so the store cannot reach the hasher, and a self-check that the two agree. arbor's prototypes currently compute the hash inside the store module (`p17/src/hash.re`, `node.re`), which is fine at prototype scale and is exactly the coupling `HashHandle` exists to break. It is also the concrete answer to `../../design/03-content-addressing.md` §"Hash representation", which says only that the algorithm is "not locked in" and that full state rebuilds are the fallback — Unison's answer is that you *never* rebuild, because you version instead (`02-store-and-hashing.md` §"Versioning").

## The layer graph

Roughly, with the interesting edges called out:

```
lib/*  (20 leaf packages: prelude, hash, sqlite, relation, pretty-printer, …)
  │
codebase2/core ──────── unison-core1 ──── unison-syntax
  │                        │  │              │
codebase2/codebase         │  └── unison-hashing-v2
  │                        │              │
codebase2/codebase-sqlite ─┘              │      ← NO dependency on hashing
  │                                       │
codebase2/codebase-sqlite-hashing-v2 ─────┘      ← the wiring package
  │
parser-typechecker            ← the chokepoint
  │         │           │
unison-runtime  unison-merge  unison-share-api
              │
          unison-cli → unison-cli-main
```

**What holds.** `lib/` never depends upward. `codebase2/codebase-sqlite` depends on no typechecker, no syntax, no hashing — storage is genuinely isolated. `unison-merge` is a pure library with no IO and no storage dependency; its driver lives in the CLI (`unison-cli/src/Unison/Codebase/Editor/HandleInput/Merge2.hs`). Nothing depends back down into `unison-cli`.

**What leaks, honestly stated.** Three edges cut against the clean story:

1. `codebase2/codebase-sqlite` depends on `unison-core1` — the V1 language core. The storage layer is isolated from *hashing* and from the *typechecker*, but not from the language's core types.
2. `unison-hashing-v2` — the package that must never change — itself depends on `unison-core1`, which changes constantly. The freeze is on `unison-hashing-v2`'s own types and instances, not on its dependency surface.
3. `codebase2/codebase-sqlite-hashing-v2` depends on `unison-syntax`, because hashing a namespace requires name-segment syntax. Hashing is not quite below syntax.

4. `parser-typechecker` is the real structural weakness: 37k lines that are simultaneously the typechecker, the printers, the `Codebase` abstraction, the SQLite codebase implementation, and the builtin definitions. It is where "language" and "storage" get glued, and it is depended on by runtime, merge, share-api, and cli alike. Several modules in it (`Unison.Codebase.SqliteCodebase`, `Unison.Builtin`) plainly belong elsewhere by the project's own layering logic.

**The observation for arbor.** `../../design/06-architecture.md`'s four layers map onto Unison's graph well enough to be a useful check, but not cleanly:

| arbor layer | Unison |
|---|---|
| **Store** | `codebase2/codebase-sqlite` + `codebase2/codebase` + `codebase2/core` |
| **Attachment** (aspects, namespace) | Split — namespace is in the Store layer (`Branch` is stored, and hashed); aspects have no home because the mechanism was removed (`05-deletions.md`) |
| **Language** | `unison-core1` + `unison-syntax` + most of `parser-typechecker` + `unison-runtime` |
| **Interface** | `unison-cli` + `unison-share-api` |

Two mismatches are informative. First, **arbor's Attachment layer has no Unison counterpart** — the namespace sits *inside* storage (it is a hashed object like any other), and the aspect store does not exist. Second, `unison-merge` and `Update2.hs` do not fit any of the four: they are algorithms over the namespace that need the typechecker, so they end up above Language and below Interface. arbor's `p11` `update_strategy.re` and `follow_clean.re` occupy the same unlabeled position, and `../../design/06-architecture.md` does not name it.

## Conventions worth stealing

- **hpack + `hie.yaml`.** Generated cabal files and a machine-readable component map. Cheap.
- **Orphan-instance quarantine.** `lib/orphans/` holds five tiny packages that exist *only* to carry orphan instances (`unison-hash-orphans-sqlite`, `unison-core-orphans-sqlite`, …), keeping core packages free of them. `lib/orphans/README.md` explains why.
- **Weeder.** `weeder.toml` (46 KB) tracks dead exports across the whole tree, run by `scripts/check-weeds`. A 161k-line codebase that deletes as aggressively as this one (`05-deletions.md`) needs it.
- **Transcripts as the primary regression suite.** ~700 markdown files where the input is a UCM session and the golden output is the recorded transcript. See `06-pl-notes.md` §"Transcripts".
- **`development.markdown`** is purely operational — build, formatting, toolchain updates, Windows gotchas. No design content. The design content is in `docs/`, and much of it is archaeology (`00-overview.md` §"What was read").
