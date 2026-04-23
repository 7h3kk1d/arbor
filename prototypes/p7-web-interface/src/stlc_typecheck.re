/* STLC type-checker — TAPL Ch. 8+9 typing rules, straight synthesis.

     Γ ⊢ true : Bool                      (T-True)
     Γ ⊢ false : Bool                     (T-False)
     Γ ⊢ c : Bool  Γ ⊢ t : T  Γ ⊢ e : T
     ─────────────────────────────────    (T-If)
           Γ ⊢ if c then t else e : T

     Γ(k) = T
     ────────                             (T-Var, de Bruijn)
     Γ ⊢ k : T

     Γ, T1 ⊢ body : T2
     ────────────────────────             (T-Abs)
     Γ ⊢ \:T1. body : T1 → T2

     Γ ⊢ f : T1 → T2   Γ ⊢ a : T1
     ───────────────────────────         (T-App)
           Γ ⊢ f a : T2

   `ctx` is a list of types in de-Bruijn order: `List.nth(ctx, 0)` is
   the type of the innermost binder.

   Also exposes `check`, an aspect-procedure wrapper that caches the
   inferred type on the `stlc:type-check` aspect (value Type_of(Ty.t),
   procedure `stlc:type-check:v1`). Because the Store invariant is
   "every stored stlc definition type-checks," the aspect lookup cannot
   fail on anything ingested — but the function still returns a
   `result` uniformly so callers don't special-case. */

let ( let* ) = Result.bind;

let aspect_id: Attachment.aspect_id = "stlc:type-check";
let procedure_id: Attachment.procedure_id = "stlc:type-check:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["stlc"],
};

let rec infer =
        (~ctx: list(Ty.t), t: Stlc_ast.t): result(Ty.t, string) =>
  switch (t) {
  | Stlc_ast.True => Ok(Ty.Bool)
  | Stlc_ast.False => Ok(Ty.Bool)
  | Stlc_ast.Var(k) =>
    switch (List.nth_opt(ctx, k)) {
    | Some(ty) => Ok(ty)
    | None =>
      Error(
        "unbound de-Bruijn index " ++ string_of_int(k) ++ " (open term)",
      )
    }
  | Stlc_ast.Lam(ty, body) =>
    let* body_ty = infer(~ctx=[ty, ...ctx], body);
    Ok(Ty.Arrow(ty, body_ty));
  | Stlc_ast.App(f, a) =>
    let* f_ty = infer(~ctx, f);
    let* a_ty = infer(~ctx, a);
    switch (f_ty) {
    | Ty.Arrow(dom, cod) =>
      if (Ty.equal(dom, a_ty)) {
        Ok(cod);
      } else {
        Error(
          "application: function expects "
          ++ Ty.print(dom)
          ++ " but argument has type "
          ++ Ty.print(a_ty),
        );
      }
    | _ =>
      Error(
        "application: function position has type "
        ++ Ty.print(f_ty)
        ++ " which is not an arrow",
      )
    };
  | Stlc_ast.If(c, th, el) =>
    let* c_ty = infer(~ctx, c);
    if (!Ty.equal(c_ty, Ty.Bool)) {
      Error(
        "if: guard has type " ++ Ty.print(c_ty) ++ ", expected Bool",
      );
    } else {
      let* t_ty = infer(~ctx, th);
      let* e_ty = infer(~ctx, el);
      if (Ty.equal(t_ty, e_ty)) {
        Ok(t_ty);
      } else {
        Error(
          "if: branches have different types "
          ++ Ty.print(t_ty)
          ++ " vs "
          ++ Ty.print(e_ty),
        );
      };
    }
  };

/* ==================== Aspect procedure ==================== */

type error =
  | Dangling_hash(Hash.t)
  | Not_stlc(Hash.t)
  | Type_error(string);

let error_to_string =
  fun
  | Dangling_hash(h) => "internal: dangling hash " ++ Hash.short(h)
  | Not_stlc(h) => "not an stlc definition: " ++ Hash.short(h)
  | Type_error(msg) => "type error: " ++ msg;

let peek_cache = (att: Attachment.t, h: Hash.t): option(Ty.t) =>
  switch (
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(Attachment.Type_of(ty)) => Some(ty)
  | _ => None
  };

let check =
    (~store: Store.t, ~att: Attachment.t, h: Hash.t)
    : result((Ty.t, bool /*was_cached*/), error) =>
  switch (peek_cache(att, h)) {
  | Some(ty) => Ok((ty, true))
  | None =>
    switch (Store.lookup(store, h)) {
    | None => Error(Dangling_hash(h))
    | Some(Definition.Lc(_)) => Error(Not_stlc(h))
    | Some(Definition.Stlc(_)) =>
      switch (Store.reconstruct_stlc(store, h)) {
      | None => Error(Dangling_hash(h))
      | Some(ast) =>
        switch (infer(~ctx=[], ast)) {
        | Error(msg) => Error(Type_error(msg))
        | Ok(ty) =>
          Attachment.attach(
            att,
            ~target=h,
            ~aspect=aspect_id,
            ~procedure=procedure_id,
            Attachment.Type_of(ty),
          );
          Ok((ty, false));
        }
      }
    }
  };

/* Enumerate all cached type-check entries for REPL ':types'. */
let all_types = (att: Attachment.t): list((Hash.t, Ty.t)) =>
  Attachment.entries_for(att, ~aspect=aspect_id, ~procedure=procedure_id)
  |> List.filter_map(((h, v)) =>
       switch (v) {
       | Attachment.Type_of(ty) => Some((h, ty))
       | _ => None
       }
     )
  |> List.sort(((a, _), (b, _)) => compare(a, b));
