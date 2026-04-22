/* DEEP AST — the parser's output, before ingest. Identical to p1's AST.
   After ingest, the Store holds shallow Node.t values instead of these;
   this type exists as the Language-layer pre-ingest representation and
   as the target of reconstruct for display. */

[@deriving (eq, show, ord)]
type t =
  | True
  | False
  | Zero
  | Succ(t)
  | Pred(t)
  | IsZero(t)
  | If(t, t, t);

let rec is_numeric =
  fun
  | Zero => true
  | Succ(a) => is_numeric(a)
  | _ => false;
