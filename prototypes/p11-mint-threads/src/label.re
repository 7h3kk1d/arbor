/* Label — the identity of a record field, sum constructor, or method
   name. A Label has nothing but its mint mark; the canonical encoding
   is a single tag byte followed by the 16-byte mark.

   Labels live in the Store alongside Term and Type definitions and are
   bound to human-readable names through the namespace, exactly as for
   terms and types. Two labels with identical mint marks are equal;
   freshly minting two labels with the same name in different contexts
   produces distinct hashes by design — that's the whole point of
   promoting labels to a minted sort.

   Field identity inside a record type is the Label's hash. Renaming the
   namespace binding leaves the Label's hash unchanged, so record types
   referencing it are structurally unaffected. */

[@deriving (eq, ord, show)]
type t = {mint: Mint.t};

let language_tag = 'R';
let sort_tag = 'L';

let create = (mint: Mint.t): t => {mint: mint};

let fresh = (): t => {mint: Mint.fresh()};

let mint = (l: t): Mint.t => l.mint;

let encode = (buf: Buffer.t, label: t): unit => {
  Buffer.add_char(buf, language_tag);
  Buffer.add_char(buf, sort_tag);
  Mint.encode(buf, label.mint);
};

let hash = (label: t): Hash.t => {
  let buf = Buffer.create(2 + Mint.length);
  encode(buf, label);
  Hash.digest_buffer(buf);
};
