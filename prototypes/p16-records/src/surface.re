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
  | Prim(prim_op, list(t))
  | TyLam(string, t) /* /\t. e */
  | TyApp(t, Surface_ty.t) /* e [T] */
  | Pack(Surface_ty.t, t, Surface_ty.t) /* pack [W] e as E */
  | Unpack(string, string, t, t) /* unpack [t] x = e in body */
  | Nil(Surface_ty.t) /* nil [T] */
  | Cons(t, t) /* cons h t */
  | Fold(t, t, t) /* fold list init step */
  | ListLit(list(t)); /* [| e1, ..., en |] — element type inferred from the head */
