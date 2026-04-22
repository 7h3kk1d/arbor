/* Shallow AST node — what actually lives in the Store. Children are
   Hash.t references; the full tree is reconstructed by walking the
   Store. Node owns its own deterministic encoding and the hash-of-node
   primitive, since both are intrinsic to what "this node" means. */

[@deriving (eq, show)]
type t =
  | Var(int)
  | Lam(Hash.t)
  | App(Hash.t, Hash.t);

/* Tag bytes. Stable; do not renumber without invalidating every stored
   hash. Matches p3's one-byte-tag scheme. */
let tag_var = '\x01';
let tag_lam = '\x02';
let tag_app = '\x03';

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
