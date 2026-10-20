/* Hash — BLAKE2B over a tagged, length-prefixed token stream.
 *
 * The hash function is p1-p17's; the INPUT DISCIPLINE is p21's, kept because it
 * is a good one independently of whose it is: every token is tagged, every
 * variable-length payload is length-prefixed, digests are written raw because
 * they are fixed width, and every stream begins with a version token so that
 * changing the scheme changes every hash.
 *
 * p21 used SHA3-512 because it models Unison and the point was to run Unison's
 * model rather than to resemble it. p22 is on the arbor line, so it returns to
 * the line's hash. digestif.ocaml (the pure OCaml backend) is required either
 * way: the C path fails under js_of_ocaml at runtime. */

type t = string; /* lowercase hex digest */

/* A token. `Hashed` carries an already-computed digest and is written raw, with
   no length prefix, because digests are fixed width and therefore
   self-delimiting. */
type token =
  | Tag(int) /* one byte */
  | Nat(int) /* 64-bit big-endian */
  | Text(string) /* 64-bit big-endian length, then the payload */
  | Hashed(t);

/* Bumping this changes every hash in the store. */
let hashing_version: token = Tag(1);

let put_u64_be = (buf: Buffer.t, n: int): unit => {
  /* OCaml's int is 63-bit; Unison's Nat is 64. The prototype's literals stay
     well inside the range, and the encoding is the full eight bytes either
     way, so the width of the stream matches. */
  for (i in 7 downto 0) {
    Buffer.add_char(buf, Char.chr(n lsr (i * 8) land 0xff));
  };
};

let put_token = (buf: Buffer.t, tok: token): unit =>
  switch (tok) {
  | Tag(b) => Buffer.add_char(buf, Char.chr(b land 0xff))
  | Nat(n) => put_u64_be(buf, n)
  | Text(s) =>
    put_u64_be(buf, String.length(s));
    Buffer.add_string(buf, s);
  | Hashed(h) => Buffer.add_string(buf, h)
  };

/* acc(tokens) — the hash of a token list, with the version token prepended. */
let acc = (tokens: list(token)): t => {
  let buf = Buffer.create(128);
  put_token(buf, hashing_version);
  List.iter(put_token(buf), tokens);
  Digestif.BLAKE2B.(digest_string(Buffer.contents(buf)) |> to_hex);
};

let equal: (t, t) => bool = String.equal;
let compare: (t, t) => int = String.compare;

let display_prefix = "#";
let to_string = (h: t): string => display_prefix ++ h;
let short = (~len=10, h: t): string =>
  display_prefix ++ String.sub(h, 0, min(len, String.length(h)));
let pp = (fmt, h: t) => Format.fprintf(fmt, "%s", short(h));
let show = (h: t): string => short(h);
