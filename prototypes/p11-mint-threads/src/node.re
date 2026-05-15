/* Shallow AST node — what actually lives in the Store. Children are
   Hash.t references; the full tree is reconstructed by walking the Store.
   Node owns its own deterministic encoding and the hash-of-node primitive.

   Language tag byte is 'R' (0x52) in p11 — distinct from p10's 'Q' so the
   two prototypes' hash spaces don't accidentally overlap. Single
   language for p11 still, but the prefix gives us room.

   Lam carries a type *hash* rather than an inline Ty.t. The Store
   registers the lambda's parameter type as a `Definition.Type` first
   and embeds the resulting hash here. Two concrete consequences:
   structurally-equal types share a hash everywhere they appear, and a
   namespace-bound type alias `type Vector = Int -> Int` produces the
   same Lam hash whether the source said `\x: Vector. body` or
   `\x: Int -> Int. body`.

   Hole is a leaf with no payload. Its hash is the constant
   BLAKE2B('R' ++ tag_hole) — every hole in the Store collides on that
   one entry, just as in p9.

   Tag bytes are stable: do not renumber without invalidating every
   stored hash. */

[@deriving (eq, show)]
type t =
  | Var(int)
  | Int_lit(int)
  | Bool_lit(bool)
  | String_lit(string)
  | Lam(Hash.t /* type */, Hash.t /* body */)
  | App(Hash.t, Hash.t)
  | Let(Hash.t, Hash.t)
  | If(Hash.t, Hash.t, Hash.t)
  | Pair(Hash.t, Hash.t)
  | Fst(Hash.t)
  | Snd(Hash.t)
  | Tuple(list(Hash.t))              /* p10: n-ary positional */
  | List_lit(list(Hash.t))            /* p10: [a, b, c] */
  | Record_lit(list((Hash.t /* label */, Hash.t /* value */)))
  | Record_update(Hash.t /* target */, list((Hash.t, Hash.t)))
  | Project_field(Hash.t /* target */, Hash.t /* label */)
  | Project_index(Hash.t /* target */, int)
  | Prim(Surface_ast.prim_op, list(Hash.t))
  | Prim_call(string /* primitive id */, list(Hash.t))
  | Hole;

let language_tag = 'R';

let tag_var = '\x01';
let tag_int_lit = '\x02';
let tag_bool_lit = '\x03';
let tag_string_lit = '\x04';
let tag_lam = '\x05';
let tag_app = '\x06';
let tag_let = '\x07';
let tag_if = '\x08';
let tag_pair = '\x09';
let tag_fst = '\x0a';
let tag_snd = '\x0b';
let tag_prim = '\x0c';
let tag_hole = '\x0d';
let tag_prim_call = '\x0e';
let tag_tuple = '\x0f';
let tag_list_lit = '\x10';
let tag_record_lit = '\x11';
let tag_record_update = '\x12';
let tag_project_field = '\x13';
let tag_project_index = '\x14';

let prim_tag =
  fun
  | Surface_ast.Add => '\x21'
  | Surface_ast.Sub => '\x22'
  | Surface_ast.Mul => '\x23'
  | Surface_ast.Div => '\x24'
  | Surface_ast.Mod => '\x25'
  | Surface_ast.And => '\x26'
  | Surface_ast.Or => '\x27'
  | Surface_ast.Not => '\x28'
  | Surface_ast.Concat => '\x29'
  | Surface_ast.Eq => '\x2a';

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

let encode_string = (buf: Buffer.t, s: string): unit => {
  encode_int64(buf, String.length(s));
  Buffer.add_string(buf, s);
};

let encode_bool = (buf: Buffer.t, b: bool): unit =>
  Buffer.add_char(buf, b ? '\x01' : '\x00');

