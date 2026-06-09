/* A definition is the unit of content-addressed storage: a term node, a type
   node, or a label (record-field identity, design/11). The leading sort byte of
   each encoder ('P' terms, 'T' types, 'L' labels) keeps the hash spaces
   disjoint. */

type t =
  | Term(Node.t)
  | Type(Tnode.t)
  | Label(Label.t);

let encode = (d: t): string =>
  switch (d) {
  | Term(n) => Node.encode(n)
  | Type(tn) => Tnode.encode(tn)
  | Label(l) => Label.encode(l)
  };

let hash = (d: t): Hash.t => Hash.digest_string(encode(d));
