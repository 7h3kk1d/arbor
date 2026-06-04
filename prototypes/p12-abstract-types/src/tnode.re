/* Content-addressed types (design/03 §"Hashing types as well as terms").
   A type definition's content is its encoding; compound types reference their
   component type definitions *by hash* (sharing + namespace-level aliasing).

   `Opaque{mint, witness}` is the abstract type (design/12): the mint gives
   distinctness (Counter over Int != Celsius over Int), the witness hash gives
   soundness (a representation-type change moves the abstract type's identity).
   The witness is reachable but is only unfolded by the checker when the opaque
   is in the current open set. */

type t =
  | Int
  | Bool
  | Product(Hash.t, Hash.t)
  | Arrow(Hash.t, Hash.t)
  | Opaque({
      mint: Mint.t,
      witness: Hash.t,
    });

/* Encoding starts with sort byte 'T' so type and term ('P') hashes never
   collide. Hashes are fixed-width hex, so concatenation is self-delimiting. */
let encode = (t: t): string => {
  let b = Buffer.create(96);
  Buffer.add_char(b, 'T');
  switch (t) {
  | Int => Buffer.add_uint8(b, 0x10)
  | Bool => Buffer.add_uint8(b, 0x11)
  | Product(h1, h2) =>
    Buffer.add_uint8(b, 0x12);
    Buffer.add_string(b, h1);
    Buffer.add_string(b, h2);
  | Arrow(h1, h2) =>
    Buffer.add_uint8(b, 0x13);
    Buffer.add_string(b, h1);
    Buffer.add_string(b, h2);
  | Opaque({mint, witness}) =>
    Buffer.add_uint8(b, 0x14);
    Buffer.add_string(b, mint);
    Buffer.add_string(b, witness);
  };
  Buffer.contents(b);
};
