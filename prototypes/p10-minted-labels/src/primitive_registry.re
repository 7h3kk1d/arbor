/* Primitive registry — a per-language extension point for built-in
   functions that have a declared type and a host-language
   implementation, but no dedicated surface syntax.

   How it works:
   - Each primitive has a stable string id (e.g. `string:reverse:v1`),
     a fully-curried Ty.t, and an OCaml function that runs on
     evaluated argument values.
   - The substrate provides `to_lam_ast` which builds the wrapping
     curried Lam whose body is a saturated `Ast.Prim_call(id, args)`.
     Bootstrap ingests this Lam, gets a hash, and binds it to a
     namespace name. The user calls the primitive via the namespace
     (`string.reverse "abc"`); the Resolver inlines the wrapping Lam
     just like any other named definition.
   - Adding a new primitive requires only registering in this module
     and seeding a name in bootstrap. No parser, lexer, or grammar
     changes per primitive.

   Hash stability: the wrapping Lam's hash depends on the id (encoded
   in `Node.Prim_call`) and the argument types (in the wrapping
   `Node.Lam`s). Per the substrate's primitive-identity rule
   (`docs/design/03-content-addressing.md`), the id should be
   versioned (`v1`/`v2`) so that changing a primitive's behavior or
   signature produces a fresh hash, leaving old references orphaned
   visibly rather than silently lying.

   The implementation function is *not* part of the hash. A silent
   change to the OCaml impl without bumping the version produces a
   cache that lies. Acceptable during bootstrap; this is the same
   honest cost the substrate already accepts for procedure identity. */

type impl = list(Ast.t) => option(Ast.t);

type descriptor = {
  id: string,
  ty: Ty.t,
  impl,
};

let table: Hashtbl.t(string, descriptor) = Hashtbl.create(16);

let register = (d: descriptor): unit =>
  Hashtbl.replace(table, d.id, d);

let find = (id: string): option(descriptor) =>
  Hashtbl.find_opt(table, id);

let all_ids = (): list(string) =>
  Hashtbl.fold((id, _, acc) => [id, ...acc], table, [])
  |> List.sort(String.compare);

let rec arg_types_acc = (acc: list(Ty.t), ty: Ty.t): list(Ty.t) =>
  switch (ty) {
  | Ty.Arrow(a, b) => arg_types_acc([a, ...acc], b)
  | _ => List.rev(acc)
  };

/* Outer-first arg types of a curried function type. */
let arg_types = (ty: Ty.t): list(Ty.t) => arg_types_acc([], ty);

let rec return_type = (ty: Ty.t): Ty.t =>
  switch (ty) {
  | Ty.Arrow(_, b) => return_type(b)
  | _ => ty
  };

let arity_of = (ty: Ty.t): int => List.length(arg_types(ty));

/* Build the curried Lam wrapping a saturated Prim_call. For arity
   n, produces:

     \x_0: A_0. \x_1: A_1. ... \x_{n-1}: A_{n-1}.
       Prim_call(id, [Var(n-1); Var(n-2); ...; Var 0])

   where A_0..A_{n-1} are the outer-first arg types. The args list
   is in outer-first order: the leftmost arg in the Prim_call list
   is the outermost lambda's bound variable. */
let to_lam_ast = (d: descriptor): Ast.t => {
  let arg_tys = arg_types(d.ty);
  let arity = List.length(arg_tys);
  let args = List.init(arity, i => Ast.Var(arity - 1 - i));
  let body = Ast.Prim_call(d.id, args);
  List.fold_right(
    (arg_ty, acc) => Ast.Lam(arg_ty, acc),
    arg_tys,
    body,
  );
};
