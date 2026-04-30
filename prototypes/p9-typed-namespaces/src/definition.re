/* Definition.t is an alias for Node.t — the shallow form is what the
   Store actually holds. p9 is single-language, so the alias is plain;
   compare p5/p6 where Definition.t is a sum over languages. */

type t = Node.t;

let hash = Node.hash;
