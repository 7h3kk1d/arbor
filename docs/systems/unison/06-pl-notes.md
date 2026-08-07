# Unison — PL notes outside arbor's current lines

**Status:** Source read @ `db60ce2`. **Notes-depth throughout** — module headers, type signatures, and Unison's own design docs, not implementation bodies. Everything in this file inherits that caveat; the typechecker, runtime, and FFI implementations are `[unread]` (`00-overview.md` §"What was read").

Collected because they are interesting from a PL standpoint, not because arbor should build them. Several point at published work worth adding to `../../related-work/`.

## Abilities — algebraic effects in the type system

Unison's effect system. `a ->{IO} b` is a function requiring the `IO` ability, with the braces conceptually attached to the arrow. `docs/ability-typechecking.markdown` `[design doc]` is the readable account; the implementation is in `parser-typechecker/src/Unison/Typechecker/Context.hs` (3,786 lines) `[unread]`.

The model in four points:

- Ability lists are **sets under union**; a variable in `{e1, e2}` ranges over sets, so `{}` is a genuine empty set rather than a bottom.
- Every subexpression has an **ambient** set, defined as the required abilities of the *nearest enclosing lambda*, plus whatever enclosing handlers eliminate. Calling `f : a ->{e1,e2} b` requires the ambient to be at least `{e1,e2}`.
- The "nearest enclosing lambda" rule has teeth. `foo2 : Text ->{IO} Text ->{} ()` cannot use `IO` in the inner lambda's body, and the doc gives the soundness argument: *"This would be unsound (you could partially apply the function, then obtain a function with a smaller abilities requirement than what it actually used)."*
- **Bare `a -> b` means `a ->{e} b` for an inferred existential `e`** — not `forall e. a ->{e} b` and not `a ->{} b`. The doc is explicit that this replaced an earlier plan: *"I realized it's not sound to do Frank-style effect generalization after typechecking and have a different proposal instead"*, because a function generalized to `a ->{e} b` after the fact might have been passed, inside the body, to something expecting `a ->{} b`.

Display then elides ability variables mentioned only once, on the principle that *"it's okay to eliminate information from an arrow `a ->{e} b` and show that as `a -> b` if the user can use that as an `a ->{e} b` for any choice of `e`"*.

**Relevance.** arbor has no effect story at all, and the `06-multi-language.md` theme notes that Unison offers no multi-language prior art. Two connections are real, though. First, `04-caching-update-merge.md`'s `checkCacheability` exists *because* of abilities: the cache guard is "no arrows in the type," justified by *"top-level definitions can't have effects without a delay."* Effects are what make an evaluation cache need a side condition. Second, `../../related-work/01-content-addressing.md` already records that Unison's own bibliography cites **Lindley, McBride, McLaughlin (POPL 2017), "Do be do be do"** — the Frank system behind this — which is the citation to reach for if arbor ever needs one.

## Kind inference as a separate constraint solver

Not folded into the typechecker. `parser-typechecker/src/Unison/KindInference.hs` plus `KindInference/{Generate,Solve,Error,UVar}.hs` and `KindInference/Constraint/*`. The header sketches it:

> Unison has Type, ->, and Ability kinds
>
> First break all decls into strongly connected components in reverse topological order. Then, for each component, generate kind constraints that arise from the constructors in the decl to discover constraints on the decl vars. These constraints are then given to a constraint solver that determines a unique kind for each type variable. Unconstrained variables are defaulted to kind Type (just like Haskell 98).

The constraint modules carry **provenance** so that a kind error can be attributed to the source construct that induced it.

**Relevance.** `../../../CLAUDE.md` §"Conventions" reserves the word "kind" for the type-theory concept that appears once F-omega is instantiated. This is what that looks like in a real system: a third kind beyond `Type` and `->` (namely `Ability`), SCC decomposition mirroring the term-level component structure from `02-store-and-hashing.md`, Haskell-98-style defaulting, and a separate solver rather than an extension of type inference. Worth revisiting when the F-omega rung arrives.

## Pattern-match coverage as a refinement-type problem

