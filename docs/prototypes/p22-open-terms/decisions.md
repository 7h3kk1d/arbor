# p22 — Decisions (ADR-lite)

Append-only. Reversals get new dated entries. Substrate-design commitments that
generalize beyond this prototype migrate to `docs/design/`; entries here are
prototype-scoped.

---

### 2026-09-22 — Free variables become positional slots, not labelled ones

**Decision.** An open term's free variables are `Slot(i)`, an index into a
telescope ordered by first occurrence in a left-to-right traversal. No mint, no
label, no name in the hashed bytes.

**Why.** Two readings of "content address over the free variables" were
available. Positional gives maximal sharing — `\n. scale n` and `\n. shrink n`
are one open term — and is recoverable-from: the labelled form is
(template, label vector), while the reverse is not. It is also the form that
makes the open hash a usable answer to "did the author edit this?", because it
is invariant under every dependency change. And it needs no mint, which keeps
the identity of a shape a function of content alone.

**Consequence.** The name the author wrote is not in the open term, so it has to
be kept somewhere else or lost — see the intent table.

---

### 2026-09-22 — The link's hash covers targets; names live in a separate table

**Decision.** `Link.hash` covers arity and targets only. The names the author
wrote for each slot go in `Store.intent`, keyed `(link hash, slot)` and
accumulating a list.

**Why.** Putting names in the hash would return names to stored programs, which
`04-naming-layer.md` forbids and which is half of why Unison's propagation is
name-sensitive. But omitting them from a hashed *record* has its own cost, and
it is exactly the clobber the design doc identifies in Unison's binder names:
two links with identical targets would be one stored object, so one author's
names would silently overwrite another's. A side table accumulating every name
anyone used has neither problem.

**Consequence.** Recorded intent is queryable but not part of identity, which is
what makes `Propagate.relink_one ~via` possible without making propagation
name-dependent.

---

### 2026-09-22 — Close recursive cycles in the link, not in the hasher or the object language

**Decision.** A recursive group is a `Bundle`: an array of (open hash, link)
where a link may contain `Self(i)`. A member is `Def.Member(bundle, i)`, whose
hash is an ordinary opaque hash.

**Why.** `03-content-addressing.md` compares closing in the hasher (Option A,
Unison) against closing in the object language (B1 tuple, B2 record) and asks
for a prototype. Because p22's open terms already abstract their intra-group
references into slots, no member mentions any other, and what is actually cyclic
is only the links. So the fixpoint belongs there. It costs no object-language
machinery at all — no `Fix`, no tuple, no record, no projection, no reduction
rule — where B1, "the cheapest option in machinery", costs a `Fix` plus a
CBV-safe rule.

**Consequence.** A's first cost is avoided: no ordinary reference becomes a pair.
`Link.Def` holds a bare `Hash.t`, and the only index in the system is consumed
inside `Bundle.member_def`.

---

### 2026-09-22 — Binders are de Bruijn, reversing p21

**Decision.** `Open_term` stores no binder names. Display names are invented.

**Why.** p21 stores them because `update` renders stored definitions back to
source that a human edits, so what comes back must carry the author's variable
names (`rem:names`). p22 never renders to propagate, so it can afford
canonicalization — and gets α-equivalence as a property of the *value* rather
than only of the hash.

**Consequence.** `pretty.re` is display-only, and the print-then-parse
round-trip stops being a correctness obligation. Binder names as a per-locale
aspect is now possible but unbuilt.

---

### 2026-09-22 — Only a genuine cycle becomes a bundle

**Decision.** A buffer is decomposed with `Scc.components`; singletons are
committed one at a time in dependency order, and only a nontrivial SCC (or a
self-reference) becomes a `Bundle`.

**Why.** p21 must treat a whole buffer as one binding group because its ingest
is group-at-a-time and gated. Doing the same here would put every definition in
a bundle and give every definition an index, discarding the confinement that is
the point of the previous decision.

**Consequence.** The editor's preview has to walk the same plan the commit path
walks, or it reports outcomes the commit will not produce. It does.

---

### 2026-09-22 — `via` restricts the first hop of a cascade only

**Decision.** `Propagate.follow ~via` filters by recorded intent at the sites
that directly reference the edited definition, and not below.

**Why.** An alias makes a *direct* reference ambiguous — a link holds a hash, and
with two names on that hash the hash cannot say which was meant. Deeper in a
cascade there is nothing to disambiguate: a dependent of a dependent follows a
hash with exactly one meaning, and the name it used for the intermediate is its
own business.

**Consequence.** Carrying `via` all the way down silently truncates the cascade
at depth one. It did, until the three-histories test caught it.

---

### 2026-09-22 — BLAKE2B, returning to the arbor line

**Decision.** `Hash.acc` is BLAKE2B, keeping p21's token discipline (tagged,
length-prefixed, version-prefixed).

**Why.** p21 used SHA3-512 because it models Unison and the point was to run
Unison's hash rather than resemble it. p22 is on the arbor line. The input
discipline is kept because it is good independently of whose it is.
