# Capabilities

**Status:** Goals, written 2026-09-09 and revised the same day. Each entry states something a user must be able to do, in terms that stand on their own. These are requirements; nothing here says whether a capability is built.

**Inclusion rule:** only capabilities that follow inevitably from the design. A pleasant consequence that could go either way is not listed.

---

## Reference and naming

### R1. Refer to a definition independently of where it sits in history or in a file tree
A reference denotes one exact term. It does not mean "whatever lives at this path," "whatever this name means now," or "version 2.1." Given a reference, you get the term its sender meant, without their repository, branch, lockfile or versions.

**Closed and open reference.** A term mentions other things, so there are two useful ways to point at it. Both are wanted, and you get the one you asked for; neither is a default that can change under you.

- **Closed:** the reference fixes the term together with everything it mentions, transitively. It denotes one complete program whose meaning does not depend on what any name denotes today, on later edits to anything it uses, or on the recipient sharing your bindings. This is the form that makes the paragraph above true.
- **Open:** the reference leaves the mentioned names unresolved, so it denotes "this shape, over whatever these names mean where it is used." This covers snippets, templates and definitions parameterized by their surroundings instead of their arguments, and it is the form you want for something you intend to re-point later.

### R2. Renaming is constant-time, however many definitions use the name
A rename is one binding edit. No dependent is touched, no text is rewritten, and no use of the old name can break. A rename cannot collide with someone else's concurrent edit to a definition that uses the name, because the two touch different things.

### R3. User-visible names are separate from program structure, so the same code can be presented in any human language
A user-visible name is a string attached to a definition, never part of it. Program structure references definitions and binders directly. One definition can carry names in several human languages, each reader sees the one they chose, and adding a language changes no program's meaning and cannot break anything. This holds for every nameable thing: top-level definitions, record and module fields, and local variables.

---

## Working on incomplete and broken code

### W1. Store work that does not compile, and come back to it later
Stopping mid-thought, handing work over and returning a week later all require recording something that does not yet work. Broken and half-finished work lives in the system, not in an untracked file beside it.

### W2. Type-check and run a program that still has blanks in it
A program with a blank in place of a subterm is still a program. It has a type, the blank has a type, and everything around the blank still runs. You never have to finish a term before the system tells you anything about it.

### W3. Change several definitions at once, each checked against the others' new versions
Two mutually recursive functions changed together are each checked against the other's new version, with no gesture separating "editing" from "in effect." Breaking one of them costs you that one, not the pair.

### W4. See what a change does to definitions you did not edit, before publishing it
You change a tax rate from 5% to 7%. Every figure on screen that depends on it (an invoice total, a quarterly summary, a chart) shows the new number immediately. Nothing is published, nobody else's view changes, and you can close the editor without publishing. The figures were computed with your edit, not with the version everyone else has.

### W5. Operations report exactly what they broke instead of refusing to run
An operation that breaks things still runs, and returns the list of what broke (the "continue the refactor" list) plus a standing query for everything currently broken. A change you asked for never blocks you, nothing is silently half-applied, and every definition that is not broken keeps working beside the ones that are.

### W6. Feedback stays at typing speed regardless of how large the definition or the codebase is
Types, errors and values appear as you type, on a definition of any size, in a store of any size. Recording results against the content they came from covers whole definitions but stops at the one being edited: every keystroke is new content and so a fresh key. The inner loop therefore needs its own incremental checking.

---

## Changing definitions, and adopting other people's changes

### C1. Nothing you depend on changes or disappears unless you ask for it
The author of a definition you use cannot edit, replace or withdraw it. There are no yanked releases, no rewritten upstream history, and no dependency that behaves differently after an update you did not make. Publishing a newer version adds a definition and leaves the one you use untouched.

### C2. Adopt someone else's change one definition at a time
Taking a fix does not require taking everything that changed with it. You can adopt one definition from a newer version of a dependency, leave the rest, and end up using definitions from several versions of the same source at once. The unit of adoption is the definition, not the package, release or repository. This is C3's choice applied to incoming changes.

### C3. Choose, per change, how far it propagates
Three answers, all first-class: nothing propagates and existing users keep their version; everything that uses it is rebuilt; or you pick the sites one at a time. Scope can also be "the things I currently have open." No region of the system is exempt from this choice or follows different rules.

