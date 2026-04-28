/* Definition.t is an alias for Node.t — the shallow form is what the
   Store actually holds. Same shape as p3 (alias over the stored
   language's node type); when a second language appears the alias
   becomes a sum. */

type t = Node.t;

let hash = Node.hash;
