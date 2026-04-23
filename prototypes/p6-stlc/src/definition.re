/* Definition.t — the shallow form that actually lives in the Store.
   A sum type: one constructor per registered language. Adding a third
   language is one new variant here plus per-language modules; the Store
   and Attachment shapes do not change.

   p6's pair is untyped Lc (p4/p5) and STLC (TAPL Ch. 8+9 — pure λ→ over
   Bool with native true/false/if, every lambda annotated).

   Hash computation is per-language (Lc_node.hash, Stlc_node.hash). Each
   language's Node.encode includes a distinct language-tag byte ('L' for
   lc, 'S' for stlc), so the two variants hash into disjoint spaces —
   the no-cross-language-references rule from
   docs/design/03-content-addressing.md and 05-translation.md becomes a
   cheap-to-check property at the byte level. */

type t =
  | Lc(Lc_node.t)
  | Stlc(Stlc_node.t);

let language = (d: t): string =>
  switch (d) {
  | Lc(_) => "lc"
  | Stlc(_) => "stlc"
  };

let hash = (d: t): Hash.t =>
  switch (d) {
  | Lc(n) => Lc_node.hash(n)
  | Stlc(n) => Stlc_node.hash(n)
  };