### C4. Find out what a change would break before making it
Given an old version and a new one, you can ask in advance whether everything that uses it still works, and get the list of what does not.

### C5. Track one definition's identity across edits to its content
"Show me the history of this thing" and "update everything that was tracking this" both need an identity that survives a content change. You assert that identity explicitly, as a statement that this term has a continuity worth keeping. It is not attached to everything automatically.

### C6. Use several versions of the same definition at the same time
Two versions of a function, type or module coexist, each with its own users, and both work. There is no resolver, no version solving, no rule that a name denotes only one thing, and no global choice imposed on everyone.

**Libraries are not special.** The mechanism is the same whether the two versions came from a dependency, a colleague, or your own edit a minute ago. No region of the system handles versioning differently.

---

## Reusing work that has already been done

### N1. We do not have to recompute anything whose inputs have not changed
This covers types, test results, evaluated values and analyses. A result is recorded against the exact thing it was computed from, so it cannot go stale and nothing needs invalidating. Editing one definition does not cost you the results for its neighbours.

### N2. A result computed on another machine counts as computed here
A type, test verdict or computed value is valid on any machine, not only the one that produced it. "Is this passing?" is answered by lookup without running anything, including for code received from someone else.

### N3. Attach arbitrary data to a definition and query it in both directions
Documentation, tests, provenance, review notes, the reasoning behind an edit, a translation, an analysis: all are associated with the definition itself and queryable in both directions.

**Concrete syntax is one of these.** Comments, layout and the author's formatting are data at their own level, separate from a definition's identity. They are kept, carried and reviewable, and a disagreement about them is never a disagreement about the program.

### N4. Anything derived can be thrown away and regenerated
Every recorded type, test result, value and analysis can be recomputed from what produced it. Pruning an overgrown store is a choice of what to delete, never a question of whether something is recoverable, because what is reproducible is known to be. Only entries a person asserted cannot be regenerated, and those are small.

---

## Reviewing and undoing change

### V1. Diff a definition against another version, closed or open
Two questions, both wanted, matching R1's two forms of reference:

- **Closed:** did anything change in what this computes, anywhere beneath it? This compares the definition together with everything it depends on.
- **Open:** did this definition change, holding everything it refers to fixed? This separates "the author rewrote this" from "something underneath moved," and makes a change reviewable.

Both are available for any two versions of a definition, at definition granularity instead of as text differences over files.

### V2. Undo any operation, and never lose work to one
Every operation that changes a binding is recorded, so any of them can be reversed, including one that removed a name you did not ask it to touch. This is version-control history applied to definitions and bindings instead of files. The model is Jujutsu: the working state is snapshotted continuously and recovery comes from the operation log, so nothing has to have been committed at the right moment to be recoverable.

---

## Many languages, many actors, and the environment itself

### P1. Keep definitions from several languages in one store, with explicit translation between them
Several languages coexist in one store. Reuse across them is an explicit translation that produces a real definition in the target language and is recorded so it runs once. A translation that cannot be done says so, and the refusal is recorded as an answer, not as an error to retry.

### P2. Publish an interface and keep its representation private
Hiding a representation is a property of the stored definition, not a convention of whoever compiled it. Anyone who has the interface has the guarantee, and two implementations with different internals are interchangeable through it.

### P3. Many people and agents can work concurrently without overwriting each other
Concurrent work is additive. Nothing is overwritten, two people doing identical work produce one thing instead of a conflict, and starting a new line of work is cheap, not a copy of everything. An automated agent is a user that runs unattended and concurrently.

### P4. The editing environment is itself built from definitions in the store
Views, editors and tools for particular types are definitions in the system, editable from inside it. Editing one produces a new definition and leaves the old one working, so the environment can change while running without the usual risk.

---

## Not promised

- **Automatic migration.** A change does not update its users behind your back. Propagation is a choice you make (C1, C3), not a default.
- **Semantic deduplication.** Two definitions that compute the same thing by different means are two definitions.
- **Elimination of migration work.** Coexisting versions (C6) let you postpone a migration; they do not perform it.
- **Distribution, effect tracking and performance targets** are out of scope for the current phase.
