/* Content-addressed definition store. Maps Hash.t -> Definition.t, plus a
   Type_of cache (term hash -> its type hash) computed at ingest. Primitive
   types Int/Bool are registered at creation so their hashes are stable handles.

   The substrate places NO opacity restriction on Seal authoring: `ingest_term`
   accepts any well-typed term, including a hand-written Seal whose impl matches
   its external type under its declared opens. Opacity is the editing layer's job
   (see Editing_context) — the design/12 trilemma resolution. */

type t = {
  defs: Hashtbl.t(Hash.t, Definition.t),
  tyof: Hashtbl.t(Hash.t, Hash.t),
  int_h: Hash.t,
  bool_h: Hash.t,
};

let register_type = (defs, tn: Tnode.t): Hash.t => {
  let d = Definition.Type(tn);
  let h = Definition.hash(d);
  if (!Hashtbl.mem(defs, h)) {
    Hashtbl.replace(defs, h, d);
  };
  h;
};

let create = (): t => {
  let defs = Hashtbl.create(256);
  let tyof = Hashtbl.create(256);
  let int_h = register_type(defs, Tnode.Int);
  let bool_h = register_type(defs, Tnode.Bool);
  {defs, tyof, int_h, bool_h};
};

let ingest_type = (st: t, tn: Tnode.t): Hash.t => register_type(st.defs, tn);

let find = (st: t, h: Hash.t): option(Definition.t) => Hashtbl.find_opt(st.defs, h);

let type_of = (st: t, h: Hash.t): option(Hash.t) => Hashtbl.find_opt(st.tyof, h);

let int_type = (st: t): Hash.t => st.int_h;
let bool_type = (st: t): Hash.t => st.bool_h;

let build_env = (st: t): Typecheck.env => {
  Typecheck.resolve: h =>
    switch (Hashtbl.find_opt(st.defs, h)) {
    | Some(Definition.Type(tn)) => tn
    | Some(Definition.Term(_)) =>
      raise(Typecheck.Type_error("expected a type, got a term: " ++ Hash.short(h)))
    | None => raise(Typecheck.Type_error("unknown type: " ++ Hash.short(h)))
    },
  mk_type: tn => register_type(st.defs, tn),
  type_of_ref: h =>
    switch (Hashtbl.find_opt(st.tyof, h)) {
    | Some(t) => t
    | None => raise(Typecheck.Type_error("no Type_of for reference: " ++ Hash.short(h)))
    },
  int_h: st.int_h,
  bool_h: st.bool_h,
};

/* `opens` lets a definition type-check with some abstract types transparent.
   The default ([]) is the opaque view used everywhere except a seal's own impl,
   which is internal to the seal and may legitimately unseal what the seal opens
   (this is what makes "internal" definitions — e.g. tests that observe the
   representation — expressible). */
let ingest_term = (st: t, ~opens=[], node: Node.t): result(Hash.t, string) => {
  let env = build_env(st);
  switch (Typecheck.synth_top(env, opens, node)) {
  | Error(m) => Error(m)
  | Ok(ty_h) =>
    let d = Definition.Term(node);
    let h = Definition.hash(d);
    if (!Hashtbl.mem(st.defs, h)) {
      Hashtbl.replace(st.defs, h, d);
    };
    Hashtbl.replace(st.tyof, h, ty_h);
    Ok(h);
  };
};

/* The definitions that UNSEAL an abstract type (open it): derived, not stored —
   every Seal whose `opens` includes it. This is deliberately broader than "the
   type's operations": it equally catches any internal definition authored
   against the representation (e.g. an internal test). The substrate draws no
   distinction among them — they are all just Seals over the same opened type. */
let unsealers = (st: t, opaque_h: Hash.t): list(Hash.t) =>
  Hashtbl.fold(
    (h, d, acc) =>
      switch (d) {
      | Definition.Term(Node.Seal({opens, _})) when List.mem(opaque_h, opens) => [
          h,
          ...acc,
        ]
      | _ => acc
      },
    st.defs,
    [],
  );
