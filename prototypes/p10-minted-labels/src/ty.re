/* Types for p9. Single language, monomorphic, first-class in node hashes
   (via Ty.encode embedded in Node.Lam). p9 broadens p6's TAPL Ch. 9
   skeleton (Bool | Arrow) to include Int, String, and a Product type to
   support primitives, base literals, and pairs.

   The encoding tag bytes are stable: do not renumber. Adding a new
   constant type is appending a new tag byte; restructuring an existing
   one breaks every stored hash. */

[@deriving (eq, ord, show)]
type t =
  | Int
  | Bool
  | String
  | Arrow(t, t)
  | Product(t, t);

let canonicalize = (t: t): t => t;

let tag_int = '\x10';
let tag_bool = '\x11';
let tag_string = '\x12';
let tag_arrow = '\x13';
let tag_product = '\x14';

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
  };

let hash = (ty: t): Hash.t => {
  let buf = Buffer.create(8);
  /* Distinctive prefix so Ty hashes cannot collide with Term hashes
     (which start with the language tag byte 'Q'). 'U' is one byte
     past 'T' — same idea as p9, distinct space. */
  Buffer.add_char(buf, 'U');
  encode(buf, canonicalize(ty));
  Hash.digest_buffer(buf);
};

/* Pretty-print. Right-associative `->`; `*` for Product binds tighter
   than `->`. Minimal parens. */
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
