/* Term nodes (the `Term` sort of a Definition). Inline tree; de Bruijn indices
   for Lam/Let binders (alpha-equivalence by canonicalization). References to
   other definitions are by hash:

   - `Ref(h)` — a reference to a stored definition (a departure from p9's
     inline-at-resolution; lets the criterion-4 dependency story be observable
     and keeps a sealed op's raw body out of its consumers).
   - `Seal{opens, ty, impl}` — the design/12 `open #A in E : T` as a node: a thin
     wrapper whose external type is `ty` (over opaque hashes in `opens`) and
     whose implementation is the definition `impl`. */

type prim_op =
  | Add
  | Sub
  | Mul
  | Eq;

type t =
  | Var(int)
  | Lit(int)
  | BoolLit(bool)
  | Lam(Hash.t, t) /* annotation type-hash, body */
  | App(t, t)
  | Let(t, t) /* rhs, body (body binds the let var) */
  | Pair(t, t)
  | Fst(t)
  | Snd(t)
  | If(t, t, t)
  | Prim(prim_op, list(t))
  | Ref(Hash.t)
  | Seal({
      opens: list(Hash.t),
      ty: Hash.t,
      impl: Hash.t,
    });

let prim_tag = (op: prim_op): int =>
  switch (op) {
  | Add => 0x01
  | Sub => 0x02
  | Mul => 0x03
  | Eq => 0x04
  };

let rec enc = (b: Buffer.t, t: t): unit =>
  switch (t) {
  | Var(i) =>
    Buffer.add_uint8(b, 0x00);
    Buffer.add_int64_be(b, Int64.of_int(i));
  | Lit(n) =>
    Buffer.add_uint8(b, 0x01);
    Buffer.add_int64_be(b, Int64.of_int(n));
  | BoolLit(x) =>
    Buffer.add_uint8(b, 0x02);
    Buffer.add_uint8(b, x ? 1 : 0);
  | Lam(ann, body) =>
    Buffer.add_uint8(b, 0x03);
    Buffer.add_string(b, ann);
    enc(b, body);
  | App(f, x) =>
    Buffer.add_uint8(b, 0x04);
    enc(b, f);
    enc(b, x);
  | Let(rhs, body) =>
    Buffer.add_uint8(b, 0x05);
    enc(b, rhs);
    enc(b, body);
  | Pair(x, y) =>
    Buffer.add_uint8(b, 0x06);
    enc(b, x);
    enc(b, y);
  | Fst(p) =>
    Buffer.add_uint8(b, 0x07);
    enc(b, p);
  | Snd(p) =>
    Buffer.add_uint8(b, 0x08);
    enc(b, p);
  | If(c, th, el) =>
    Buffer.add_uint8(b, 0x09);
    enc(b, c);
    enc(b, th);
    enc(b, el);
  | Prim(op, args) =>
    Buffer.add_uint8(b, 0x0a);
    Buffer.add_uint8(b, prim_tag(op));
    Buffer.add_uint8(b, List.length(args));
    List.iter(enc(b), args);
  | Ref(h) =>
    Buffer.add_uint8(b, 0x0b);
    Buffer.add_string(b, h);
  | Seal({opens, ty, impl}) =>
    Buffer.add_uint8(b, 0x0c);
    Buffer.add_uint8(b, List.length(opens));
    List.iter(o => Buffer.add_string(b, o), opens);
    Buffer.add_string(b, ty);
    Buffer.add_string(b, impl);
  };

let encode = (t: t): string => {
  let b = Buffer.create(128);
  Buffer.add_char(b, 'P');
  enc(b, t);
  Buffer.contents(b);
};
