/* Surface terms: what the parser produces. Binders (Lam, Let) carry string
   names; the resolver eliminates them to de Bruijn indices and resolves free
   names through the namespace to Ref(hash). No holes, no recovery (fail-fast). */

type prim_op =
  | Add
  | Sub
  | Mul
  | Eq;

type t =
  | Var(string)
  | Lit(int)
  | Bool(bool)
  | Lam(string, Surface_ty.t, t)
  | App(t, t)
  | Let(string, t, t)
  | If(t, t, t)
  | Pair(t, t)
  | Fst(t)
  | Snd(t)
  | Prim(prim_op, list(t));
