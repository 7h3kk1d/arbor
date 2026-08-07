# Unison — The store and the hash

**Status:** Source read @ `db60ce2`. See `00-overview.md` for the pin and scope. Everything is `[source-verified @ db60ce2]` unless tagged otherwise.

## The hash function

**SHA3-512, untruncated** — 64 bytes, 512 bits (`unison-hashing-v2/src/Unison/Hashing/V2/Tokenizable.hs:122-137`). No truncation anywhere; grepped for `take`/`truncate` across `lib/unison-hash`, `unison-hashing-v2`, `codebase2/core`, and `unison-core/src/Unison/Hashable.hs` and found none `[verified by absence @ db60ce2]`.

Rendered as **lowercase, unpadded base32hex** (RFC 4648 §7, alphabet `0-9a-v`) — `lib/unison-util-base32hex/src/U/Util/Base32Hex.hs:25-27`. 512 bits is 103 characters, which is why users interact almost exclusively with prefixes, and why the `hash` table's unique index is `COLLATE NOCASE` — so SQLite's `LIKE` optimization turns short-hash lookup into an indexed prefix scan (`codebase2/codebase-sqlite/sql/create.sql:14-26`).

Nothing is hashed as raw bytes. The input is a **tagged, length-prefixed token stream** (`Tokenizable.hs:127-137`): `Tag` is one byte; `Bytes`/`Text` are `word64BE length ++ payload`; `Int` is `int64BE`; `Nat` is `word64BE`; `Hashed` is 64 raw bytes with no length prefix because it is fixed-width. Layers are domain-separated by a leading tag — terms start each layer with `Tag 1`, types with `Tag 0`, decls with `Tag 2` — *"to avoid collisions with terms"* (`Hashing/V2/DataDeclaration.hs:115-118`, `Hashing/V2/Term.hs:147-152`).

**Observation.** arbor does the same thing for the same reason: p17's sort bytes `'P'`/`'T'`/`'L'` disambiguate terms, types, and labels in one hash space. Unison's version is a per-*layer* tag rather than a per-*sort* prefix — every constructor of every AST node contributes a tag, not just the top. That is strictly stronger, and it is what makes `../../design/03-content-addressing.md`'s "one global hash-space" safe across an open-ended set of node constructors.

## Versioning — the answer to "what if the hash changes"

Every hash begins with a version token: `hashingVersion = Tag 2` (`Tokenizable.hs:37-38`). The comment explains the subtlety — bumping the version must change *all* hashes, because otherwise simple values would collide across versions in the single global `hash` table.

Underneath, the schema anticipates multiple hash functions coexisting. `hash_object(hash_id, object_id, hash_version)` is *"a layer of indirection that allows multiple hashes to be associated to the same object. For example, if the hashing algorithm for the object is changed"* (`sql/create.sql:33-49`). Currently every write hardcodes `2` (`U/Codebase/Sqlite/Queries.hs:778`, with a TODO), and `removeHashObjectsByHashingVersion` (`Queries.hs:2271`) exists to retire a generation.

**Correction owed to arbor.** `../../design/03-content-addressing.md` §"Hash representation" says only that the algorithm is not locked in and that *"if we need to change the representation, we accept full state rebuilds."* Unison's answer is that **you never rebuild** — you version the hash function inside its own input, and keep a many-hashes-to-one-object table so old references keep resolving while new ones are minted under the new scheme. That is prior art for a question arbor currently answers with "accept the cost," and it is cheap enough to be worth knowing about before the bootstrap phase ends. It does not change arbor's stance; it changes what the alternative is.

## Components — a hash names a cycle, not a definition

**This is the deepest structural difference from arbor's model.**

Mutually recursive definitions are hashed *together*, as one object, and an individual definition is addressed by a pair:

```haskell
data Reference' t h = ReferenceBuiltin t | ReferenceDerived (Id' h)
type Pos = Word64
-- | @Pos@ is a position into a cycle, as cycles are hashed together.
data Id' h = Id h Pos
```
`codebase2/core/U/Codebase/Reference.hs:77-119`.

