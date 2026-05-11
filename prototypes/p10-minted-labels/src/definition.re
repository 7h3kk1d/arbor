/* Definition.t — what the Store holds.

   p10 holds three sorts of definition in one hash-space, all under the
   p10 language-tag-byte 'Q' (subspaces by leading bytes):
   - `Term(Node.t)` — terms in the prototype's typed language;
     hashes begin with the Node language tag 'Q'.
   - `Type(Ty.t)`  — concrete types as first-class definitions;
     hashes begin with 'U' from `Ty.hash`.
   - `Label(Label.t)` — field / constructor / method identities;
     hashes begin with 'Q' followed by the Label sort-tag 'L'.

   Disambiguation between sorts is at the leaf encoding (the leading
   one or two bytes), so the three share one Store table without
   collision. The sum exists so the Store can dispatch, the Resolver
   can kind-check named references, and the UI can render each
   appropriately.

   Slice A: the `Label` arm is wired in; mint marks for Term and Type
   are not yet baked into their canonical bytes — that's slice B. For
   now, Term and Type hashes match Node.hash / Ty.hash, exactly as in
   p9 modulo the changed language tag bytes. */

[@deriving (eq, show)]
type t =
  | Term(Node.t)
  | Type(Ty.t)
  | Label(Label.t);

let hash = (def: t): Hash.t =>
  switch (def) {
  | Term(n) => Node.hash(n)
  | Type(ty) => Ty.hash(ty)
  | Label(label) => Label.hash(label)
  };

[@deriving (eq, show)]
type kind =
  | Term_kind
  | Type_kind
  | Label_kind;

let kind = (def: t): kind =>
  switch (def) {
  | Term(_) => Term_kind
  | Type(_) => Type_kind
  | Label(_) => Label_kind
  };

let kind_to_string =
  fun
  | Term_kind => "term"
  | Type_kind => "type"
  | Label_kind => "label";
