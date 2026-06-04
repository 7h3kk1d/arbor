/* Minted marks (docs/design/10-minted-identity.md). A mint is a fresh 16-byte
   mark drawn when an abstract type is created; it is placed *inside* the Opaque
   type's hash so that two independently-authored abstractions over the same
   witness stay distinct (Counter over Int != Celsius over Int).

   Generation is deterministic — drawn from an explicit counter the caller
   threads (a "mint source") — so a full re-ingest reproduces the same hashes.
   Random/time-based minting would break that reproducibility; "how marks are
   minted" is itself an open question in design/10, and the counter is the
   simplest reproducible choice for the prototype. */

type t = string; /* 16 raw bytes */

type source = {mutable next: int};

let make_source = (): source => {next: 0};

let fresh = (src: source): t => {
  let n = src.next;
  src.next = n + 1;
  let b = Bytes.make(16, '\000');
  Bytes.set_int64_be(b, 8, Int64.of_int(n));
  Bytes.to_string(b);
};

let equal: (t, t) => bool = String.equal;

let compare: (t, t) => int = String.compare;

/* Stable short rendering for UI labels; never exposes the raw mark bytes. */
let short = (m: t): string => {
  let hex = Digestif.BLAKE2B.(digest_string(m) |> to_hex);
  "m_" ++ String.sub(hex, 0, 8);
};
