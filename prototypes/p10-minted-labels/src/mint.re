/* Mint marks — opaque 16-byte identifiers stamped into the hash of every
   user-created definition (Term, Type, Label) in p10.

   Marks are generated fresh on every ingest. The substrate offers two
   generators:

   - `fresh` draws 16 random bytes from `Random.bits` seeded from the
     system clock at module init. Suitable for the runtime web UI: each
     ingest yields a different mark, so re-ingesting the same source
     produces a different definition. State is not persistent across
     page reloads anyway, so the lack of reproducibility is invisible.

   - `from_counter` and `reset_counter` produce a deterministic
     monotonic sequence. Used by tests so hash assertions are stable. */

[@deriving (eq, ord, show)]
type t = string; /* exactly 16 bytes */

let length = 16;

let zero: t = String.make(length, '\x00');

let counter = ref(0);

let reset_counter = () => counter := 0;

let from_counter = (): t => {
  incr(counter);
  let n = counter^;
  let b = Bytes.make(length, '\x00');
  /* Write the counter as big-endian bytes in the trailing 8 bytes,
     leaving the leading 8 as zero so the mark is easy to recognize. */
  for (i in 0 to 7) {
    let shift = (7 - i) * 8;
    Bytes.set(b, 8 + i, Char.chr((n lsr shift) land 0xff));
  };
  Bytes.unsafe_to_string(b);
};

let random_state = lazy(Random.State.make_self_init());

let fresh = (): t => {
  let st = Lazy.force(random_state);
  let b = Bytes.make(length, '\x00');
  for (i in 0 to length - 1) {
    Bytes.set(b, i, Char.chr(Random.State.bits(st) land 0xff));
  };
  Bytes.unsafe_to_string(b);
};

/* Encoding: 16 raw bytes appended to a buffer. */
let encode = (buf: Buffer.t, m: t): unit => Buffer.add_string(buf, m);

/* Hex print for inspection. */
let print = (m: t): string => {
  let buf = Buffer.create(2 * length);
  String.iter(
    c => Buffer.add_string(buf, Printf.sprintf("%02x", Char.code(c))),
    m,
  );
  Buffer.contents(buf);
};

let short = (m: t): string => String.sub(print(m), 0, 8);