let encode = (buf: Buffer.t, node: t): unit => {
  Buffer.add_char(buf, language_tag);
  let write_child = (h: Hash.t) => Buffer.add_string(buf, h);
  switch (node) {
  | Var(k) =>
    Buffer.add_char(buf, tag_var);
    encode_int64(buf, k);
  | Int_lit(n) =>
    Buffer.add_char(buf, tag_int_lit);
    encode_int64(buf, n);
  | Bool_lit(b) =>
    Buffer.add_char(buf, tag_bool_lit);
    encode_bool(buf, b);
  | String_lit(s) =>
    Buffer.add_char(buf, tag_string_lit);
    encode_string(buf, s);
  | Lam(ty_h, body_h) =>
    Buffer.add_char(buf, tag_lam);
    write_child(ty_h);
    write_child(body_h);
  | App(f, a) =>
    Buffer.add_char(buf, tag_app);
    write_child(f);
    write_child(a);
  | Let(rhs, body) =>
    Buffer.add_char(buf, tag_let);
    write_child(rhs);
    write_child(body);
  | If(c, t, e) =>
    Buffer.add_char(buf, tag_if);
    write_child(c);
    write_child(t);
    write_child(e);
  | Pair(a, b) =>
    Buffer.add_char(buf, tag_pair);
    write_child(a);
    write_child(b);
  | Fst(a) =>
    Buffer.add_char(buf, tag_fst);
    write_child(a);
  | Snd(a) =>
    Buffer.add_char(buf, tag_snd);
    write_child(a);
  | Prim(op, args) =>
    Buffer.add_char(buf, tag_prim);
    Buffer.add_char(buf, prim_tag(op));
    encode_int64(buf, List.length(args));
    List.iter(write_child, args);
  | Prim_call(id, args) =>
    Buffer.add_char(buf, tag_prim_call);
    encode_string(buf, id);
    encode_int64(buf, List.length(args));
    List.iter(write_child, args);
  | Tuple(items) =>
    Buffer.add_char(buf, tag_tuple);
    encode_int64(buf, List.length(items));
    List.iter(write_child, items);
  | List_lit(items) =>
    Buffer.add_char(buf, tag_list_lit);
    encode_int64(buf, List.length(items));
    List.iter(write_child, items);
  | Record_lit(fields) =>
    /* Canonical encoding: sort fields by label hash so literal-order
       doesn't perturb the resulting hash. */
    let sorted =
      List.sort(((h1, _), (h2, _)) => Hash.compare(h1, h2), fields);
    Buffer.add_char(buf, tag_record_lit);
    encode_int64(buf, List.length(sorted));
    List.iter(
      ((label_h, value_h)) => {
        write_child(label_h);
        write_child(value_h);
      },
      sorted,
    );
  | Record_update(target, fields) =>
    /* Sort fields by label hash for canonical form. The target is
       written first since its hash is independent of label order. */
    let sorted =
      List.sort(((h1, _), (h2, _)) => Hash.compare(h1, h2), fields);
    Buffer.add_char(buf, tag_record_update);
    write_child(target);
    encode_int64(buf, List.length(sorted));
    List.iter(
      ((label_h, value_h)) => {
        write_child(label_h);
        write_child(value_h);
      },
      sorted,
    );
  | Project_field(target, label_h) =>
    Buffer.add_char(buf, tag_project_field);
    write_child(target);
    write_child(label_h);
  | Project_index(target, i) =>
    Buffer.add_char(buf, tag_project_index);
    write_child(target);
    encode_int64(buf, i);
  | Hole => Buffer.add_char(buf, tag_hole)
  };
};

let hash = (node: t): Hash.t => {
  let buf = Buffer.create(16);
  encode(buf, node);
  Hash.digest_buffer(buf);
};

/* Term-side children only. Lam's type-hash is intentionally excluded:
   it points at a `Definition.Type`, which lives in the Store but is
   not part of the term DAG that derived aspects (eval, has-holes)
   walk. has-holes specifically benefits — types are hole-free in p9,
   so recursing into them would always return false. */
let children = (node: t): list(Hash.t) =>
  switch (node) {
  | Var(_)
  | Int_lit(_)
  | Bool_lit(_)
  | String_lit(_)
  | Hole => []
  | Lam(_ty, body) => [body]
  | Fst(body)
  | Snd(body) => [body]
  | App(a, b)
  | Let(a, b)
  | Pair(a, b) => [a, b]
  | If(a, b, c) => [a, b, c]
  | Tuple(items)
  | List_lit(items) => items
  | Record_lit(fields) =>
    /* Label children are themselves stored Definitions (Labels) and
       are part of the DAG, so they count. */
    List.concat_map(((l, v)) => [l, v], fields)
  | Record_update(target, fields) =>
    [target, ...List.concat_map(((l, v)) => [l, v], fields)]
  | Project_field(target, label_h) => [target, label_h]
  | Project_index(target, _) => [target]
  | Prim(_, args)
  | Prim_call(_, args) => args
  };

let is_value =
  fun
  | Lam(_, _)
  | Int_lit(_)
  | Bool_lit(_)
  | String_lit(_) => true
  | Pair(_, _) => true
  | Tuple(_) => true
  | List_lit(_) => true
  | Record_lit(_) => true
  | _ => false;
