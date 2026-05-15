# p11 decisions

ADR-lite log; append-only. Reversals are new entries.

## 2026-05-12: Fork from p10 (not p9)

Forking from p10 carries the mint-by-default substrate, the label sort, and records/tuples/lists forward. p11's new machinery (mint threads, binding history, update strategies) needs definitions with real mint marks already in place to act on; p9 would have meant introducing mint marks and edit-tracking at once.

## 2026-05-12: Explicit edit-of gesture, not rebind-preserves-mint

Plain rebind under an existing name still mints a fresh thread. Mint preservation requires the explicit "edit of X" button. The substrate stays neutral: the gesture is the user's signal that this is a version, not a different thing. Per `docs/design/10-minted-identity.md` §"Marks that survive content edits" working synthesis ("intentional, not automatic accumulated lineage").

## 2026-05-12: Three strategies in v1 (Pin, Follow, Explicit migrate)

All three are implemented on the same three primitives (`callers_of`, `multi_rebind`, `follow-clean`); skipping any one of them leaves a hole in the demo. Pin is the resting default (matches p10 today).

## 2026-05-12: Binding history surfaced first-class in UI

History becomes load-bearing for orphan rendering (`name(vN)`) — not merely a debugging convenience. Per `docs/design/04-naming-layer.md` §"Update strategies": rendering orphans as bare hashes is hostile.

## 2026-05-12: `follow-clean:v1` pair-keyed via synthetic key

Key `(h_old, h_new)` encoded as `BLAKE2B(h_old || h_new)` and stored under that synthetic single-hash key in the existing aspect store. Lighter touch than changing the aspect key schema. If the cache proves valuable across prototypes, the schema change becomes load-bearing — bubble up to `02-definitions-and-derived-data.md` then.

## 2026-05-12: Cross-sort `edit_of` rejected at Store boundary

A Term cannot be edited into a Type and vice-versa; `Store.edit_of` returns an error rather than silently allowing it. Mint threads are intra-sort by construction.

## 2026-05-12: Language tag byte 'Q' → 'R'

Hash space deliberately incompatible with p10. Same posture as every prior prototype bump.
