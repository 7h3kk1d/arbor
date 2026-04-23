/* Shallow stlc AST node — the stlc form of what lives in the Store
   (wrapped in Definition.Stlc). Children are Hash.t references; the
   Lam binder also carries a Ty.t annotation inline (types are not
   separately content-addressed).

   Language tag byte is 'S' (0x53) — distinct from lc's 'L' — so stlc
   and lc definitions occupy disjoint hash spaces. */

[@deriving (eq, show)]
type t =
  | Var(int)
  | Lam(Ty.t, Hash.t)
  | App(Hash.t, Hash.t)
  | True
  | False
  | If(Hash.t, Hash.t, Hash.t);

let language_tag = 'S';

let tag_var = '\x01';
let tag_lam = '\x02';
let tag_app = '\x03';
let tag_true = '\x04';
let tag_false = '\x05';
let tag_if = '\x06';

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
  Buffer.add_char(buf, language_tag);
  let write_child = (h: Hash.t) => Buffer.add_string(buf, h);
  switch (node) {
  | Var(k) =>
    Buffer.add_char(buf, tag_var);
    encode_int64(buf, k);
  | Lam(ty, h) =>
    Buffer.add_char(buf, tag_lam);
    Ty.encode(buf, Ty.canonicalize(ty));
    write_child(h);
  | App(f, a) =>
    Buffer.add_char(buf, tag_app);
    write_child(f);
    write_child(a);
  | True => Buffer.add_char(buf, tag_true)
  | False => Buffer.add_char(buf, tag_false)
  | If(c, t, e) =>
    Buffer.add_char(buf, tag_if);
    write_child(c);
    write_child(t);
    write_child(e);
  };
};

let hash = (node: t): Hash.t => {
  let buf = Buffer.create(16);
  encode(buf, node);
  Hash.digest_buffer(buf);
};

let is_value =
  fun
  | Lam(_, _)
  | True
  | False => true
  | _ => false;
