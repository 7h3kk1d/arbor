/* Hash — SHA3-512 over a tagged, length-prefixed token stream. (def:tokens)
 *
 * Unlike p1-p17 (BLAKE2B over an ad-hoc byte encoding) this is Unison's actual
 * hash function and Unison's actual input discipline, because the point of p21
 * is to run Unison's model rather than to resemble it. digestif.ocaml (the pure
 * OCaml backend) is required: the C path fails under js_of_ocaml at runtime.
 *
 * Nothing is hashed as raw bytes. Every stream begins with a version token, so
 * that changing the hash function changes every hash (rem:version). */

type t = string; /* lowercase hex digest */

/* A token. `Hashed` carries an already-computed digest and is written raw, with
   no length prefix, because digests are fixed width and therefore
   self-delimiting. */
type token =
  | Tag(int) /* one byte */
  | Nat(int) /* 64-bit big-endian */
  | Text(string) /* 64-bit big-endian length, then the payload */
  | Hashed(t);

/* Unison's hashingVersion. Bumping this changes every hash in the store. */
let hashing_version: token = Tag(2);

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
  Digestif.SHA3_512.(digest_string(Buffer.contents(buf)) |> to_hex);
};

let equal: (t, t) => bool = String.equal;
let compare: (t, t) => int = String.compare;

let display_prefix = "#";
let to_string = (h: t): string => display_prefix ++ h;
let short = (~len=10, h: t): string =>
  display_prefix ++ String.sub(h, 0, min(len, String.length(h)));
let pp = (fmt, h: t) => Format.fprintf(fmt, "%s", short(h));
let show = (h: t): string => short(h);