`parser-typechecker/src/Unison/PatternMatchCoverage.hs` and 15 sibling modules, implementing **"Lower Your Guards"** (Peyton Jones et al.) — cited by URL in the header. `Solve.hs` is 1,011 lines; supporting structures include `GrdTree`, `NormalizedConstraints`, `IntervalSet`, and a union-find map `UFMap`.

The algorithm, from the header: desugar a match into a guard tree; annotate leaves with **refinement types** describing the values that reach them, so redundant and inaccessible clauses show up as *uninhabited* refinement types; build up a refinement type describing uncovered values; then find inhabitants of it to show the user concrete missing cases.

A Unison-specific wrinkle stated in the header: *"An inaccessible pattern in unison would be one that performs effects in a guard although the constraints are unsatisfiable. Such a pattern cannot be deleted without altering the program."* Effects make redundancy and inaccessibility genuinely different properties.

**Relevance.** None to the substrate. Recorded because "Lower Your Guards" is a citable paper that `../../related-work/` does not have, and because it is a good example of a language feature whose *implementation* is a constraint solver an order of magnitude larger than the feature looks.

## Doc literals — documentation as a parsed, typed, host-agnostic value

`unison-syntax/src/Unison/Syntax/Parser/Doc.hs`, and the notable property is in its header:

> This is completely independent of the Unison language, and requires a couple parsers to be passed in to then provide a parser for `Doc` applied to any host language.

The doc parser is parameterized over an identifier parser, a code parser, and a termination parser. Docs are not comments: they parse to a tree, they have a type, they are content-addressed like any other value, and — per `05-deletions.md` — they are located by the naming convention `foo.doc` rather than by a metadata link. Embedded code in a doc is real code, evaluated and typechecked.

**Relevance.** This is the strongest surviving example of the thing `05-deletions.md` says Unison retreated from and arbor is building: documentation *is* a first-class content-addressed value there. What changed is only how it is **found** — by name rather than by aspect. Worth holding onto when reading that file, because it means the retreat was about the association mechanism, not about the "everything is a definition" principle.

## Transcripts — golden-file sessions as the primary test suite

~700 markdown files under `unison-src/transcripts/`. A transcript's input is a markdown document with fenced `ucm` and `unison` blocks; the output is the same document with results interleaved, recorded as a golden file. `unison-src/transcripts/idempotent/` holds a large set where input and output must round-trip *identically*. Runner: `unison-cli/src/Unison/Codebase/Transcript/Runner.hs` (parses with CMark and megaparsec).

Two things stand out.

**Tests are written in the user interface, not in Haskell.** A regression is reproduced by pasting a UCM session. The suite is therefore readable by users and doubles as documentation, and it constrains the *interface* — including output wording — rather than internal functions. The cost is visible in the deletion commits of `05-deletions.md`: removing a feature meant deleting its transcripts, which is why those commits are thousands of lines.

**The round-trip suite is a property test for a metatheorem.** `unison-src/transcripts-round-trip/` prints a whole corpus with `edit.new`, re-adds it into a fresh namespace, and asserts the namespace diff is empty — *"This diff should be empty if the two namespaces are equivalent. If it's nonempty, the diff will show us the hashes that differ."* The corpus is `reparses-with-same-hash.u` (must round-trip to the same hash) and `reparses.u` (must merely re-parse). See `04-caching-update-merge.md` §"Update" for why this is load-bearing and `07-arbor-mapping.md` §"Migration" for the arbor connection.

**Relevance.** `../../related-work/11-benchmarks-and-evaluation.md` records that no benchmark exists for program stores or term-manipulation toolkits. This is not a benchmark, but it is an *evaluation instrument* of a shape arbor could adopt cheaply and immediately: a corpus of terms plus the assertion that print-then-parse preserves hashes is a direct executable check of `prop:print-elab`, and arbor's prototypes have neither the corpus nor the check.

## Code shipping by hash

The capability arbor declares out of scope (`../../design/00-overview.md:38`), worth understanding for what is being given up. `[skimmed]`

