/* Deterministic hash of a Definition. Hand-rolled tag-byte encoding fed
   into BLAKE2B. Digestif doesn't ship BLAKE3; BLAKE2B is the closest
   modern option already in our dependency set. */

type t = string; /* lowercase hex digest */

let tag_true = '\x01';
let tag_false = '\x02';
let tag_zero = '\x03';
let tag_succ = '\x04';
let tag_pred = '\x05';
let tag_iszero = '\x06';
let tag_if = '\x07';

let rec encode = (buf, t) =>
  switch (t) {
  | Ast.True => Buffer.add_char(buf, tag_true)
  | Ast.False => Buffer.add_char(buf, tag_false)
  | Ast.Zero => Buffer.add_char(buf, tag_zero)
  | Ast.Succ(a) =>
    Buffer.add_char(buf, tag_succ);
    encode(buf, a);
  | Ast.Pred(a) =>
    Buffer.add_char(buf, tag_pred);
    encode(buf, a);
  | Ast.IsZero(a) =>
    Buffer.add_char(buf, tag_iszero);
    encode(buf, a);
  | Ast.If(c, thn, els) =>
    Buffer.add_char(buf, tag_if);
    encode(buf, c);
    encode(buf, thn);
    encode(buf, els);
  };

let of_ast = (t: Ast.t): t => {
  let buf = Buffer.create(32);
  encode(buf, t);
  Digestif.BLAKE2B.(digest_string(Buffer.contents(buf)) |> to_hex);
};

let to_string = (h: t): string => "h:" ++ h;

let equal: (t, t) => bool = String.equal;

let short = (~len=12, h: t): string =>
  "h:" ++ String.sub(h, 0, min(len, String.length(h)));

/* Lookup by hex prefix. Returns the matched hash or an error if zero or
   multiple candidates match. */
type lookup_result =
  | Found(t)
  | NotFound
  | Ambiguous(list(t));

let lookup_by_prefix = (prefix: string, candidates: list(t)): lookup_result => {
  let prefix =
    if (String.length(prefix) >= 2 && String.sub(prefix, 0, 2) == "h:") {
      String.sub(prefix, 2, String.length(prefix) - 2);
    } else {
      prefix;
    };
  let matches =
    List.filter(
      h => {
        let pl = String.length(prefix);
        String.length(h) >= pl && String.sub(h, 0, pl) == prefix;
      },
      candidates,
    );
  switch (matches) {
  | [] => NotFound
  | [h] => Found(h)
  | hs => Ambiguous(hs)
  };
};
