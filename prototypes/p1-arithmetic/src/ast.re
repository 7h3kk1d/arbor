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

let is_value =
  fun
  | True
  | False => true
  | t => is_numeric(t);