So the store is not `Hash ⇀ Node`. It is `Hash ⇀ Node⁺` — `docs/repoformats/v2.markdown` puts it plainly: *"the entire component is identified by a single hash"*, where *"component" ambiguously means the whole cycle, a strongly-connected component of definitions*. A non-recursive definition is a singleton component with `Pos = 0`. Textually: `##Text.take` for builtins, `#<hash>` for position 0, `#<hash>.<n>` beyond it (`unison-core/src/Unison/Reference.hs:146-179`).

### The canonicalization algorithm

`unison-hashing-v2/src/Unison/Hashing/V2/ABT.hs`. Given a set of bindings, `hashComponents` (`:106-134`) requires the whole set to be closed, runs Tarjan SCC to get components in topological order, and for each component substitutes the *already-computed* `Derived h i` references for previously-hashed names before hashing.

The interesting part is ordering a single cycle, `doHashCycle` (`:178-217`). Members must be given indices, but any index assignment presupposes an order, and the order must not depend on how the user wrote them. The trick:

```haskell
permutationEnv = Left names : env    -- ALL cycle members become ONE indistinguishable frame
namedHashes    = second (hash' permutationEnv) <$> namedTerms
(permutedNames, permutedTerms) = zip namedTerms hashes & sortOn snd & fmap fst & unzip
newEnv = map Right permutedNames ++ env   -- now each member gets its own index
```

In `permutationEnv`, every mutually-recursive name collapses into a single `Left [v1..vn]` frame, so `hash'`'s variable case (`:151-158`) resolves *every* intra-cycle reference to the same de Bruijn index. The per-member hashes are therefore independent of the order the members were listed in, and sorting them yields a canonical permutation. Only then is the real environment built, giving each member a distinct index.

Then `hashComponent` (`:74-100`) computes the component hash from tokens **shared by every member** — `commonTokens = Tag 1 : map Hashed hashes` — so each member is disambiguated *only* by its own position, and the overall hash is the accumulation of the sorted member hashes.

### It can fail, and arbor should record that

The algorithm has a real, shipped failure mode. If two cycle members are structurally identical modulo intra-cycle references, their permutation hashes collide and the canonical order is **underdetermined**. Unison detects this and refuses:

