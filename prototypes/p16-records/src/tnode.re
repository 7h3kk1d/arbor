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
    })
  /* System-F (p13): a de Bruijn type variable (counts Forall binders outward)
     and the universal type. `Forall(body)`'s body is a type with one more type
     variable in scope; de Bruijn ⇒ alpha-equivalence of ∀ is automatic. */
  | TVar(int)
  | Forall(Hash.t)
  /* Existential (p14): `Exists(body)`'s body is a type with one more type
     variable in scope — the hidden witness. de Bruijn, like Forall. */
  | Exists(Hash.t)
  /* p16: `Abstract(mint)` is a minted abstract type constant — the type
     extracted by opening an existential (a Skolem; no witness, never unfolds).
     `List(elem)` is a homogeneous list type. */
  | Abstract(Mint.t)
  | List(Hash.t);

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
  | TVar(i) =>
    Buffer.add_uint8(b, 0x15);
    Buffer.add_int64_be(b, Int64.of_int(i));
  | Forall(body) =>
    Buffer.add_uint8(b, 0x16);
    Buffer.add_string(b, body);
  | Exists(body) =>
    Buffer.add_uint8(b, 0x17);
    Buffer.add_string(b, body);
  | Abstract(mint) =>
    Buffer.add_uint8(b, 0x18);
    Buffer.add_string(b, mint);
  | List(elem) =>
    Buffer.add_uint8(b, 0x19);
    Buffer.add_string(b, elem);
  };
  Buffer.contents(b);
};
