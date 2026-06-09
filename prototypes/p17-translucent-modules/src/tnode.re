/* Content-addressed types (design/03 §"Hashing types as well as terms").
   A type definition's content is its encoding; compound types reference their
   component type definitions *by hash* (sharing + namespace-level aliasing).

   `Opaque{mint, witness}` is the abstract type (design/12): the mint gives
   distinctness (Counter over Int != Celsius over Int), the witness hash gives
   soundness (a representation-type change moves the abstract type's identity).
   The witness is reachable but is only unfolded by the checker when the opaque
   is in the current open set. */

type t =
  | Int
  | Bool
  | Product(Hash.t, Hash.t)
  | Arrow(Hash.t, Hash.t)
  | Opaque({
      mint: Mint.t,
      witness: Hash.t,
    })
  /* System-F (p13): a de Bruijn type variable (counts Forall binders outward)
     and the universal type. `Forall(body)`'s body is a type with one more type
     variable in scope; de Bruijn ⇒ alpha-equivalence of ∀ is automatic. */
  | TVar(int)
  | Forall(Hash.t)
  /* p15: `Abstract(mint)` is a minted abstract type constant — the type
     extracted by opening a module (a Skolem; no witness, never unfolds).
     `List(elem)` is a homogeneous list type. */
  | Abstract(Mint.t)
  | List(Hash.t)
  /* p16: a record type — `(label-hash, field-type-hash)` pairs. Field identity
     is the label hash (never a name). Canonical by sorted label hash, so
     `{x,y}` = `{y,x}` and renames never perturb the hash (design/11). */
  | Record(list((Hash.t, Hash.t)))
  /* p17: a signature — the translucent sum (design/12 "signature = binder +
     label-record"). Components: `Sopaque` (type t — hidden), `Smanifest(T)`
     (type u = T — transparent equation), `Sval(T)` (x : T).

     Binding (the rank rule): only `Sopaque` binds. With k opaques, the opaque
     at rank i in LABEL-HASH SORT ORDER is `TVar(i)` inside every manifest and
     value payload; all payloads live under all k binders at once (enclosing
     Forall/TyLam vars sit at indices >= k). Index = sorted rank, not
     declaration position, so component order carries no information and the
     encoding canonicalizes by sorting — `Sig` stays order-insensitive like
     `Record`. */
  | Sig(list((Hash.t, sig_comp)))
and sig_comp =
  | Sopaque
  | Smanifest(Hash.t)
  | Sval(Hash.t);

/* The sorted opaque labels — rank i in this list is TVar(i). The single source
   of truth for the rank rule (resolver, checker, open-module, pretty all use
   this; a divergence is a silent miscompile). */
let opaque_ranks = (comps: list((Hash.t, sig_comp))): list(Hash.t) =>
  comps
  |> List.filter_map(((l, c)) =>
       switch (c) {
       | Sopaque => Some(l)
       | _ => None
       }
     )
  |> List.sort(String.compare);

/* Encoding starts with sort byte 'T' so type and term ('P') hashes never
   collide. Hashes are fixed-width hex, so concatenation is self-delimiting. */
let encode = (t: t): string => {
  let b = Buffer.create(96);
  Buffer.add_char(b, 'T');
  switch (t) {
  | Int => Buffer.add_uint8(b, 0x10)
  | Bool => Buffer.add_uint8(b, 0x11)
  | Product(h1, h2) =>
    Buffer.add_uint8(b, 0x12);
    Buffer.add_string(b, h1);
    Buffer.add_string(b, h2);
  | Arrow(h1, h2) =>
    Buffer.add_uint8(b, 0x13);
    Buffer.add_string(b, h1);
    Buffer.add_string(b, h2);
  | Opaque({mint, witness}) =>
    Buffer.add_uint8(b, 0x14);
    Buffer.add_string(b, mint);
    Buffer.add_string(b, witness);
  | TVar(i) =>
    Buffer.add_uint8(b, 0x15);
    Buffer.add_int64_be(b, Int64.of_int(i));
  | Forall(body) =>
    Buffer.add_uint8(b, 0x16);
    Buffer.add_string(b, body);
  /* tag 0x17 (Exists) retired in p17 — not reused */
  | Abstract(mint) =>
    Buffer.add_uint8(b, 0x18);
    Buffer.add_string(b, mint);
  | List(elem) =>
    Buffer.add_uint8(b, 0x19);
    Buffer.add_string(b, elem);
  | Record(fields) =>
    /* canonical: sort by label hash so field order never affects identity */
    let sorted =
      List.sort(((l1, _), (l2, _)) => String.compare(l1, l2), fields);
    Buffer.add_uint8(b, 0x1a);
    Buffer.add_uint8(b, List.length(sorted));
    List.iter(
      ((l, ft)) => {
        Buffer.add_string(b, l);
        Buffer.add_string(b, ft);
      },
      sorted,
    );
  | Sig(comps) =>
    /* canonical: opaques, then manifests, then values, each sorted by label
       hash — order-insensitive because binding is by sorted rank, not
       position. Payload hashes are untouched by the reordering. */
    let group = p =>
      comps
      |> List.filter(((_, c)) => p(c))
      |> List.sort(((l1, _), (l2, _)) => String.compare(l1, l2));
    let opaques =
      group(
        fun
        | Sopaque => true
        | _ => false,
      );
    let manifests =
      group(
        fun
        | Smanifest(_) => true
        | _ => false,
      );
    let vals =
      group(
        fun
        | Sval(_) => true
        | _ => false,
      );
    Buffer.add_uint8(b, 0x1b);
    Buffer.add_uint8(b, List.length(comps));
    List.iter(
      ((l, c)) => {
        switch (c) {
        | Sopaque =>
          Buffer.add_uint8(b, 0x01);
          Buffer.add_string(b, l);
        | Smanifest(h) =>
          Buffer.add_uint8(b, 0x02);
          Buffer.add_string(b, l);
          Buffer.add_string(b, h);
        | Sval(h) =>
          Buffer.add_uint8(b, 0x03);
          Buffer.add_string(b, l);
          Buffer.add_string(b, h);
        }
      },
      opaques @ manifests @ vals,
    );
  };
  Buffer.contents(b);
};
