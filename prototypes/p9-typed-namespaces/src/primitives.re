/* Canonical p9 default primitive set.

   This module owns the list of built-in primitives that the prototype
   ships with. Each entry pairs a namespace name (`string.length`)
   with a Primitive_registry.descriptor (`string:length:v1` plus its
   type and impl).

   Adding a new primitive is one entry in `defaults` — no parser,
   lexer, or grammar changes. The id should follow the `<category>:
   <name>:v<version>` convention and bump the version when the type
   or behavior changes (see `docs/design/03-content-addressing.md`
   §"Primitive identity"). */

let defaults: list((string, Primitive_registry.descriptor)) = [
  (
    "string.length",
    {
      Primitive_registry.id: "string:length:v1",
      ty: Ty.Arrow(Ty.String, Ty.Int),
      impl:
        (
          fun
          | [Ast.String_lit(s)] => Some(Ast.Int_lit(String.length(s)))
          | _ => None
        ),
    },
  ),
  (
    "string.reverse",
    {
      Primitive_registry.id: "string:reverse:v1",
      ty: Ty.Arrow(Ty.String, Ty.String),
      impl:
        (
          fun
          | [Ast.String_lit(s)] => {
              let n = String.length(s);
              let b = Bytes.create(n);
              for (i in 0 to n - 1) {
                Bytes.set(b, i, s.[n - 1 - i]);
              };
              Some(Ast.String_lit(Bytes.to_string(b)));
            }
          | _ => None
        ),
    },
  ),
  (
    "string.substring",
    {
      Primitive_registry.id: "string:substring:v1",
      ty: Ty.Arrow(Ty.String, Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.String))),
      impl:
        (
          fun
          | [Ast.String_lit(s), Ast.Int_lit(lo), Ast.Int_lit(hi)] => {
              let len = String.length(s);
              let lo = max(0, min(len, lo));
              let hi = max(lo, min(len, hi));
              Some(Ast.String_lit(String.sub(s, lo, hi - lo)));
            }
          | _ => None
        ),
    },
  ),
  (
    "int.abs",
    {
      Primitive_registry.id: "int:abs:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Int),
      impl:
        (
          fun
          | [Ast.Int_lit(n)] => Some(Ast.Int_lit(abs(n)))
          | _ => None
        ),
    },
  ),
  (
    "int.to_string",
    {
      Primitive_registry.id: "int:to_string:v1",
      ty: Ty.Arrow(Ty.Int, Ty.String),
      impl:
        (
          fun
          | [Ast.Int_lit(n)] => Some(Ast.String_lit(string_of_int(n)))
          | _ => None
        ),
    },
  ),
];

/* Add every default descriptor to the registry. Idempotent — calling
   this twice (e.g. once from bootstrap, once from a test) is
   harmless because Primitive_registry.register replaces by id. */
let register_all = (): unit =>
  List.iter(
    ((_, d)) => Primitive_registry.register(d),
    defaults,
  );

/* Build the wrapping Lam Ast for each default, ingest it (attaching
   the typecheck + has-holes aspects), and bind the corresponding
   name. Existing-name bindings are silently skipped so re-running
   on a partially-seeded namespace is safe. */
let install_bindings =
    (~store: Store.t, ~att: Attachment.t, ~ns: Namespace.t): unit =>
  List.iter(
    ((name, d)) => {
      let ast = Primitive_registry.to_lam_ast(d);
      switch (Resolver.ingest_ast(~store, ~att, ast)) {
      | Ok({hash, _}) =>
        try(Namespace.bind(ns, ~name, hash)) {
        | _ => ()
        }
      | Error(_) => ()
      };
    },
    defaults,
  );

let install =
    (~store: Store.t, ~att: Attachment.t, ~ns: Namespace.t): unit => {
  register_all();
  install_bindings(~store, ~att, ~ns);
};
