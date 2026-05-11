/* SURFACE TYPE AST — what the parser produces for type expressions.

   Distinct from Ty.t: surface types include `Named(string)` for
   named-type references and `Hole` for explicit `?` in type position
   (or recovery-inserted holes). The Resolver expands these to a
   bare Ty.t before any internal use.

   p10 extends p9's surface types with:
   - `Tuple(list(t))` — n-ary positional tuple types, target of the
     new `(Int, String, Bool)` surface syntax.
   - `Record_decl(list((string, t)))` — labeled record types in their
     declaration form, e.g. `{ x: Int, y: Int }`. Labels are surface
     strings here; the Resolver mints them through the namespace.
   - `List(t)` — monomorphic list element type.

   `Hole` in type position currently resolves to `Ty.Int`, matching
   p6/p9-pre-types behavior. */

[@deriving (eq, show, ord)]
type t =
  | Int
  | Bool
  | String
  | Arrow(t, t)
  | Product(t, t)
  | Tuple(list(t))
  | Record_decl(list((string, t)))
  | List(t)
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
  | Tuple(ts) =>
    "("
    ++ String.concat(", ", List.map(t => print_prec(~prec=0, t), ts))
    ++ ")"
  | Record_decl(fields) =>
    "{ "
    ++ String.concat(
         ", ",
         List.map(
           ((name, t)) => name ++ ": " ++ print_prec(~prec=0, t),
           fields,
         ),
       )
    ++ " }"
  | List(t) =>
    let body = "List " ++ print_prec(~prec=3, t);
    if (prec >= 3) {
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

/* Lift Ty.t into Surface_ty.t. Record fields' label hashes lose their
   namespace names here — `of_ty` is name-blind. Callers wanting
   reverse-resolved labels go through the name-aware Pretty layer
   directly. We render label hashes as short-prefix strings. */
let rec of_ty = (ty: Ty.t): t =>
  switch (ty) {
  | Ty.Int => Int
  | Ty.Bool => Bool
  | Ty.String => String
  | Ty.Arrow(a, b) => Arrow(of_ty(a), of_ty(b))
  | Ty.Product(a, b) => Product(of_ty(a), of_ty(b))
  | Ty.Tuple(ts) => Tuple(List.map(of_ty, ts))
  | Ty.Record(fields) =>
    Record_decl(
      List.map(((h, t)) => (Hash.short(h), of_ty(t)), fields),
    )
  | Ty.List(t) => List(of_ty(t))
  | Ty.Named(h) => Named(Hash.short(h))
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
  | Tuple(ts) => List.fold_left((acc, t) => acc + count_holes(t), 0, ts)
  | Record_decl(fields) =>
    List.fold_left((acc, (_, t)) => acc + count_holes(t), 0, fields)
  | List(t) => count_holes(t)
  };
