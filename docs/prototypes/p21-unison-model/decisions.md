# p21 — Decisions (ADR-lite)

Append-only. Reversals get new dated entries. Substrate-design commitments that generalize beyond
this prototype migrate to `docs/design/`; entries here are prototype-scoped.

Note that p21 is a *model of another system*. Most decisions below are "be faithful", and the ones
worth recording are the places where being faithful required a choice the source does not force.

---

### 2026-09-02 — Faithful to Unison, including where arbor disagrees

**Decision.** Implement Unison's model as `mini-unison.tex` states it, including the parts arbor has
recorded the opposite position on: the commit gate, the relation-valued namespace, the type inside
the hash, name-dependent component identity, render-and-reparse migration, and evict-on-failure.

**Why.** The alternative — take the component hashing and keep arbor's permissive ingest and
single-valued `N` — produces a nicer editor and a much weaker experiment. The point is to find out
what Unison's choices cost by paying them, and each divergence is a row in the findings table. A
hybrid would have no control group.

---

### 2026-09-02 — Store binder names; do not use de Bruijn

**Decision.** Terms are a named ABT. Binder names are stored and erased only inside `Abt.hash`.

**Why.** Forced by `update`. Step 2 renders every dependent back to source that a human reads and
edits, so the names in it have to be the author's. Every arbor prototype from p4 on stores de Bruijn
indices and invents display names; such a system could state its round trip only up to a choice of
names, and would hand the user back their own code with someone else's variables in it
(`rem:names`). This is the clearest case in the series of a *surface* requirement dictating a
*storage* representation.

---

### 2026-09-02 — SHA3-512, not BLAKE2B

**Decision.** Use SHA3-512 over a tagged, length-prefixed token stream with a leading version token,
via `digestif.ocaml`.

**Why.** It is Unison's, and for once nothing stops us: the pure-OCaml backend has SHA3-512 and works
under js_of_ocaml, which is the constraint that forced `digestif.ocaml` in the first place. Using the
modelled system's own hash function removes a whole class of "is this a real difference or an
artifact of our encoding?" question from the findings. Byte-compatibility with real Unison remains a
non-goal — the surface language's encoding is not modelled — so the hashes still will not match
`ucm`.

---

### 2026-09-02 — Split stanzas by layout in the driver, not in the grammar

**Decision.** `Parse.file` chunks the buffer into stanzas before Menhir sees them: a top-level stanza
begins at column 0, continuation lines are indented. The grammar parses one stanza and stays
newline-insensitive.

**Why.** A newline-insensitive file grammar is genuinely ambiguous — after `> f`, an `IDENT` may
continue the application or begin the next stanza, and Menhir reports the shift/reduce conflict.
Unison solves this with layout, and doing the same keeps the grammar conflict-free (it is) and gives
per-stanza error reporting, which the buffer's live feedback wants anyway. The alternative — a
layout-aware lexer emitting virtual separators — is more faithful in mechanism and buys nothing here.

---

### 2026-09-02 — Every binding needs a signature

**Decision.** A buffer stanza `name = e` without a matching `name : T` is a problem, reported and
not ingested.

**Why.** The language has no inference, so a recursive occurrence's type has to come from somewhere,
and the declared signature is the only candidate: `Ingest.typecheck_group` seeds the context with
every member's declared type before checking any of them. It also keeps `prop:print-parse` honest —
a definition rendered without its signature would not re-typecheck, and the round trip would fail for
a reason that has nothing to do with references. Unison infers, and would not need this.

---

### 2026-09-02 — Live feedback simulates ingest rather than querying it

**Decision.** `Ops.feedback` re-runs parse → resolve → typecheck over the whole buffer on every
keystroke, against a context seeded with the group's declared types. It stores nothing and caches
nothing.

**Why.** Under a commit gate there is nothing else it *can* do: a buffer definition has no hash, so
there is no object to attach feedback to and no key to cache it under. This is worth recording
because it is the concrete cost of the gate, and it is the strongest evidence the prototype produces
for `13-editing-context.md`'s claim that permissive ingest is what makes live editing cheap. The
first version of this function checked each binding in an *empty* context and reported every
co-dependent edit as an unbound variable — a bug the browser pass caught and the native tests did
not, since the tests went through `Ingest` and never through this path.

---

### 2026-09-02 — One commit gesture

**Decision.** The buffer has a single `commit` button, wired to `Update.run`.

**Why.** At `db60ce2`, `add` is an alias for `update` (`InputPatterns.hs:882-896`); `Input.AddI` no
longer exists, merged in 2025. Two buttons would model a distinction the system stopped making. This
also corrects `prop:follow-only`'s description of `add` as the abstention gesture — abstention is
choosing a name nothing depends on, not choosing a different command.
