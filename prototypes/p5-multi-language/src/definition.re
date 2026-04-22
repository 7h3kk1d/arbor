/* Definition.t — the shallow form that actually lives in the Store.
   First prototype where this is a SUM rather than an alias: one
   constructor per registered language. Adding a third language is one
   new variant here plus per-language modules; the Store and Attachment
   shapes do not change.

   Hash computation is per-language (Arith_node.hash, Lc_node.hash).
   Each language's Node.encode includes a distinct language-tag byte,
   so the two variants hash into disjoint spaces — the no-cross-language
   references rule from docs/design/03-content-addressing.md and
   05-translation.md becomes a cheap-to-check property at the byte
   level. */

type t =
  | Arith(Arith_node.t)
  | Lc(Lc_node.t);

let language = (d: t): string =>
  switch (d) {
  | Arith(_) => "arith"
  | Lc(_) => "lc"
  };

let hash = (d: t): Hash.t =>
  switch (d) {
  | Arith(n) => Arith_node.hash(n)
  | Lc(n) => Lc_node.hash(n)
  };