```haskell
for_ structurallyEquivalentElements \vs ->
  ([IncompleteElementOrderingError ...], ())
```
`ABT.hs:190-192`, with the error type at `:33-36` pointing at [unisonweb/unison#2787](https://github.com/unisonweb/unison/issues/2787). The user-facing workaround is to perturb one definition — literally, add a dummy binding like `_ = "this is the foo definition"`. The storage layer mirrors it as `HashingFailure` (`codebase2/codebase-sqlite/U/Codebase/Sqlite/HashHandle.hs:33-55`). Internal `let rec` bindings deliberately suppress the warning rather than fail (`ABT.hs:169-174`), because there the ordering is not observable.

**Correction owed to arbor.** `../../design/03-content-addressing.md:141` says: *"When a language introduces mutually recursive definitions, we'll need a canonical ordering for the group so the hash is stable. Unison's approach is a known starting point."* Two things are understated. First, the commitment is larger than an ordering — it changes the *type of the store*, from hash-to-node to hash-to-component, and every reference from a hash to a pair. Second, **the known starting point has a known defect**: canonical ordering of a cycle is not always possible, and the mature implementation's answer is to detect the case and ask the user to break the tie by hand. That is worth writing down before arbor reaches recursion, because it means "canonical form for an SCC" is a *partial* function, not a total one.

## Alpha-equivalence: named ABTs, de Bruijn only at hash time

Unison's abstract binding trees keep names:

```haskell
data ABT f v r = Var v | Cycle r | Abs v r | Tm (f r)
data Term f v a = Term { freeVars :: Set v, annotation :: a, out :: ABT f v (Term f v a) }
```
`codebase2/core/U/Core/ABT.hs:16-25`, crediting Neel Krishnaswami's ABT posts at `:14`. `Eq`/`Ord` are α-equivalence, implemented by renaming aligned `Abs` binders to a common fresh variable (`:49-60`).

De Bruijn indices appear **only during hashing**. `hash'` carries an environment `[Either [v] v]` — binders innermost-first, `Right v` an ordinary binder and `Left [v]` a cycle frame — and a variable hashes as its index into it (`ABT.hs:151-166`). Binder names are discarded on the way in (`Abs'' v t -> hash' (Right v : env) t`); free variables are a hard error. Annotations are excluded by construction: *"We ignore annotations in the `Term`, as these should never affect the meaning of the term"* (`:136-137`). Commutative/unordered subterms are canonicalized by sorting their child hashes (`:167`).

**Observation.** This is the same result as arbor's p4 α-canonicalization but the opposite implementation choice. arbor stores de Bruijn and reconstructs names for display (`p4/src/{ast,canonicalize,surface_ast}.re`); Unison stores names and de-Bruijn-izes to hash. arbor's `docs/prototypes/p4-lambda-calculus/decisions.md:21` already records considering "ABTs á la Unison" and declining for prototype simplicity. Worth noting what the choice actually buys Unison: because names survive into storage, the pretty-printer has the *author's* variable names, not invented ones — which matters a great deal for `04-caching-update-merge.md` §"Update", where correctness depends on printing code a human will recognize. arbor's round-trip is stated up to name choice (`formalism/open-questions.md` §"Round-trip precision", closed 2026-07-30) precisely because it does not keep them.

## Reference transparency — the fork arbor took the other way

```haskell
TermRef (ReferenceDerived h 0) -> Hash.fromByteString (Hash.toByteString h)
```
`unison-hashing-v2/src/Unison/Hashing/V2/Term.hs:135-146`, with the comment: *"this case ensures that references are 'transparent' wrt hash … So for example `x = 1 + 1` and `y = x` hash the same."*

A reference node contributes **literally the referenced hash** to its parent's hash input, at any position, rather than a tagged encoding of its own. Two consequences, and the second is the strong one:

1. A definition whose whole body is a bare reference *is* the referenced definition — `x = 1 + 1` and `y = x` are one object, not two.
2. **Inlining is hash-preserving.** `App(Ref h₁, Ref h₂)` hashes identically to the term with both callees' bodies substituted in, because each `Ref` already contributed exactly what the inlined body would have. Factoring a subterm out into its own definition, or folding it back in, changes nothing about identity.

**Be precise about what this fork is and is not.** It is *not* "Unison has no `Ref` node" — the node exists and is stored, which is why the dependents index can be populated at save time (`03-namespace-and-history.md` §"Primary state vs. derived indices"). It is that in Unison, `Ref(h)` and the definition at `h` are **hash-indistinguishable**, whereas arbor's `nRef(h)` has its own tagged encoding and so hashes differently from what it points at.

`formalism/decisions.md` (2026-07-23, "Abstract hash + indirect `Ref`") reintroduced `Ref` precisely because *"the interesting metatheory … is vacuous without indirection."* That rationale is untouched by this finding — Unison keeps the indirection too. What is at stake is narrower and still real:

- **Hash-transparent (Unison)** — inlining, factoring, and aliasing are all identity-preserving, so an optimizer or a translator may freely move between them. Cost: the store cannot represent the difference between "the author factored this out" and "the author wrote it inline," so that intent is unrecoverable from the store alone.
- **Opaque `Ref` (arbor)** — the shape the author wrote survives into the hash, which is what `../../design/12-type-abstraction.md`'s projection-inlining dependency model needs in order to say that inlining a projection *changes* the dependency structure. Cost: two definitions that differ only by a factoring do not deduplicate, and `thm:alpha` has to be read as "collapses α-equivalence" and not "collapses observational equivalence."

Filed as a question in `formalism/open-questions.md` §"Divergences from Unison"; see `07-arbor-mapping.md` §"Ref".

## The type is inside the term hash

`hashTermComponents` wraps every term in a `TermAnn e typ` node before hashing (`Hashing/V2/Term.hs:92-109`). A `hashTermComponentsWithoutTypes` exists, and `parser-typechecker/src/Unison/Hashing/V2/Convert.hs:87` warns: *"This shouldn't be used when storing terms in the codebase, as it doesn't incorporate the type into the hash."*

So in Unison a definition's identity includes its type. Changing a signature — even to an equivalent one written differently — produces a different definition.

**Contrast with arbor-stlc**, where the type is a *derived aspect* Θ keyed by the term's hash, nothing is gated on types, and typing is observational (`formalism/paper/arbor-stlc.tex`). The two positions are not reconcilable, and each buys something: Unison gets type-changes-are-identity-changes for free and never needs to ask whether a cached type is stale; arbor gets to store ill-typed and partially-typed terms, which is what `p9`'s `Type_with_holes` and the whole holes line require. Worth stating explicitly in the formalism rather than leaving as an unexamined default. See `07-arbor-mapping.md` §"Types".

## Decls get extra normalization

Data declarations are hashed as `Modified modifier (absChain bound (Constructors [ctorTypes]))` (`Hashing/V2/DataDeclaration.hs:51-54`), with constructors **sorted by the hash of their type** (`:87-89`) so declaration order does not affect identity, and `Constructors` itself hashed via `hashCycle`.

`Modifier` is the minting switch, and its tokenization is the whole mechanism:

```haskell
data Modifier = Structural | Unique Text
instance Hashable.Tokenizable Modifier where
  tokens Structural  = [Hashable.Tag 0]
  tokens (Unique txt) = [Hashable.Tag 1, Hashable.Text txt]
```
`Hashing/V2/DataDeclaration.hs:29`, `:130-132`. A mint is a `Text` GUID hashed into the declaration alongside its structure — arbor's `10-minted-identity.md` model exactly. See `04-caching-update-merge.md` §"Minting" for where the GUID comes from and what it costs.

A known limitation is documented rather than fixed: results differ if a constructor has the same fully-qualified name as one of the types (`:71-74`, *"TODO: assert this and bomb if not satisfied"*).

## On disk: localization, and why blobs are codebase-independent

The `object` table stores blobs; the `hash` and `text` tables are the only places hashes and strings live, both interned. Object references inside a blob are **not** database ids. They are offsets into a small per-object lookup table:

```haskell
data LocalIds' t h = LocalIds { textLookup :: Vector t, defnLookup :: Vector h }
```
`codebase2/codebase-sqlite/U/Codebase/Sqlite/LocalIds.hs:14-17`. The rationale, from `LocalizeObject.hs:4-27`:

> Localization is a stateful process in which the real database identifiers contained within an object … are canonicalized as local identifiers counting up from 0 **in the order they are encountered in the object**.

Two payoffs, and the second is the point. The blob is smaller (varint offsets, each distinct text stored once per object). And **the blob is codebase-independent** — the only codebase-specific data has been hoisted into the header vectors, so the same object serializes to identical bytes in every codebase. Syncing a definition between codebases rewrites the header and copies the body untouched (`decompose*`/`recompose*`, `Serialization.hs:824-831`). `docs/repoformats/v2.markdown` works this through with a full example.

Self-reference inside a component is encoded as absence: `TermRef = Reference' LocalTextId (Maybe LocalDefnId)`, where `Nothing` means "this component" (`Term/Format.hs:20-23`). That is how a blob refers to itself without knowing its own hash; `unhashComponent` (`codebase2/codebase/U/Codebase/Term.hs:265-324`) turns those back into fresh variables on read.

**Observation.** This is the concrete form of `../../design/03-content-addressing.md`'s "no cross-language references" cousin: a stored object that mentions nothing outside itself except by content. arbor gets it for free today because prototype stores are in-memory and hash-keyed throughout. It becomes a real design question the moment there is a wire format, and Unison's answer — intern per-object, not per-database — is the non-obvious part.

## Where the two-hash-scheme wrinkle is

There is a **second, unrelated** hashing typeclass: `unison-core/src/Unison/Hashable.hs`, also SHA3-512 but *without* the version tag (`:106-109`), whose documentation warns (`:48-53`) that it is *"meant only to be used as a utility when hash-based identities are useful in algorithms, the runtime, etc."* It backs merge's syntactic hashes (`04-caching-update-merge.md` §"Merge") and the runtime's ANF rehashing. Worth knowing so that "Unison's hash" is never ambiguous: the content-addressing hash is `Unison.Hashing.V2` and nothing else.
