/* SURFACE AST — what the parser produces and the printer renders.

   Carries string names for binders (Lam, Let) and variables. Names may
   be dot-delimited (e.g. "math.add"). Resolver eliminates names: bound
   names become de Bruijn indices, free names resolve through the
   Namespace either by full path or by Unison-style longest-segment
   suffix.

   Both Lam and Let bind by name on the surface and by de Bruijn index
   internally — the substrate's α-equivalence-by-canonicalization
   guarantee from p4 extends to Let here.

   p9 carries forward p8's Hole constructor — sources are explicit `?` in
   user input or implicit insertion by Parse_recover. All holes are
   structurally equal (no payload). */

[@deriving (eq, show, ord)]
type prim_op =
  | Add
  | Sub
  | Mul
  | Div
  | Mod
  | And
  | Or
  | Not
  | Concat
  | Eq;

[@deriving (eq, show, ord)]
type t =
  | Var(string)
  | Int_lit(int)
  | Bool_lit(bool)
  | String_lit(string)
  | Lam(string, Ty.t, t)
  | App(t, t)
  | Let(string, t, t)
  | If(t, t, t)
  | Pair(t, t)
  | Fst(t)
  | Snd(t)
  | Prim(prim_op, list(t))
  | Hole;

let prim_op_to_string =
  fun
  | Add => "+"
  | Sub => "-"
  | Mul => "mul"
  | Div => "/"
  | Mod => "mod"
  | And => "&&"
  | Or => "||"
  | Not => "not"
  | Concat => "++"
  | Eq => "==";

/* Count holes in a surface term; used by the UI to label "N holes
   inserted by recovery" and by tests. */
let rec count_holes = (t: t): int =>
  switch (t) {
  | Var(_) => 0
  | Int_lit(_)
  | Bool_lit(_)
  | String_lit(_) => 0
  | Hole => 1
  | Lam(_, _, body) => count_holes(body)
  | Let(_, rhs, body) => count_holes(rhs) + count_holes(body)
  | App(f, a) => count_holes(f) + count_holes(a)
  | If(c, t', e) => count_holes(c) + count_holes(t') + count_holes(e)
  | Pair(a, b) => count_holes(a) + count_holes(b)
  | Fst(a)
  | Snd(a) => count_holes(a)
  | Prim(_, args) =>
    List.fold_left((acc, a) => acc + count_holes(a), 0, args)
  };
