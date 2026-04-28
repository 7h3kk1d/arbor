/* Shallow AST node — what actually lives in the Store. Children are
   Hash.t references; the full tree is reconstructed by walking the
   Store. Node owns its own deterministic encoding and the hash-of-node
   primitive, since both are intrinsic to what "this node" means.

   p8 adds `Hole`: an opaque leaf with no payload. Its tag byte is
   '\x04'. The hash of `Hole` is a constant — the BLAKE2B digest of
   the single byte '\x04' — so every hole in the Store collides on
   the same entry and participates in structural sharing as a single
   shared leaf. */

[@deriving (eq, show)]
type t =
  | Var(int)
  | Lam(Hash.t)
  | App(Hash.t, Hash.t)
  | Hole;

/* Tag bytes. Stable; do not renumber without invalidating every stored
   hash. p4 used \x01..\x03; \x04 is p8's addition. */
let tag_var = '\x01';
let tag_lam = '\x02';
let tag_app = '\x03';
let tag_hole = '\x04';

/* Variable indices are encoded big-endian as 8 bytes so that no index
   in a realistic program risks collision with a future expansion. */
let encode_int64 = (buf: Buffer.t, n: int): unit => {
  let b = Bytes.create(8);
  let n64 = Int64.of_int(n);
  for (i in 0 to 7) {
    let shift = (7 - i) * 8;
    Bytes.set(
      b,
      i,
      Char.chr(Int64.to_int(Int64.logand(Int64.shift_right_logical(n64, shift), 0xffL))),
    );
  };
  Buffer.add_bytes(buf, b);
};

let encode = (buf: Buffer.t, node: t): unit => {
  let write_child = (h: Hash.t) => Buffer.add_string(buf, h);
  switch (node) {
  | Var(k) =>
    Buffer.add_char(buf, tag_var);
    encode_int64(buf, k);
  | Lam(h) =>
    Buffer.add_char(buf, tag_lam);
    write_child(h);
  | App(f, a) =>
    Buffer.add_char(buf, tag_app);
    write_child(f);
    write_child(a);
  | Hole =>
    Buffer.add_char(buf, tag_hole);
  };
};

let hash = (node: t): Hash.t => {
  let buf = Buffer.create(16);
  encode(buf, node);
  Hash.digest_buffer(buf);
};

let is_lam =
  fun
  | Lam(_) => true
  | _ => false;
