/* Surface types: what the parser produces for type expressions. `Named` is a
   reference resolved through the namespace to a type hash (concrete or opaque).
   The resolver turns this into a registered type hash. */

type t =
  | Int
  | Bool
  | Arrow(t, t)
  | Product(t, t)
  | Named(string);
