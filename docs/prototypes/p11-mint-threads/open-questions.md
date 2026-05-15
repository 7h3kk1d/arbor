# p11 open questions

Running list. Items graduate to `decisions.md` when resolved.

- **Rebind-preserves-mint as a default.** p11 commits to explicit-gesture only. Does that hold up after the bootstrap demo, or do users want rebind under the same name to imply "version of"?
- **`follow-clean:v1` cache value.** Worth storing, or cheap enough to recompute every Follow attempt?
- **Migrate dialog preview.** Should the dialog show a diff of the cascade output before commit, or is the per-site list enough?
- **Orphan accumulation.** At what scale does the namespace tree become unusable? Is the latest-only toggle sufficient, or does the tree need a thread-grouping mode?
- **Per-site toggles vs. rules.** Migrate is per-site only in v1. At what graph size does that become impractical and a rule filter (e.g., "all callers under `Math.*`") become necessary?
- **Mint thread surfacing.** Detail-pane section is the current home — is that prominent enough, or does the namespace tree itself want a thread-grouping mode?
- **Cross-sort `edit_of` error UI.** Rejected at the Store boundary; what should the editor surface?
- **History annotations.** Aspects on history rows (per `04-naming-layer.md` §"Update strategies") — when does the prototype actually want them?
- **`follow-clean` storage vs. session-only.** Bubbles up to `open-questions.md` §"Update strategies".
- **Tombstone unbind rows.** Are `(None, ts)` rows useful in practice, or is a plain truncation simpler?
