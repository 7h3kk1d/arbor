/* stlc → lc translator: erase type annotations and Church-encode
   booleans. Total on every stored stlc definition (well-typedness is
   the Store invariant).

   Aspect `translation-to-lc`, procedure
   `stlc-to-lc:erase-church:v1`. Output stored as
   Translation_target(Hash.t) on the stlc source. Cache hit on repeat.

   Encodings (TAPL-standard, same as p5's arith-to-lc-church):
     true := \t. \f. t   -> Lam(Lam(Var(1)))
     false:= \t. \f. f   -> Lam(Lam(Var(0)))
     if c t e := c t e   (applied Church conditional)

   Type annotations on Lam are dropped (untyped lc has no annotations).
   De-Bruijn term structure carries across unchanged. */

let aspect_id: Attachment.aspect_id = "translation-to-lc";
let procedure_id: Attachment.procedure_id =
  "stlc-to-lc:erase-church:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["stlc"],
};

type error =
  | Not_stlc(Hash.t)
  | Dangling(Hash.t);

let error_to_string =
  fun
  | Not_stlc(h) =>
    "translation requires an stlc definition, but "
    ++ Hash.short(h)
    ++ " is not stlc"
  | Dangling(h) =>
    "translation source is not in the store: " ++ Hash.short(h);

let church_true: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(1)));
let church_false: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(0)));

/* Walk the stlc AST, emitting a pure lc AST. Type annotations are
   dropped. `true`/`false` become Church booleans; `if c t e` becomes
   the plain application `(c t e)` because a Church boolean consumes
   both branches. */
let rec translate_ast = (s: Stlc_ast.t): Lc_ast.t =>
  switch (s) {
  | Stlc_ast.Var(k) => Lc_ast.Var(k)
  | Stlc_ast.Lam(_ty, body) => Lc_ast.Lam(translate_ast(body))
  | Stlc_ast.App(f, a) => Lc_ast.App(translate_ast(f), translate_ast(a))
  | Stlc_ast.True => church_true
  | Stlc_ast.False => church_false
  | Stlc_ast.If(c, t, e) =>
    Lc_ast.App(
      Lc_ast.App(translate_ast(c), translate_ast(t)),
      translate_ast(e),
    )
  };

let translate =
    (~store: Store.t, ~att: Attachment.t, source: Hash.t)
    : result((Hash.t, bool), error) =>
  switch (
    Attachment.peek(
      att,
      ~target=source,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
    )
  ) {
  | Some(Attachment.Translation_target(target)) => Ok((target, true))
  | Some(_)
  | None =>
    switch (Store.lookup(store, source)) {
    | None => Error(Dangling(source))
    | Some(Definition.Lc(_)) => Error(Not_stlc(source))
    | Some(Definition.Stlc(_)) =>
      switch (Store.reconstruct_stlc(store, source)) {
      | None => Error(Dangling(source))
      | Some(stlc) =>
        let lc_ast = translate_ast(stlc);
        let target = Store.ingest_lc(store, Lc_canonicalize.canonicalize(lc_ast));
        Attachment.attach(
          att,
          ~target=source,
          ~aspect=aspect_id,
          ~procedure=procedure_id,
          Attachment.Translation_target(target),
        );
        Ok((target, false));
      }
    }
  };

let all_translations =
    (att: Attachment.t): list((Hash.t, Hash.t)) =>
  Attachment.entries_for(att, ~aspect=aspect_id, ~procedure=procedure_id)
  |> List.filter_map(((src, v)) =>
       switch (v) {
       | Attachment.Translation_target(tgt) => Some((src, tgt))
       | _ => None
       }
     )
  |> List.sort(((a, _), (b, _)) => String.compare(a, b));

let peek_translation =
    (att: Attachment.t, source: Hash.t): option(Hash.t) =>
  switch (
    Attachment.peek(
      att,
      ~target=source,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
    )
  ) {
  | Some(Attachment.Translation_target(h)) => Some(h)
  | _ => None
  };
