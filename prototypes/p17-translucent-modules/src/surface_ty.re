/* Surface types: what the parser produces for type expressions. `Named` is a
   reference resolved through the namespace to a type hash (concrete or opaque).
   The resolver turns this into a registered type hash. */

type t =
  | Int
  | Bool
  | Arrow(t, t)
  | Product(t, t)
  | Named(string) /* resolves to a bound type variable or a namespace type */
  | Forall(string, t)
  | List(t)
  | Record(list((string, t))) /* { x: T, y: U } — field names resolve to labels */
  | Sig(list(sig_item)) /* sig { type t, type u = T, x: T } */
and sig_item =
  | Stype(string) /* type t — opaque */
  | Stype_eq(string, t) /* type u = T — manifest */
  | Sfield(string, t); /* x : T — value component */
