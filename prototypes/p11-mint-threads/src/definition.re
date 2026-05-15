/* Definition.t — what the Store holds.

   p10 holds five sorts of entries in one hash-space (subspaces by
   leading bytes inside the language-tag 'R'):

   - `Term(Node.t)` — substructure terms. Internal AST nodes hashed
     structurally (α-equivalence-by-canonicalization, content sharing,
     no mint). Not directly namespace-bound. Hashes begin with 'R'
     followed by the Node body's tag byte.
   - `Type(Ty.t)` — substructure types. Structural, no mint. Not
     directly namespace-bound. Hashes begin with 'U' (Ty's tag).
   - `Named_term(Mint.t, Hash.t)` — a user-named top-level term
     binding. Wraps a substructure body hash with a fresh 16-byte
     mint mark. Two `pi = 3.14` ingests share the same body hash but
     mint distinct Named_term hashes. Hash bytes: 'R' + 'M' + mint + body.
   - `Named_type(Mint.t, Hash.t)` — same maneuver for types. Two
     `type Foo = Int` ingests share `Int`'s substructure hash but
     mint distinct Named_types. Hash bytes: 'R' + 'N' + mint + body.
   - `Label(Label.t)` — record/constructor/method field identity.
     Just a mint mark, no body. Hash bytes: 'R' + 'L' + mint.

   Namespace bindings point at Named_term, Named_type, and Label
   hashes — never at substructure Term/Type hashes. References inside
   stored bodies (Lam's body, App's children, etc.) point at
   substructure Term/Type hashes, preserving structural sharing
   regardless of the surrounding mint. */

[@deriving (eq, show)]
type t =
  | Term(Node.t)
  | Type(Ty.t)
  | Named_term(Mint.t, Hash.t)
  | Named_type(Mint.t, Hash.t)
  | Label(Label.t);

let language_tag = 'R';
let tag_named_term = 'M';
let tag_named_type = 'N';

let hash_named_term = (mint: Mint.t, body: Hash.t): Hash.t => {
  let buf = Buffer.create(2 + Mint.length + String.length(body));
  Buffer.add_char(buf, language_tag);
  Buffer.add_char(buf, tag_named_term);
  Mint.encode(buf, mint);
  Buffer.add_string(buf, body);
  Hash.digest_buffer(buf);
};

let hash_named_type = (mint: Mint.t, body: Hash.t): Hash.t => {
  let buf = Buffer.create(2 + Mint.length + String.length(body));
  Buffer.add_char(buf, language_tag);
  Buffer.add_char(buf, tag_named_type);
  Mint.encode(buf, mint);
  Buffer.add_string(buf, body);
  Hash.digest_buffer(buf);
};

let hash = (def: t): Hash.t =>
  switch (def) {
  | Term(n) => Node.hash(n)
  | Type(ty) => Ty.hash(ty)
  | Named_term(mint, body) => hash_named_term(mint, body)
  | Named_type(mint, body) => hash_named_type(mint, body)
  | Label(label) => Label.hash(label)
  };

/* User-facing kinds. Substructure Term and Type collapse into the same
   user-facing Term_kind / Type_kind as their Named counterparts; the
   substructure-vs-named distinction is internal to the Store. */
[@deriving (eq, show)]
type kind =
  | Term_kind
  | Type_kind
  | Label_kind;

let kind = (def: t): kind =>
  switch (def) {
  | Term(_)
  | Named_term(_, _) => Term_kind
  | Type(_)
  | Named_type(_, _) => Type_kind
  | Label(_) => Label_kind
  };

let kind_to_string =
  fun
  | Term_kind => "term"
  | Type_kind => "type"
  | Label_kind => "label";
