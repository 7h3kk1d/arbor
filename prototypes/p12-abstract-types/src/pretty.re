/* Name-aware rendering of a type hash. A bound name short-circuits (so an opaque
   type prints as `Counter.t`); otherwise the type is rendered structurally,
   recursing into component hashes (which may themselves be named). An opaque
   type with no name falls back to `opaque(m_xxxx)` — the witness is never shown
   (it would leak the representation outside an opening context). */

let rec ty_to_string =
        (~ns: Namespace.t, ~st: Store.t, ~prec: int, h: Hash.t): string =>
  switch (Namespace.name_of(ns, h)) {
  | Some(name) => name
  | None =>
    switch (Store.find(st, h)) {
    | Some(Definition.Type(Tnode.Int)) => "Int"
    | Some(Definition.Type(Tnode.Bool)) => "Bool"
    | Some(Definition.Type(Tnode.Arrow(a, b))) =>
      let s =
        ty_to_string(~ns, ~st, ~prec=1, a)
        ++ " -> "
        ++ ty_to_string(~ns, ~st, ~prec=0, b);
      if (prec >= 1) {
        "(" ++ s ++ ")";
      } else {
        s;
      };
    | Some(Definition.Type(Tnode.Product(a, b))) =>
      let s =
        ty_to_string(~ns, ~st, ~prec=2, a)
        ++ " * "
        ++ ty_to_string(~ns, ~st, ~prec=2, b);
      if (prec >= 2) {
        "(" ++ s ++ ")";
      } else {
        s;
      };
    | Some(Definition.Type(Tnode.Opaque({mint, _}))) =>
      "opaque(" ++ Mint.short(mint) ++ ")"
    | Some(Definition.Term(_)) => "<term>"
    | None => Hash.short(h)
    }
  };

let ty = (~ns, ~st, h) => ty_to_string(~ns, ~st, ~prec=0, h);
