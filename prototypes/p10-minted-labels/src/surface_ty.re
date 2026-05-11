/* SURFACE TYPE AST — what the parser produces for type expressions.

   Distinct from Ty.t: surface types include `Named(string)` for
   named-type references and `Hole` for explicit `?` in type position
   (or recovery-inserted holes). The Resolver expands these to a
   bare Ty.t before any internal use.

   `Hole` in type position currently resolves to `Ty.Int`, matching
   p6/p9-pre-types behavior. Promotion to a real `Ty.Unknown` is
   tracked in docs/prototypes/p9-typed-namespaces/open-questions.md. */

[@deriving (eq, show, ord)]
type t =
  | Int
  | Bool
  | String
  | Arrow(t, t)
  | Product(t, t)
  | Named(string)
  | Hole;

let rec print_prec = (~prec: int, s: t): string =>
  switch (s) {
  | Int => "Int"
  | Bool => "Bool"
  | String => "String"
  | Named(n) => n
  | Hole => "?"
  | Product(a, b) =>
    let body =
      print_prec(~prec=2, a) ++ " * " ++ print_prec(~prec=2, b);
    if (prec >= 2) {
      "(" ++ body ++ ")";
    } else {
      body;
    }
  | Arrow(a, b) =>
    let body =
      print_prec(~prec=1, a) ++ " -> " ++ print_prec(~prec=0, b);
    if (prec >= 1) {
      "(" ++ body ++ ")";
    } else {
      body;
    }
  };

let print = (s: t): string => print_prec(~prec=0, s);

/* Lift Ty.t into Surface_ty.t. Used when a stored Lam is rendered
   back to surface form: the stored type is a concrete Ty.t with no
   names; a name-aware caller may post-process by replacing subtrees
   whose Ty.hash matches a namespace binding with `Named(name)`. */
let rec of_ty = (ty: Ty.t): t =>
  switch (ty) {
  | Ty.Int => Int
  | Ty.Bool => Bool
  | Ty.String => String
  | Ty.Arrow(a, b) => Arrow(of_ty(a), of_ty(b))
  | Ty.Product(a, b) => Product(of_ty(a), of_ty(b))
  };

/* Count holes in a surface type (parallel to Surface_ast.count_holes). */
let rec count_holes = (s: t): int =>
  switch (s) {
  | Int
  | Bool
  | String
  | Named(_) => 0
  | Hole => 1
  | Arrow(a, b)
  | Product(a, b) => count_holes(a) + count_holes(b)
  };
