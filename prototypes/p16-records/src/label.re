/* A label — the identity of a record field (design/11-label-sort). A label
   carries only its mint mark; its canonical encoding is a sort byte plus the
   mark. Field identity inside a record type/literal is the label's *hash*, never
   its name — so renaming the namespace binding leaves the hash unchanged and
   every record referencing it is structurally unaffected. Two labels minted for
   the same field name in unrelated contexts are distinct by design (that is the
   point of promoting labels to a minted sort).

   Labels live in the Store alongside Term and Type definitions and bind to
   human-readable names through the namespace, exactly as terms and types do.
   Only labels mint in p16; terms and types stay structural. */

type t = Mint.t;

let create = (m: Mint.t): t => m;
let mint = (l: t): Mint.t => l;
let equal: (t, t) => bool = Mint.equal;
let compare: (t, t) => int = Mint.compare;

/* Sort byte 'L' keeps label hashes disjoint from terms ('P') and types ('T'). */
let encode = (l: t): string => {
  let b = Buffer.create(17);
  Buffer.add_char(b, 'L');
  Buffer.add_string(b, l);
  Buffer.contents(b);
};
