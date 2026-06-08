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
  | Exists(string, t)
  | List(t)
  | Record(list((string, t))); /* { x: T, y: U } — field names resolve to labels */
