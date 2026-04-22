/* arith → lc translator via Church encoding.

   A named, versioned procedure in the Attachment sense (identity
   `arith-to-lc-church:translate:v1`). The substrate's first concrete
   translator per docs/design/05-translation.md: output aspect is a
   derived entry on the SOURCE (arith) definition, value is the
   translated lc hash. Second runs on the same source return the cached
   hash without reingesting.

   Strategy: deep-walk the reconstructed Arith_ast.t into a Lc_ast.t
   whose evaluation agrees with arith evaluation under a standard
   Church interpretation. Booleans and numerals are the usual encodings
   (TRU = λt.λf.t, ZERO = λs.λz.z, SUCC = λn.λs.λz. s (n s z)). Pred
   uses Kleene's combinator. `if c then t else e` emits plain
   application (c t e) because a Church boolean consumes its two
   branches directly — no separate `if` combinator is needed.

   The translator carries no state; the Store's hash-cons deduplicates
   the fixed combinator ASTs automatically across calls. Eager
   transitive closure (§Transitive dependencies in the design doc) is
   trivially satisfied here because p3/p4's inline-at-resolution stance
   means arith definitions are stored as closed deep trees with no
   cross-definition references — a single-definition walk is enough. */

let aspect_id: Attachment.aspect_id = "translation-to-lc";
let procedure_id: Attachment.procedure_id =
  "arith-to-lc-church:translate:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["arith"],
};

type error =
  | Not_arith(Hash.t)
  | Dangling(Hash.t);

let error_to_string =
  fun
  | Not_arith(h) =>
    "translation requires an arith definition, but "
    ++ Hash.short(h)
    ++ " is not arith"
  | Dangling(h) =>
    "translation source is not in the store: " ++ Hash.short(h);

/* ==================== Church encodings (de Bruijn) ==================== */

/* TRU = λt. λf. t → under \t.\f.: t=Var(1) */
let church_true: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(1)));

/* FLS = λt. λf. f → under \t.\f.: f=Var(0) */
let church_false: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(0)));

/* ZERO = λs. λz. z → structurally identical to FLS, but distinct role */
let church_zero: Lc_ast.t = Lc_ast.Lam(Lc_ast.Lam(Lc_ast.Var(0)));

/* SUCC = λn. λs. λz. s (n s z)
   Under \n.\s.\z.: n=Var(2), s=Var(1), z=Var(0)
   n s z        = App(App(Var(2), Var(1)), Var(0))
   s (n s z)    = App(Var(1), App(App(Var(2), Var(1)), Var(0)))
   full SUCC    = Lam(Lam(Lam(...)))
*/
let church_succ: Lc_ast.t =
  Lc_ast.Lam(
    Lc_ast.Lam(
      Lc_ast.Lam(
        Lc_ast.App(
          Lc_ast.Var(1),
          Lc_ast.App(Lc_ast.App(Lc_ast.Var(2), Lc_ast.Var(1)), Lc_ast.Var(0)),
        ),
      ),
    ),
  );

/* ISZERO = λn. n (λx. FLS) TRU
   Under \n.: n=Var(0)
   \x. FLS is Lam with FLS's body pushed under one more binder; FLS is
   closed so its indices are unchanged: Lam(FLS) = Lam(Lam(Lam(Var(0)))).
   TRU also closed: Lam(Lam(Var(1))).
   body = App(App(Var(0), Lam(church_false)), church_true)
*/
let church_iszero: Lc_ast.t =
  Lc_ast.Lam(
    Lc_ast.App(
      Lc_ast.App(Lc_ast.Var(0), Lc_ast.Lam(church_false)),
      church_true,
    ),
  );

/* PRED = λn. λf. λx. n (λg. λh. h (g f)) (λu. x) (λu. u)
   Under \n.\f.\x.: n=Var(2), f=Var(1), x=Var(0)
   Inside \g.\h.:    g=Var(1), h=Var(0), x=Var(2), f=Var(3), n=Var(4)
     g f         = App(Var(1), Var(3))
     h (g f)     = App(Var(0), App(Var(1), Var(3)))
   wrapped       = Lam(Lam(App(Var(0), App(Var(1), Var(3)))))
   Inside \u.:   u=Var(0), x=Var(1), f=Var(2), n=Var(3)
     \u. x       = Lam(Var(1))
     \u. u       = Lam(Var(0))
   body = App(App(App(Var(2), wrapped), Lam(Var(1))), Lam(Var(0)))
*/
let church_pred: Lc_ast.t = {
  let wrapped =
    Lc_ast.Lam(
      Lc_ast.Lam(
        Lc_ast.App(Lc_ast.Var(0), Lc_ast.App(Lc_ast.Var(1), Lc_ast.Var(3))),
      ),
    );
  let x_thunk = Lc_ast.Lam(Lc_ast.Var(1));
  let id_thunk = Lc_ast.Lam(Lc_ast.Var(0));
  Lc_ast.Lam(
    Lc_ast.Lam(
      Lc_ast.Lam(
        Lc_ast.App(
          Lc_ast.App(Lc_ast.App(Lc_ast.Var(2), wrapped), x_thunk),
          id_thunk,
        ),
      ),
    ),
  );
};

/* ==================== Translation ==================== */

let rec translate_ast = (a: Arith_ast.t): Lc_ast.t =>
  switch (a) {
  | Arith_ast.True => church_true
  | Arith_ast.False => church_false
  | Arith_ast.Zero => church_zero
  | Arith_ast.Succ(n) => Lc_ast.App(church_succ, translate_ast(n))
  | Arith_ast.Pred(n) => Lc_ast.App(church_pred, translate_ast(n))
  | Arith_ast.IsZero(n) => Lc_ast.App(church_iszero, translate_ast(n))
  | Arith_ast.If(c, t, e) =>
    Lc_ast.App(
      Lc_ast.App(translate_ast(c), translate_ast(t)),
      translate_ast(e),
    )
  };

/* Returns (target, was_cached). Cache hit: returns the cached target
   without touching Store. Cache miss: reconstructs the source's arith
   AST, translates, ingests, records the aspect, returns the new target.
   The aspect is Derived so the first write wins for a given source. */
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
    | Some(Definition.Lc(_)) => Error(Not_arith(source))
    | Some(Definition.Arith(_)) =>
      switch (Store.reconstruct_arith(store, source)) {
      | None => Error(Dangling(source))
      | Some(arith) =>
        let lc_ast = translate_ast(arith);
        let target = Store.ingest_lc(store, lc_ast);
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

/* All cached (source → target) pairs for this translator identity.
   Sorted by source hash for determinism. Powers `:translations`. */
let all_translations =
    (att: Attachment.t): list((Hash.t, Hash.t)) => {
  let pairs =
    Attachment.entries_for(att, ~aspect=aspect_id, ~procedure=procedure_id)
    |> List.filter_map(((src, v)) =>
         switch (v) {
         | Attachment.Translation_target(tgt) => Some((src, tgt))
         | _ => None
         }
       );
  List.sort(((a, _), (b, _)) => String.compare(a, b), pairs);
};

/* Look up a single translation by source hash, without touching
   hit/miss counters. */
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
