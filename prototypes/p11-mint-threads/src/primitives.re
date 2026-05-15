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
  (
    "int.neg",
    {
      Primitive_registry.id: "int:neg:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Int),
      impl:
        (
          fun
          | [Ast.Int_lit(n)] => Some(Ast.Int_lit(- n))
          | _ => None
        ),
    },
  ),
  (
    "int.min",
    {
      Primitive_registry.id: "int:min:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Int)),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] => Some(Ast.Int_lit(min(a, b)))
          | _ => None
        ),
    },
  ),
  (
    "int.max",
    {
      Primitive_registry.id: "int:max:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Int)),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] => Some(Ast.Int_lit(max(a, b)))
          | _ => None
        ),
    },
  ),
  (
    "int.lt",
    {
      Primitive_registry.id: "int:lt:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] => Some(Ast.Bool_lit(a < b))
          | _ => None
        ),
    },
  ),
  (
    "int.lte",
    {
      Primitive_registry.id: "int:lte:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] => Some(Ast.Bool_lit(a <= b))
          | _ => None
        ),
    },
  ),
  (
    "int.gt",
    {
      Primitive_registry.id: "int:gt:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] => Some(Ast.Bool_lit(a > b))
          | _ => None
        ),
    },
  ),
  (
    "int.gte",
    {
      Primitive_registry.id: "int:gte:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] => Some(Ast.Bool_lit(a >= b))
          | _ => None
        ),
    },
  ),
  (
    "int.pow",
    {
      Primitive_registry.id: "int:pow:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Int)),
      impl:
        (
          fun
          | [Ast.Int_lit(base), Ast.Int_lit(exp)] when exp >= 0 => {
              let rec go = (acc, e) =>
                e == 0 ? acc : go(acc * base, e - 1);
              Some(Ast.Int_lit(go(1, exp)));
            }
          | _ => None
        ),
    },
  ),
  /* int.divmod: only primitive that returns a Product value */
  (
    "int.divmod",
    {
      Primitive_registry.id: "int:divmod:v1",
      ty: Ty.Arrow(Ty.Int, Ty.Arrow(Ty.Int, Ty.Product(Ty.Int, Ty.Int))),
      impl:
        (
          fun
          | [Ast.Int_lit(a), Ast.Int_lit(b)] when b != 0 =>
            Some(Ast.Pair(Ast.Int_lit(a / b), Ast.Int_lit(a mod b)))
          | _ => None
        ),
    },
  ),
  (
    "bool.to_string",
    {
      Primitive_registry.id: "bool:to_string:v1",
      ty: Ty.Arrow(Ty.Bool, Ty.String),
      impl:
        (
          fun
          | [Ast.Bool_lit(b)] => Some(Ast.String_lit(string_of_bool(b)))
          | _ => None
        ),
    },
  ),
  (
    "string.to_upper",
    {
      Primitive_registry.id: "string:to_upper:v1",
      ty: Ty.Arrow(Ty.String, Ty.String),
      impl:
        (
          fun
          | [Ast.String_lit(s)] =>
            Some(Ast.String_lit(String.uppercase_ascii(s)))
          | _ => None
        ),
    },
  ),
  (
    "string.to_lower",
    {
      Primitive_registry.id: "string:to_lower:v1",
      ty: Ty.Arrow(Ty.String, Ty.String),
      impl:
        (
          fun
          | [Ast.String_lit(s)] =>
            Some(Ast.String_lit(String.lowercase_ascii(s)))
          | _ => None
        ),
    },
  ),
  (
    "string.trim",
    {
      Primitive_registry.id: "string:trim:v1",
      ty: Ty.Arrow(Ty.String, Ty.String),
      impl:
        (
          fun
          | [Ast.String_lit(s)] =>
            Some(Ast.String_lit(String.trim(s)))
          | _ => None
        ),
    },
  ),
  /* arg order: haystack first, then prefix/suffix — matches surface call order */
  (
    "string.starts_with",
    {
      Primitive_registry.id: "string:starts_with:v1",
      ty: Ty.Arrow(Ty.String, Ty.Arrow(Ty.String, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.String_lit(s), Ast.String_lit(prefix)] =>
            Some(Ast.Bool_lit(String.starts_with(~prefix, s)))
          | _ => None
        ),
    },
  ),
  (
    "string.ends_with",
    {
      Primitive_registry.id: "string:ends_with:v1",
      ty: Ty.Arrow(Ty.String, Ty.Arrow(Ty.String, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.String_lit(s), Ast.String_lit(suffix)] =>
            Some(Ast.Bool_lit(String.ends_with(~suffix, s)))
          | _ => None
        ),
    },
  ),
  (
    "string.contains",
    {
      Primitive_registry.id: "string:contains:v1",
      ty: Ty.Arrow(Ty.String, Ty.Arrow(Ty.String, Ty.Bool)),
      impl:
        (
          fun
          | [Ast.String_lit(haystack), Ast.String_lit(needle)] => {
              let hn = String.length(haystack);
              let nn = String.length(needle);
              if (nn == 0) {
                Some(Ast.Bool_lit(true));
              } else if (nn > hn) {
                Some(Ast.Bool_lit(false));
              } else {
                let found = ref(false);
                for (i in 0 to hn - nn) {
                  if (String.sub(haystack, i, nn) == needle) {
                    found := true;
                  };
                };
                Some(Ast.Bool_lit(found^));
              };
            }
          | _ => None
        ),
    },
  ),
  (
    "string.repeat",
    {
      Primitive_registry.id: "string:repeat:v1",
      ty: Ty.Arrow(Ty.String, Ty.Arrow(Ty.Int, Ty.String)),
      impl:
        (
          fun
          | [Ast.String_lit(s), Ast.Int_lit(n)] when n >= 0 => {
              let buf = Buffer.create(String.length(s) * n + 1);
              for (_ in 1 to n) {
                Buffer.add_string(buf, s);
              };
              Some(Ast.String_lit(Buffer.contents(buf)));
            }
          | _ => None
        ),
    },
  ),
  /* List operations. Monomorphic over element type — p10 doesn't yet
     have parametric polymorphism, so each operation that depends on
     the element shape gets a per-type version. The most useful general
     ones (length, reverse) work uniformly because they only count or
     reorder. */
  (
    "list.range",
    {
      Primitive_registry.id: "list:range:v1",
      ty: Ty.Arrow(Ty.Int, Ty.List(Ty.Int)),
      impl:
        (
          fun
          | [Ast.Int_lit(n)] => {
              let items =
                if (n <= 0) {
                  [];
                } else {
                  let rec build = (i, acc) =>
                    if (i < 0) {
                      acc;
                    } else {
                      build(i - 1, [Ast.Int_lit(i), ...acc]);
                    };
                  build(n - 1, []);
                };
              Some(Ast.List_lit(items));
            }
          | _ => None
        ),
    },
  ),
  (
    "list.length_int",
    {
      Primitive_registry.id: "list:length_int:v1",
      ty: Ty.Arrow(Ty.List(Ty.Int), Ty.Int),
      impl:
        (
          fun
          | [Ast.List_lit(items)] =>
            Some(Ast.Int_lit(List.length(items)))
          | _ => None
        ),
    },
  ),
  (
    "list.length_string",
    {
      Primitive_registry.id: "list:length_string:v1",
      ty: Ty.Arrow(Ty.List(Ty.String), Ty.Int),
      impl:
        (
          fun
          | [Ast.List_lit(items)] =>
            Some(Ast.Int_lit(List.length(items)))
          | _ => None
        ),
    },
  ),
  (
    "list.sum",
    {
      Primitive_registry.id: "list:sum:v1",
      ty: Ty.Arrow(Ty.List(Ty.Int), Ty.Int),
      impl: {
        let rec sum_items = (acc, lst) =>
          switch (lst) {
          | [] => Some(acc)
          | [Ast.Int_lit(n), ...rest] => sum_items(acc + n, rest)
          | _ => None
          };
        (
          fun
          | [Ast.List_lit(items)] =>
            sum_items(0, items) |> Option.map(n => Ast.Int_lit(n))
          | _ => None
        );
      },
    },
  ),
  (
    "list.product",
    {
      Primitive_registry.id: "list:product:v1",
      ty: Ty.Arrow(Ty.List(Ty.Int), Ty.Int),
      impl: {
        let rec prod_items = (acc, lst) =>
          switch (lst) {
          | [] => Some(acc)
          | [Ast.Int_lit(n), ...rest] => prod_items(acc * n, rest)
          | _ => None
          };
        (
          fun
          | [Ast.List_lit(items)] =>
            prod_items(1, items) |> Option.map(n => Ast.Int_lit(n))
          | _ => None
        );
      },
    },
  ),
  (
    "list.reverse_int",
    {
      Primitive_registry.id: "list:reverse_int:v1",
      ty: Ty.Arrow(Ty.List(Ty.Int), Ty.List(Ty.Int)),
      impl:
        (
          fun
          | [Ast.List_lit(items)] =>
            Some(Ast.List_lit(List.rev(items)))
          | _ => None
        ),
    },
  ),
  (
    "list.head_int",
    {
      Primitive_registry.id: "list:head_int:v1",
      ty: Ty.Arrow(Ty.List(Ty.Int), Ty.Int),
      impl:
        (
          fun
          | [Ast.List_lit([Ast.Int_lit(n), ..._])] =>
            Some(Ast.Int_lit(n))
          /* Empty list: return 0 as a permissive default.
             A real language would use an Option type; p10
             doesn't have sums yet. */
          | [Ast.List_lit([])] => Some(Ast.Int_lit(0))
          | _ => None
        ),
    },
  ),
  (
    "list.tail_int",
    {
      Primitive_registry.id: "list:tail_int:v1",
      ty: Ty.Arrow(Ty.List(Ty.Int), Ty.List(Ty.Int)),
      impl:
        (
          fun
          | [Ast.List_lit([_, ...rest])] => Some(Ast.List_lit(rest))
          | [Ast.List_lit([])] => Some(Ast.List_lit([]))
          | _ => None
        ),
    },
  ),
  (
    "list.cons_int",
    {
      Primitive_registry.id: "list:cons_int:v1",
      ty:
        Ty.Arrow(
          Ty.Int,
          Ty.Arrow(Ty.List(Ty.Int), Ty.List(Ty.Int)),
        ),
      impl:
        (
          fun
          | [Ast.Int_lit(n), Ast.List_lit(items)] =>
            Some(Ast.List_lit([Ast.Int_lit(n), ...items]))
          | _ => None
        ),
    },
  ),
  (
    "list.concat_int",
    {
      Primitive_registry.id: "list:concat_int:v1",
      ty:
        Ty.Arrow(
          Ty.List(Ty.Int),
          Ty.Arrow(Ty.List(Ty.Int), Ty.List(Ty.Int)),
        ),
      impl:
        (
          fun
          | [Ast.List_lit(a), Ast.List_lit(b)] =>
            Some(Ast.List_lit(a @ b))
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
        /* Every user-facing binding is minted in p10. The substructure
           body lives at `hash`; we wrap it with a fresh Named_term
           and bind the wrapper's hash to the name. */
        let bound = Store.register_named_term(store, hash);
        try(Namespace.bind(ns, ~name, bound)) {
        | _ => ()
        };
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
