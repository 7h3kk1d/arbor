/* Types. (def:utyping)
 *
 *   T ::= Nat | Text | T -> U
 *
 * Types are hashed too — they have to be, because def:typeinhash puts a
 * definition's type inside its term hash. The type layer's tokens are
 * domain-separated from the term layer's by a leading tag. */

type t =
  | Nat
  | Text
  | Arrow(t, t);

/* Type-layer tag. The term layer uses Tag(1); using Tag(0) here is what stops
   a type and a term from ever colliding. (def:tokens) */
let layer_tag = 0;

let rec hash = (ty: t): Hash.t =>
  Hash.acc(
    switch (ty) {
    | Nat => [Hash.Tag(layer_tag), Hash.Tag(0)]
    | Text => [Hash.Tag(layer_tag), Hash.Tag(1)]
    | Arrow(a, b) => [
        Hash.Tag(layer_tag),
        Hash.Tag(2),
        Hash.Hashed(hash(a)),
        Hash.Hashed(hash(b)),
      ]
    },
  );

let rec equal = (a: t, b: t): bool =>
  switch (a, b) {
  | (Nat, Nat) => true
  | (Text, Text) => true
  | (Arrow(a1, b1), Arrow(a2, b2)) => equal(a1, a2) && equal(b1, b2)
  | _ => false
  };

let rec to_string = (ty: t): string =>
  switch (ty) {
  | Nat => "Nat"
  | Text => "Text"
  | Arrow(Arrow(_, _) as a, b) =>
    "(" ++ to_string(a) ++ ") -> " ++ to_string(b)
  | Arrow(a, b) => to_string(a) ++ " -> " ++ to_string(b)
  };
