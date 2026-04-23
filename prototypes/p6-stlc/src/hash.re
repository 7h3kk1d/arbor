/* Content hash. Identity for values in the Store. BLAKE2B via digestif,
   displayed as lowercase hex. The node-to-bytes encoding lives in each
   language's Node module, which owns its own hashing. This module is
   the primitive layer: digests, comparisons, prefix lookup.

   Unchanged across p3/p4/p5. */

type t = string; /* lowercase hex digest of the node's encoding */

let digest_buffer = (buf: Buffer.t): t =>
  Digestif.BLAKE2B.(digest_string(Buffer.contents(buf)) |> to_hex);

let equal: (t, t) => bool = String.equal;

let display_prefix = "#";

let to_string = (h: t): string => display_prefix ++ h;

let short = (~len=12, h: t): string =>
  display_prefix ++ String.sub(h, 0, min(len, String.length(h)));

let pp = (fmt, h: t) => Format.fprintf(fmt, "%s", short(h));

let show = (h: t): string => short(h);

let strip_display_prefix = (s: string): string =>
  if (String.length(s) >= 1 && String.sub(s, 0, 1) == "#") {
    String.sub(s, 1, String.length(s) - 1);
  } else if (String.length(s) >= 2 && String.sub(s, 0, 2) == "h:") {
    String.sub(s, 2, String.length(s) - 2);
  } else {
    s;
  };

let has_display_prefix = (s: string): bool =>
  (String.length(s) >= 1 && String.sub(s, 0, 1) == "#")
  || (String.length(s) >= 2 && String.sub(s, 0, 2) == "h:");

let is_hex_char = c =>
  (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');

let looks_like_hash_prefix = (s: string): bool => {
  let bare = strip_display_prefix(s);
  let all_hex = {
    let n = String.length(bare);
    n > 0 && {
      let ok = ref(true);
      for (i in 0 to n - 1) {
        if (!is_hex_char(bare.[i])) {
          ok := false;
        };
      };
      ok^;
    };
  };
  has_display_prefix(s) ? all_hex : all_hex && String.length(bare) >= 4;
};

type lookup_result =
  | Found(t)
  | NotFound
  | Ambiguous(list(t));

let lookup_by_prefix = (prefix: string, candidates: list(t)): lookup_result => {
  let prefix = strip_display_prefix(prefix);
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