`unison-runtime/src/Unison/Runtime/Interface.hs` provides `standalone`/`runStandalone` for `unison compile`, with `StoredCache`/`getStoredCache`/`putStoredCache`; serialization lives in `Runtime/ANF/Serialize.hs`, `ANF/Serialize/{CodeV4,ValueV5,Tags}.hs`, and `Runtime/MCode/Serialize.hs`. `docs/distributed-programming-rfc.markdown` and `docs/distributed-garbage-collection.markdown` are the design line `[design doc — likely dormant]`.

The shape of the idea: because every definition is identified by an intrinsic hash and every reference is by hash, a program can be transmitted as *hashes plus whatever the receiver is missing*, and the receiver can ask for exactly the closure it lacks. The sync protocol in `unison-share-api/src/Unison/Sync*.hs` is the deployed version of this for codebases, and `02-store-and-hashing.md` §"localization" is what makes it byte-efficient: blobs are codebase-independent, so only the header is rewritten.

**Relevance.** This is the payoff Unison's whole design is arranged around, and it is worth stating plainly that arbor's non-goal gives up *the* headline application of content addressing. It also sharpens what arbor's own headline application is: `../../related-work/07-commons.md`'s FAIR framing and `../../related-work/08-agentic-vc.md`'s concurrent-agent framing both need "the same hash means the same thing across machines," which is the *precondition* for code shipping even if arbor never ships code.

## The scratch file and slurping — the commit gesture

`unison-cli/src/Unison/Codebase/Editor/{Slurp,SlurpComponent,SlurpResult}.hs`. "Slurping" is deciding which definitions from the scratch `.u` file to actually add or update, given that a file is a soup of bindings with dependencies among themselves and on the codebase. `Slurp.hs` tags each variable as term, type, or constructor, computes what each transitively needs, and partitions into added / updated / blocked / extra.

**Relevance.** This is the same job as p12's *editing context* and the `Opaque`/`Seal`-on-commit machinery: a staging area where the user's text is not yet in the store, and a commit gesture that decides what enters and what that means for existing names. arbor's `../../design/06-architecture.md` puts this in the Interface layer; Unison puts it in the CLI, which agrees. Worth reading properly if arbor's editing layer grows a multi-definition commit.

## LSP and MCP in the same binary

`unison-cli/src/Unison/LSP.hs` plus 25 modules, and `unison-cli/src/Unison/MCP.hs` plus eight. The MCP server exposes typechecking, documentation reading, Unison Share search, and library installation to an agent over stdio, with a `file://unison-guide` resource the server description instructs the client to read first.

**Relevance.** `../../related-work/08-agentic-vc.md` frames arbor around agents as autonomous concurrent users. The observation here is modest but concrete: the closest comparable system already ships an agent-facing interface as a first-class modality alongside the CLI and the LSP, in the same binary and over the same codebase abstraction. That is `../../design/06-architecture.md`'s Interface layer doing exactly what it claims — several modalities over one core — and it is an existence proof that the layering holds up when an agent is one of the modalities.

## Smaller things worth remembering

- **Constructors are not separately addressable** (`03-namespace-and-history.md` §"Referent vs Reference"). A name can denote something that has no hash of its own. arbor will meet this with datatypes.
- **A definition's type is part of its hash** (`02-store-and-hashing.md`), so "same code, different signature" is a different definition. Combined with `checkCacheability`, this means the type is doing identity, caching, and effect-tracking work simultaneously.
- **`unison-core/src/Unison/Hashable.hs` is a second SHA3-512 hash without a version tag**, used only for in-algorithm identity (synhash, ANF rehashing). Two hash schemes in one system, deliberately, with a doc comment explaining which is which.
- **`Causal`'s merge is commutative but not associative**, stated in the algebraic spec at `parser-typechecker/src/Unison/Codebase/Causal/Type.hs:34-35`. A rare case of an implementation documenting the law it does *not* satisfy.
- **`docs/data-types.markdown` is wrong about its own system** — it says structural types are the default when they are not (`04-caching-update-merge.md` §"Minting"). Its second bullet is still interesting: *"If the user writes a structural type where two constructors have the same structure, that's a type error"* — the coincidental-convergence hazard from `../../related-work/01-content-addressing.md` §"Coincidental convergence", handled by rejecting the program rather than by minting.
