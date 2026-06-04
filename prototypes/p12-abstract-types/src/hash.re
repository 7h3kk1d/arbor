/* Content hash. Identity for values in the Store. BLAKE2B via digestif,
   displayed as lowercase hex. The node-to-bytes encoding lives in the encoder
   that owns each sort; this module is the primitive layer: digests,
   comparisons, prefix lookup. Carried (essentially verbatim) from p11. */

type t = string; /* lowercase hex digest */

let digest_buffer = (buf: Buffer.t): t =>
  Digestif.BLAKE2B.(digest_string(Buffer.contents(buf)) |> to_hex);

let digest_string = (s: string): t =>
  Digestif.BLAKE2B.(digest_string(s) |> to_hex);

let equal: (t, t) => bool = String.equal;

let compare: (t, t) => int = String.compare;

let display_prefix = "#";

let to_string = (h: t): string => display_prefix ++ h;

let short = (~len=12, h: t): string =>
  display_prefix ++ String.sub(h, 0, min(len, String.length(h)));

let pp = (fmt, h: t) => Format.fprintf(fmt, "%s", short(h));

let show = (h: t): string => short(h);
