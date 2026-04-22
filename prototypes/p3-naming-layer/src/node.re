/* Shallow AST node — what actually lives in the Store. Children are
   Hash.t references; the full tree is reconstructed by walking the
   Store. Node owns its own deterministic encoding and the hash-of-node
   primitive, since both are intrinsic to what "this node" means. */

[@deriving (eq, show)]
type t =
  | True
  | False
  | Zero
  | Succ(Hash.t)
  | Pred(Hash.t)
  | IsZero(Hash.t)
  | If(Hash.t, Hash.t, Hash.t);

/* Tag bytes. Stable; do not renumber without invalidating every stored
   hash. Matches p1's scheme for atoms only (non-atoms diverge because
   p2 hashes child *digests* rather than inline children). */
let tag_true = '\x01';
let tag_false = '\x02';
let tag_zero = '\x03';
let tag_succ = '\x04';
let tag_pred = '\x05';
let tag_iszero = '\x06';
let tag_if = '\x07';

let encode = (buf: Buffer.t, node: t): unit => {
  let write_child = (h: Hash.t) => Buffer.add_string(buf, h);
  switch (node) {
  | True => Buffer.add_char(buf, tag_true)
  | False => Buffer.add_char(buf, tag_false)
  | Zero => Buffer.add_char(buf, tag_zero)
  | Succ(h) =>
    Buffer.add_char(buf, tag_succ);
    write_child(h);
  | Pred(h) =>
    Buffer.add_char(buf, tag_pred);
    write_child(h);
  | IsZero(h) =>
    Buffer.add_char(buf, tag_iszero);
    write_child(h);
  | If(c, thn, els) =>
    Buffer.add_char(buf, tag_if);
    write_child(c);
    write_child(thn);
    write_child(els);
  };
};

let hash = (node: t): Hash.t => {
  let buf = Buffer.create(16);
  encode(buf, node);
  Hash.digest_buffer(buf);
};

/* Shape-level predicates. Evaluator preserves the invariant that
   numeric-shaped values actually are numeric all the way down, so
   callers only need the top constructor to distinguish cases. */
let is_bool_value =
  fun
  | True
  | False => true
  | _ => false;

let is_numeric_top =
  fun
  | Zero
  | Succ(_) => true
  | _ => false;

let is_value_top = n => is_bool_value(n) || is_numeric_top(n);
