/* Content hash. Identity for values in the Store. BLAKE2B via digestif,
   displayed as lowercase hex. The node-to-bytes encoding lives in
   Node.re, which owns its own hashing (`Node.hash`). This module is the
   primitive layer: digests, comparisons, prefix lookup. */

type t = string; /* lowercase hex digest of the node's encoding */

let digest_buffer = (buf: Buffer.t): t =>
  Digestif.BLAKE2B.(digest_string(Buffer.contents(buf)) |> to_hex);

let equal: (t, t) => bool = String.equal;

let to_string = (h: t): string => "h:" ++ h;

let short = (~len=12, h: t): string =>
  "h:" ++ String.sub(h, 0, min(len, String.length(h)));

let pp = (fmt, h: t) => Format.fprintf(fmt, "%s", short(h));

let show = (h: t): string => short(h);

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
