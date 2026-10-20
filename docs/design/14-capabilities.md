# Capabilities

**Status:** Goals, written 2026-09-09, revised the same day. Each entry states something a user must be able to do, in terms that stand on their own. These are requirements, not a status report — nothing here says whether it is built.

**Inclusion rule:** only capabilities that follow inevitably from the design. A pleasant consequence that could go either way is not a goal and is not listed.

---

## Reference and naming

### R1. Refer to a definition independently of where it sits in history or in a file tree
A reference denotes one exact term. Not "whatever lives at this path," not "whatever this name means now," not "version 2.1." Someone hands you a reference and you get the term they meant — without their repository, their branch, their lockfile, or their version of anything.

**Closed and open reference.** A term mentions other things, and there are two useful ways to point at it. Both are wanted, and which one you get is something you asked for rather than a default that can change under you.

- **Closed** — the reference fixes the term *together with* everything it mentions, all the way down. It denotes one exact, complete program: what it means does not depend on what any name denotes today, on later edits to anything it uses, or on the recipient having the same bindings you do. This is the form that makes the paragraph above true.
- **Open** — the reference leaves the mentioned names unresolved, so it denotes "this shape, over whatever these names mean where it is used." A snippet, a template, or a definition deliberately parameterized by its surroundings rather than by its arguments — and the form you want when you intend the thing to be re-pointed later.

### R2. Renaming is constant-time, however many definitions use the name
Renaming costs one binding edit, regardless of how many definitions use the name. No dependent is touched, no text is rewritten, and no use of the old name can break. A rename also cannot collide with someone else's concurrent edit to a definition that uses the name — the two touch different things.

### R3. User-visible names are separate from program structure, so the same code can be presented in any human language
A user-visible name is a string attached to a definition, never part of it; the program's structure references definitions and binders directly. So one definition can carry names in several human languages at once, each reader sees the one they asked for, and adding a language is additive — it changes no program's meaning and cannot break anything. This applies to every nameable thing: top-level definitions, record and module fields, and local variables.

---

## Working on incomplete and broken code

### W1. Store work that does not compile, and come back to it later
Stopping mid-thought, handing work over, and coming back to it a week later all require recording something that does not yet work. Broken and half-finished work has a home in the system rather than in an untracked file beside it.

### W2. Type-check and run a program that still has blanks in it
A program with a blank where a subterm should be is still a program: it has a type, the blank has a type, and everything around the blank still runs. You are never required to finish a term before the system will tell you anything about it.

### W3. Change several definitions at once, each checked against the others' new versions
Two functions that call each other, changed at the same time, each checked against the other's new version — with no gesture separating "editing" from "in effect." Making one of the two wrong costs you that one, not the pair.

### W4. See what a change does to definitions you did not edit, before publishing it
You change a tax rate from 5% to 7%. Every figure on your screen that depends on it — an invoice total, a quarterly summary, a chart — shows the new number immediately. Nothing has been published, nobody else's view has changed, and you can close the editor without publishing. The numbers you are looking at were computed with your edit, not with the version everyone else still has.

### W5. Operations report exactly what they broke instead of refusing to run
An operation that breaks things does it, and hands you the exact list of what broke — the "continue the refactor" list — plus a standing query for anything currently broken. You are never blocked by a change you asked for, nothing is silently half-applied, and every definition that is *not* broken keeps working normally next to the ones that are.

### W6. Feedback stays at typing speed regardless of how large the definition or the codebase is
Types, errors, and values appear as you type, on a definition of any size, in a store of any size. Recording results against the content they were computed from covers whole definitions and stops at the boundary of the one being edited — every keystroke is new content, so it is a fresh key — which means the inner loop needs incremental checking of its own and does not inherit it.

---

## Changing definitions, and adopting other people's changes

### C1. Nothing you depend on changes or disappears unless you ask for it
A definition you are using cannot be edited, replaced, or withdrawn by the person who wrote it. There is no yanked release, no rewritten upstream history, and no dependency that starts behaving differently after an update you did not make. Someone publishing a newer version of something *adds* a definition; it does not touch the one you are using.

### C2. Adopt someone else's change one definition at a time
Taking a fix does not require taking everything else that changed alongside it. You can pull in the single definition you want out of a newer version of a dependency, leave the rest as it stands, and end up using definitions that came from several versions of the same source at once. The unit of adoption is the definition — not the package, the release, or the repository — which is the same choice C3 gives you for outgoing changes, in the incoming direction.

