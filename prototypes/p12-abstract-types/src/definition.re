/* A definition is the unit of content-addressed storage: a term node or a type
   node. The leading sort byte of each encoder ('P' for terms, 'T' for types)
   keeps the two hash spaces disjoint. */

type t =
  | Term(Node.t)
  | Type(Tnode.t);

let encode = (d: t): string =>
  switch (d) {
  | Term(n) => Node.encode(n)
  | Type(tn) => Tnode.encode(tn)
  };

let hash = (d: t): Hash.t => Hash.digest_string(encode(d));
