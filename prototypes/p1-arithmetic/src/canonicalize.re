/* Untyped arithmetic has no binders, no commutativity, no reorderings.
   The AST as parsed is already canonical; this module exists so callers
   can always canonicalize before hashing, and so future languages can
   slot their own normalization in here. */

let canonicalize: Ast.t => Ast.t = t => t;