### C3. Choose, per change, how far it propagates
Three answers, all first-class: nothing propagates and existing users keep the version they have; everything that uses it is rebuilt; or you pick the sites, one at a time. Scope can also be "the things I currently have open." No region of the system is exempt from this choice or has different rules.

### C4. Find out what a change would break before making it
Given an old version and a new one, get a straight answer to "does everything that uses this still work?" ahead of time, and the list of what does not.

### C5. Track one definition's identity across edits to its content
"Show me the history of this thing" and "update everything that was tracking this" both need an identity that survives a change to the content. That identity is something you assert deliberately — a statement that this term has a continuity worth keeping — not an automatic annotation on everything.

### C6. Use several versions of the same definition at the same time
Two versions of a function, a type, or a whole module coexist, each with its own users, and both work. No resolver, no version solving, no rule that a name may denote only one thing, and no single global choice imposed on everyone.

**Libraries are not special.** This is the same mechanism whether the two versions came from a dependency, from a colleague, or from the definition you edited a minute ago. There is no privileged region of the system where versioning works differently.

---

## Reusing work that has already been done

### N1. We do not have to recompute anything whose inputs have not changed
Types, test results, evaluated values, analyses. A result is recorded against the exact thing it was computed from, so it cannot be stale and there is nothing to invalidate. Editing one definition does not cost you the results for everything near it.

### N2. A result computed on another machine counts as computed here
A type, a test verdict, or a computed value is valid on any machine, not just the one that produced it. "Is this passing?" is answerable by lookup rather than by running anything, including for code you received from someone else.

### N3. Attach arbitrary data to a definition and query it in both directions
Documentation, tests, provenance, review notes, the reasoning behind an edit, a translation, an analysis — all associated with the definition itself and queryable in both directions. 
**Concrete syntax is one of these.** Comments, layout, and the author's chosen formatting are supported as data at their own level, separate from a definition's identity. So they are kept, carried, and reviewable — and a disagreement about any of them is never a disagreement about the program.

### N4. Anything derived can be thrown away and regenerated
Every recorded type, test result, value, and analysis is recomputable from what produced it. So a store that has accumulated more than you want to keep is a question of choosing what to delete, never a question of whether something is recoverable — what is reproducible is known to be reproducible. The only entries that cannot be regenerated are the ones a person asserted, and those are small.

---

## Reviewing and undoing change

### V1. Diff a definition against another version, closed or open
Two different questions, both wanted, matching R1's two kinds of reference.

- **Closed** — did anything at all change in what this computes, anywhere beneath it? Compares the definition together with everything it depends on.
- **Open** — did *this* definition change, holding everything it refers to fixed? This is the question that separates "the author rewrote this" from "something underneath it moved," and it is the one that makes reviewing a change legible.

Both are available for any two versions of a definition, at definition granularity rather than as text differences over files.

### V2. Undo any operation, and never lose work to one
Every operation that changes a binding is recorded, so any of them can be reversed — including one that removed a name you did not ask it to touch. This is what a version-control system's history is for, applied to definitions and bindings rather than to files, and the model to follow is Jujutsu's: the working state is snapshotted continuously and recovery comes from the operation log, so nothing has to have been committed at the right moment in order to be recoverable.

---

## Many languages, many actors, and the environment itself

### P1. Keep definitions from several languages in one store, with explicit translation between them
Several languages coexist in one store, and reuse across them is an explicit translation that produces a real definition in the target language, recorded so it is done once. A translation that cannot be done says so, and the refusal is recorded as an answer rather than as an error to retry.

### P2. Publish an interface and keep its representation private
Hiding a representation is a property of the stored definition, not a convention of whoever compiled it. Anyone who gets the interface gets the guarantee, and two implementations with different internals are usable interchangeably through it.

### P3. Many people and agents can work concurrently without overwriting each other
Concurrent work is additive: nothing is overwritten, two people doing identical work produce one thing rather than a conflict, and starting a new line of work is cheap rather than a copy of everything. An automated agent is just a user that runs unattended and concurrently.

### P4. The editing environment is itself built from definitions in the store
Views, editors, and tools for particular types are definitions in the system, editable from inside it. Editing one produces a new definition and leaves the old one working, so the environment can be changed while it is running without the risk that makes this dangerous elsewhere.

---

## Not promised

- **Automatic migration.** A change to a definition does not update its users behind your back; C1 is a choice you make, not a default that happens.
- **Semantic deduplication.** Two definitions that compute the same thing by different means are two definitions.
- **Elimination of migration work.** Several versions coexisting (C4) lets you postpone a migration; it does not do it for you.
- **Distribution, effect tracking, and performance targets** are out of scope for the current phase.
