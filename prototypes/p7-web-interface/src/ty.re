/* Types of the simply-typed λ-calculus, TAPL Ch. 9 as written:
   `T ::= Bool | T → T`. No type variables, no polymorphism.

   Types are syntactic and first-class in hashes — an Stlc_node.Lam
   encoding includes Ty.encode(ty), so `\x:Bool. x` and
   `\x:(Bool→Bool). x` have different hashes.

   canonicalize is the identity today. It exists for symmetry with
   Lc_canonicalize and as a hook for future normalization rules (e.g. a
   later prototype that adds type abbreviations or normalization under
   the arrow would do its work here). */

[@deriving (eq, ord, show)]
type t =
  | Bool
  | Arrow(t, t);

let canonicalize = (t: t): t => t;

let tag_bool = '\x10';
let tag_arrow = '\x11';

let rec encode = (buf: Buffer.t, ty: t): unit =>
  switch (ty) {
  | Bool => Buffer.add_char(buf, tag_bool)
  | Arrow(a, b) =>
    Buffer.add_char(buf, tag_arrow);
    encode(buf, a);
    encode(buf, b);
  };

let hash = (ty: t): Hash.t => {
  let buf = Buffer.create(8);
  /* Include a distinctive prefix so Ty hashes cannot collide with
     Definition hashes (which start with language-tag bytes 'L' or
     'S'). */
  Buffer.add_char(buf, 'T');
  encode(buf, canonicalize(ty));
  Hash.digest_buffer(buf);
};

/* Pretty-print with right-associated arrows and minimal parens. */
let rec pp_prec = (fmt, ~prec: int, ty: t) =>
  switch (ty) {
  | Bool => Format.fprintf(fmt, "Bool")
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
