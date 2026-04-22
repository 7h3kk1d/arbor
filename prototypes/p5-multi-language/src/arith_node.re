/* Shallow arithmetic AST node — the arith form of what lives in the
   Store (wrapped in Definition.Arith). Children are Hash.t references.
   Node owns its own deterministic encoding and hash primitive.

   New in p5: a one-byte LANGUAGE TAG is prepended to every encoding so
   that arith and lc hashes live in disjoint spaces. Tag byte is 'A'
   (0x41) for arithmetic. Without the prefix, a p3-style arith True
   (`\x01`) and a p4-style lc Var (`\x01...`) would hash through
   structurally similar inputs — the distinct byte lengths still give
   distinct digests, but the prefix makes the separation explicit and
   cheap to audit. */

[@deriving (eq, show)]
type t =
  | True
  | False
  | Zero
  | Succ(Hash.t)
  | Pred(Hash.t)
  | IsZero(Hash.t)
  | If(Hash.t, Hash.t, Hash.t);

let language_tag = 'A';

let tag_true = '\x01';
let tag_false = '\x02';
let tag_zero = '\x03';
let tag_succ = '\x04';
let tag_pred = '\x05';
let tag_iszero = '\x06';
let tag_if = '\x07';

let encode = (buf: Buffer.t, node: t): unit => {
  Buffer.add_char(buf, language_tag);
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
