/* Definition.t — what the Store holds.

   p9 holds two kinds of definitions in one hash-space:
   - `Term(Node.t)` — the prototype's typed term language; hashes
     start with the language tag byte 'P'.
   - `Type(Ty.t)`  — concrete types as first-class definitions;
     hashes start with the leading byte 'T' from `Ty.hash`.

   Disambiguation between term and type hashes is at the leaf
   encoding (the leading byte), so the two share one Store table
   without collision. The sum exists so the Store can dispatch, the
   Resolver can kind-check named references, and the UI can render
   each appropriately. */

[@deriving (eq, show)]
type t =
  | Term(Node.t)
  | Type(Ty.t);

let hash = (def: t): Hash.t =>
  switch (def) {
  | Term(n) => Node.hash(n)
  | Type(ty) => Ty.hash(ty)
  };

[@deriving (eq, show)]
type kind =
  | Term_kind
  | Type_kind;

let kind = (def: t): kind =>
  switch (def) {
  | Term(_) => Term_kind
  | Type(_) => Type_kind
  };

let kind_to_string =
  fun
  | Term_kind => "term"
  | Type_kind => "type";
