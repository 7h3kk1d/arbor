/* Types for p10. Single language, monomorphic, first-class in node
   hashes (via Ty.encode embedded in Node.Lam).

   p9's binary `Product(t, t)` is retained for backward compatibility
   with the still-in-place Pair/Fst/Snd surface constructs (the parser
   is being rewritten in a later slice). p10 introduces four new
   constructors that the new surface syntax will target:

   - `Tuple(list(t))` — n-ary positional. Generalizes binary Product.
   - `Record(list((Hash.t, t)))` — labeled, with content-addressed
     label hashes. Canonical form sorts the field list by label hash
     so renames don't perturb the type hash and `{ x:Int, y:Int }` and
     `{ y:Int, x:Int }` share an encoding.
   - `List(t)` — monomorphic element type.
   - `Named(Hash.t)` — reference to a stored Type definition (alias
     mechanism: a `type Foo = Int` declaration mints a `Named_type`
     wrapping the substructure `Int`'s hash; another reference to
     `Foo` in a type position resolves to a `Named(<that hash>)`,
     which behaves like the underlying type at the substrate level).

   Tag bytes are stable: do not renumber without invalidating every
   stored hash. */

[@deriving (eq, ord, show)]
type t =
  | Int
  | Bool
  | String
  | Arrow(t, t)
  | Product(t, t)
  | Tuple(list(t))
  | Record(list((Hash.t, t)))
  | List(t)
  | Named(Hash.t);

let tag_int = '\x10';
let tag_bool = '\x11';
let tag_string = '\x12';
let tag_arrow = '\x13';
let tag_product = '\x14';
let tag_tuple = '\x15';
let tag_record = '\x16';
let tag_list = '\x17';
let tag_named = '\x18';

/* Canonicalize: sort Record fields by label hash so two literal
   orders produce the same encoding. Recurse into all type children. */
let rec canonicalize = (t: t): t =>
  switch (t) {
  | Int
  | Bool
  | String
  | Named(_) => t
  | Arrow(a, b) => Arrow(canonicalize(a), canonicalize(b))
  | Product(a, b) => Product(canonicalize(a), canonicalize(b))
  | Tuple(ts) => Tuple(List.map(canonicalize, ts))
  | Record(fields) =>
    let canonical_fields =
      List.map(((h, t)) => (h, canonicalize(t)), fields);
    let sorted =
      List.sort(
        ((h1, _), (h2, _)) => Hash.compare(h1, h2),
        canonical_fields,
      );
    Record(sorted);
  | List(t) => List(canonicalize(t))
  };

let encode_varint = (buf: Buffer.t, n: int): unit => {
  /* Stable big-endian 8-byte length, same as Node.encode_int64.
     Keeps the encoding deterministic across platforms. */
  let b = Bytes.create(8);
  for (i in 0 to 7) {
    let shift = (7 - i) * 8;
    Bytes.set(
      b,
      i,
      Char.chr((n lsr shift) land 0xff),
    );
  };
  Buffer.add_bytes(buf, b);
};

let rec encode = (buf: Buffer.t, ty: t): unit =>
  switch (ty) {
  | Int => Buffer.add_char(buf, tag_int)
  | Bool => Buffer.add_char(buf, tag_bool)
  | String => Buffer.add_char(buf, tag_string)
  | Arrow(a, b) =>
    Buffer.add_char(buf, tag_arrow);
    encode(buf, a);
    encode(buf, b);
  | Product(a, b) =>
    Buffer.add_char(buf, tag_product);
    encode(buf, a);
    encode(buf, b);
  | Tuple(ts) =>
    Buffer.add_char(buf, tag_tuple);
    encode_varint(buf, List.length(ts));
    List.iter(t => encode(buf, t), ts);
  | Record(fields) =>
    Buffer.add_char(buf, tag_record);
    encode_varint(buf, List.length(fields));
    List.iter(
      ((label_h, ty)) => {
        Buffer.add_string(buf, label_h);
        encode(buf, ty);
      },
      fields,
    );
  | List(elem) =>
    Buffer.add_char(buf, tag_list);
    encode(buf, elem);
  | Named(h) =>
    Buffer.add_char(buf, tag_named);
    Buffer.add_string(buf, h);
  };

let hash = (ty: t): Hash.t => {
  let buf = Buffer.create(8);
  /* Distinctive prefix so Ty hashes cannot collide with Term hashes
     (which start with the language tag byte 'R'). */
  Buffer.add_char(buf, 'U');
  encode(buf, canonicalize(ty));
  Hash.digest_buffer(buf);
};

/* Pretty-print. Right-associative `->`; `*` for Product binds tighter
   than `->`. Tuple types render `(a, b, c)`; Record types render
   `{ <hash-prefix>: t, ... }` (the resolver/UI substitute names by
   reverse-lookup). List renders `List t`. Named renders the hash
   short-prefix; UI reverse-resolves to a name. */
let rec pp_prec = (fmt, ~prec: int, ty: t) =>
  switch (ty) {
  | Int => Format.fprintf(fmt, "Int")
  | Bool => Format.fprintf(fmt, "Bool")
  | String => Format.fprintf(fmt, "String")
  | Product(a, b) =>
    if (prec >= 2) {
      Format.fprintf(fmt, "(");
    };
    pp_prec(fmt, ~prec=2, a);
    Format.fprintf(fmt, " * ");
    pp_prec(fmt, ~prec=2, b);
    if (prec >= 2) {
      Format.fprintf(fmt, ")");
    };
  | Tuple(ts) =>
    Format.fprintf(fmt, "(");
    let n = List.length(ts);
    List.iteri(
      (i, t) => {
        if (i > 0) {
          Format.fprintf(fmt, ", ");
        };
        pp_prec(fmt, ~prec=0, t);
      },
      ts,
    );
    let _ = n;
    Format.fprintf(fmt, ")");
  | Record(fields) =>
    Format.fprintf(fmt, "{");
    List.iteri(
      (i, (h, t)) => {
        if (i > 0) {
          Format.fprintf(fmt, ", ");
        } else {
          Format.fprintf(fmt, " ");
        };
        Format.fprintf(fmt, "%s: ", Hash.short(h));
        pp_prec(fmt, ~prec=0, t);
      },
      fields,
    );
    Format.fprintf(fmt, " }");
  | List(elem) =>
    if (prec >= 3) {
      Format.fprintf(fmt, "(");
    };
    Format.fprintf(fmt, "List ");
    pp_prec(fmt, ~prec=3, elem);
    if (prec >= 3) {
      Format.fprintf(fmt, ")");
    };
  | Named(h) => Format.fprintf(fmt, "#%s", Hash.short(h))
  | Arrow(a, b) =>
    if (prec >= 1) {
      Format.fprintf(fmt, "(");
    };
    pp_prec(fmt, ~prec=1, a);
    Format.fprintf(fmt, " -> ");
    pp_prec(fmt, ~prec=0, b);
    if (prec >= 1) {
      Format.fprintf(fmt, ")");
    };
  };

let print = (ty: t): string => {
  let buf = Buffer.create(16);
  let fmt = Format.formatter_of_buffer(buf);
  pp_prec(fmt, ~prec=0, ty);
  Format.pp_print_flush(fmt, ());
  Buffer.contents(buf);
};
