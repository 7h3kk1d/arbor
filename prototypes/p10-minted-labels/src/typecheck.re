/* Permissive bidirectional type-checker for p9.

   Three outcomes for any term:
     Well_typed(ty)              — fully typed, no holes encountered
     Well_typed_with_holes(ty)   — best-guess type; some subterm was a
                                   hole or otherwise not fully constrained
     Ill_typed(msg)              — hard mismatch (e.g. `1 + true`,
                                   `(\x:Int. x) "abc"`); rejected at ingest

   Hole semantics:
     - `check ctx Hole expected` always succeeds (a hole accepts any
       expected type) and reports has_holes=true.
     - `synth ctx Hole` returns `(Int, has_holes=true)` as a "best guess
       in the absence of context." The `has_holes` flag propagates up to
       the top-level so the caller can decide between Well_typed and
       Well_typed_with_holes.

   Permissiveness:
     When a downstream check would normally reject because an upstream
     has-holes term has an indeterminate type, the checker swallows the
     mismatch (since the holes might fill in a way that makes the program
     well-typed). It only emits Ill_typed when both sides are fully
     determinate (no holes in either) and they still don't match.

   No unification (yet). The "best guess" type carried through holes is
   produced by local rules: hole synth defaults to Int; If/Eq propagate
   from the first non-holey branch; App falls back to Int when the
   function position is holey. If practical use shows too many real
   programs landing in `Type_with_holes(Int)` when something more
   informative would help, escalate to ref-cell unification. */

let ( let* ) = Result.bind;

let aspect_id: Attachment.aspect_id = "typecheck";
let procedure_id: Attachment.procedure_id = "typecheck:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["p10"],
};

type check_result =
  | Well_typed(Ty.t)
  | Well_typed_with_holes(Ty.t)
  | Ill_typed(string);

/* Internal helpers return (type, has_holes, ()|err). The boolean
   propagates "any subterm was holey or stretched our type discipline"
   up to the top. */

let rec synth =
        (~ctx: list(Ty.t), t: Ast.t)
        : result((Ty.t, bool), string) =>
  switch (t) {
  | Ast.Hole => Ok((Ty.Int, true))
  | Ast.Int_lit(_) => Ok((Ty.Int, false))
  | Ast.Bool_lit(_) => Ok((Ty.Bool, false))
  | Ast.String_lit(_) => Ok((Ty.String, false))
  | Ast.Var(k) =>
    switch (List.nth_opt(ctx, k)) {
    | Some(ty) => Ok((ty, false))
    | None =>
      Error("unbound de-Bruijn index " ++ string_of_int(k))
    }
  | Ast.Lam(ty_arg, body) =>
    let* (body_ty, body_holes) = synth(~ctx=[ty_arg, ...ctx], body);
    Ok((Ty.Arrow(ty_arg, body_ty), body_holes));
  | Ast.App(f, a) =>
    let* (f_ty, f_holes) = synth(~ctx, f);
    switch (f_ty) {
    | Ty.Arrow(dom, cod) =>
      let* a_holes = check(~ctx, a, dom);
      Ok((cod, f_holes || a_holes));
    | _ =>
      if (f_holes) {
        /* function position was holey; we don't actually know its
           shape. Just synthesize the argument and produce a permissive
           best-guess type. */
        let* (_, a_holes) = synth(~ctx, a);
        Ok((Ty.Int, true || a_holes));
      } else {
        Error(
          "application: function position has type "
          ++ Ty.print(f_ty)
          ++ " which is not an arrow",
        );
      }
    };
  | Ast.Let(rhs, body) =>
    let* (rhs_ty, rhs_holes) = synth(~ctx, rhs);
    let* (body_ty, body_holes) = synth(~ctx=[rhs_ty, ...ctx], body);
    Ok((body_ty, rhs_holes || body_holes));
  | Ast.If(c, th, el) =>
    let* c_holes = check(~ctx, c, Ty.Bool);
    let* (t_ty, t_holes) = synth(~ctx, th);
    let* e_holes = check(~ctx, el, t_ty);
    Ok((t_ty, c_holes || t_holes || e_holes));
  | Ast.Pair(a, b) =>
    let* (a_ty, a_holes) = synth(~ctx, a);
    let* (b_ty, b_holes) = synth(~ctx, b);
    Ok((Ty.Product(a_ty, b_ty), a_holes || b_holes));
  | Ast.Fst(p) =>
    let* (p_ty, p_holes) = synth(~ctx, p);
    switch (p_ty) {
    | Ty.Product(a, _) => Ok((a, p_holes))
    | _ when p_holes => Ok((Ty.Int, true))
    | _ =>
      Error(
        "fst: argument has type "
        ++ Ty.print(p_ty)
        ++ " which is not a product",
      )
    };
  | Ast.Snd(p) =>
    let* (p_ty, p_holes) = synth(~ctx, p);
    switch (p_ty) {
    | Ty.Product(_, b) => Ok((b, p_holes))
    | _ when p_holes => Ok((Ty.Int, true))
    | _ =>
      Error(
        "snd: argument has type "
        ++ Ty.print(p_ty)
        ++ " which is not a product",
      )
    };
  | Ast.Prim(op, args) => synth_prim(~ctx, op, args)
  | Ast.Prim_call(id, args) => synth_prim_call(~ctx, id, args)
  | Ast.Tuple(items) =>
    let rec synth_all = (acc_ty, acc_holes, lst) =>
      switch (lst) {
      | [] => Ok((Ty.Tuple(List.rev(acc_ty)), acc_holes))
      | [t, ...rest] =>
        let* (t_ty, t_holes) = synth(~ctx, t);
        synth_all([t_ty, ...acc_ty], acc_holes || t_holes, rest);
      };
    synth_all([], false, items);
  | Ast.List_lit([]) =>
    /* Empty list with no context — best-guess Int element. */
    Ok((Ty.List(Ty.Int), true))
  | Ast.List_lit([first, ...rest]) =>
    let* (first_ty, first_holes) = synth(~ctx, first);
    let rec check_rest = (holes, lst) =>
      switch (lst) {
      | [] => Ok(holes)
      | [t, ...rest] =>
        let* t_holes = check(~ctx, t, first_ty);
        check_rest(holes || t_holes, rest);
      };
    let* rest_holes = check_rest(false, rest);
    Ok((Ty.List(first_ty), first_holes || rest_holes));
  | Ast.Record_lit(fields) =>
    let rec synth_all = (acc_ty, acc_holes, lst) =>
      switch (lst) {
      | [] => Ok((Ty.Record(List.rev(acc_ty)), acc_holes))
      | [(label_h, t), ...rest] =>
        let* (t_ty, t_holes) = synth(~ctx, t);
        synth_all([(label_h, t_ty), ...acc_ty], acc_holes || t_holes, rest);
      };
    synth_all([], false, fields);
  | Ast.Record_update(target, updates) =>
    let* (target_ty, target_holes) = synth(~ctx, target);
    switch (target_ty) {
    | Ty.Record(existing_fields) =>
      let rec check_updates = (holes, lst) =>
        switch (lst) {
        | [] => Ok(holes)
        | [(label_h, t), ...rest] =>
          switch (List.assoc_opt(label_h, existing_fields)) {
          | None =>
            if (target_holes) {
              /* Permissive: holes may yet make this make sense. */
              let* (_, t_holes) = synth(~ctx, t);
              check_updates(holes || t_holes || true, rest);
            } else {
              Error(
                "record update: label "
                ++ Hash.short(label_h)
                ++ " not in target record",
              );
            }
          | Some(expected) =>
            let* t_holes = check(~ctx, t, expected);
            check_updates(holes || t_holes, rest);
          }
        };
      let* update_holes = check_updates(false, updates);
      Ok((target_ty, target_holes || update_holes));
    | _ when target_holes => Ok((Ty.Int, true))
    | _ =>
      Error(
        "record update: target has type "
        ++ Ty.print(target_ty)
        ++ " which is not a record",
      )
    };
  | Ast.Project_field(target, label_h) =>
    let* (target_ty, target_holes) = synth(~ctx, target);
    switch (target_ty) {
    | Ty.Record(fields) =>
      switch (List.assoc_opt(label_h, fields)) {
      | Some(ty) => Ok((ty, target_holes))
      | None when target_holes => Ok((Ty.Int, true))
      | None =>
        Error(
          "field projection: label "
          ++ Hash.short(label_h)
          ++ " not in record type "
          ++ Ty.print(target_ty),
        )
      }
    | _ when target_holes => Ok((Ty.Int, true))
    | _ =>
      Error(
        "field projection: target has type "
        ++ Ty.print(target_ty)
        ++ " which is not a record",
      )
    };
  | Ast.Project_index(target, i) =>
    let* (target_ty, target_holes) = synth(~ctx, target);
    switch (target_ty) {
    | Ty.Tuple(items) =>
      switch (List.nth_opt(items, i)) {
      | Some(ty) => Ok((ty, target_holes))
      | None when target_holes => Ok((Ty.Int, true))
      | None =>
        Error(
          "tuple index "
          ++ string_of_int(i)
          ++ " out of bounds for "
          ++ Ty.print(target_ty),
        )
      }
    /* Transitional: also accept binary Product when i is 0 or 1. */
    | Ty.Product(a, _) when i == 0 => Ok((a, target_holes))
    | Ty.Product(_, b) when i == 1 => Ok((b, target_holes))
    | _ when target_holes => Ok((Ty.Int, true))
    | _ =>
      Error(
        "tuple index: target has type "
        ++ Ty.print(target_ty)
        ++ " which is not a tuple",
      )
    };
  }

and synth_prim_call =
    (~ctx: list(Ty.t), id: string, args: list(Ast.t))
    : result((Ty.t, bool), string) =>
  switch (Primitive_registry.find(id)) {
  | None => Error("unknown primitive: " ++ id)
  | Some({ty, _}) =>
    let arg_tys = Primitive_registry.arg_types(ty);
    let ret_ty = Primitive_registry.return_type(ty);
    let expected_arity = List.length(arg_tys);
    let actual_arity = List.length(args);
    if (expected_arity != actual_arity) {
      Error(
        "primitive "
        ++ id
        ++ ": expected "
        ++ string_of_int(expected_arity)
        ++ " arguments, got "
        ++ string_of_int(actual_arity),
      );
    } else {
      let rec check_all = (acc_holes, args, tys) =>
        switch (args, tys) {
        | ([], []) => Ok((ret_ty, acc_holes))
        | ([a, ...rest_a], [t, ...rest_t]) =>
          let* h = check(~ctx, a, t);
          check_all(acc_holes || h, rest_a, rest_t);
        | _ => Error("primitive arity mismatch (internal)")
        };
      check_all(false, args, arg_tys);
    }
  }

and check =
    (~ctx: list(Ty.t), t: Ast.t, expected: Ty.t)
    : result(bool, string) =>
  switch (t) {
  | Ast.Hole => Ok(true)
  | _ =>
    let* (got, got_holes) = synth(~ctx, t);
    if (got_holes) {
      Ok(true);
    } else if (Ty.equal(got, expected)) {
      Ok(false);
    } else {
      Error(
        "expected " ++ Ty.print(expected) ++ ", got " ++ Ty.print(got),
      );
    }
  }

and synth_prim =
    (~ctx: list(Ty.t), op: Surface_ast.prim_op, args: list(Ast.t))
    : result((Ty.t, bool), string) => {
  let need_arity = n =>
    if (List.length(args) == n) {
      Ok();
    } else {
      Error(
        "operator "
        ++ Surface_ast.prim_op_to_string(op)
        ++ ": expected "
        ++ string_of_int(n)
        ++ " arguments, got "
        ++ string_of_int(List.length(args)),
      );
    };
  let nth = i => List.nth(args, i);
  let check_two = (~a_ty, ~b_ty, ~result_ty) => {
    let* () = need_arity(2);
    let* h1 = check(~ctx, nth(0), a_ty);
    let* h2 = check(~ctx, nth(1), b_ty);
    Ok((result_ty, h1 || h2));
  };
  let check_one = (~arg_ty, ~result_ty) => {
    let* () = need_arity(1);
    let* h1 = check(~ctx, nth(0), arg_ty);
    Ok((result_ty, h1));
  };
  switch (op) {
  | Surface_ast.Add
  | Surface_ast.Sub
  | Surface_ast.Mul
  | Surface_ast.Div
  | Surface_ast.Mod =>
    check_two(~a_ty=Ty.Int, ~b_ty=Ty.Int, ~result_ty=Ty.Int)
  | Surface_ast.And
  | Surface_ast.Or =>
    check_two(~a_ty=Ty.Bool, ~b_ty=Ty.Bool, ~result_ty=Ty.Bool)
  | Surface_ast.Not => check_one(~arg_ty=Ty.Bool, ~result_ty=Ty.Bool)
  | Surface_ast.Concat =>
    check_two(~a_ty=Ty.String, ~b_ty=Ty.String, ~result_ty=Ty.String)
  | Surface_ast.Eq =>
    let* () = need_arity(2);
    let* (a_ty, a_holes) = synth(~ctx, nth(0));
    if (a_holes) {
      let* (_, b_holes) = synth(~ctx, nth(1));
      Ok((Ty.Bool, true || b_holes));
    } else {
      switch (a_ty) {
      | Ty.Int
      | Ty.Bool
      | Ty.String =>
        let* b_holes = check(~ctx, nth(1), a_ty);
        Ok((Ty.Bool, b_holes));
      | _ =>
        Error(
          "==: cannot compare values of type " ++ Ty.print(a_ty),
        )
      };
    };
  };
};

let check_top = (t: Ast.t): check_result =>
  switch (synth(~ctx=[], t)) {
  | Ok((ty, false)) => Well_typed(ty)
  | Ok((ty, true)) => Well_typed_with_holes(ty)
  | Error(msg) => Ill_typed(msg)
  };

/* ==================== Aspect procedure ==================== */

type error =
  | Dangling_hash(Hash.t)
  | Type_error(string);

let error_to_string =
  fun
  | Dangling_hash(h) => "internal: dangling hash " ++ Hash.short(h)
  | Type_error(msg) => "type error: " ++ msg;

/* peek_cache resolves the cached type-hash back to a Ty.t via the
   Store. If the type-hash is dangling (cannot happen in normal
   operation, since attach_result always registers it first), we fall
   through as a cache miss. */
let peek_cache =
    (~store: Store.t, att: Attachment.t, h: Hash.t)
    : option(check_result) => {
  /* Follow Named_term/Named_type wrappers: aspects are cached on the
     substructure body, not on the minted wrapper. */
  let h = Store.unwrap_named(store, h);
  switch (
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(Attachment.Type_of(ty_h)) =>
    Option.map(ty => Well_typed(ty), Store.lookup_type(store, ty_h))
  | Some(Attachment.Type_with_holes(ty_h)) =>
    Option.map(
      ty => Well_typed_with_holes(ty),
      Store.lookup_type(store, ty_h),
    )
  | _ => None
  };
};

/* attach_result registers the inferred type as a Definition.Type in
   the Store before storing the resulting hash in the aspect entry.
   This is the round-trip the aspect store needs: a future peek can
   resolve back to the Ty.t value. */
let attach_result =
    (~store: Store.t, att: Attachment.t, ~target: Hash.t, r: check_result)
    : unit =>
  switch (r) {
  | Well_typed(ty) =>
    let ty_h = Store.register_type(store, ty);
    Attachment.attach(
      att,
      ~target,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
      Attachment.Type_of(ty_h),
    );
  | Well_typed_with_holes(ty) =>
    let ty_h = Store.register_type(store, ty);
    Attachment.attach(
      att,
      ~target,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
      Attachment.Type_with_holes(ty_h),
    );
  | Ill_typed(_) => ()
  };

let check_hash =
    (~store: Store.t, ~att: Attachment.t, h: Hash.t)
    : result((check_result, bool /* was_cached */), error) =>
  switch (peek_cache(~store, att, h)) {
  | Some(r) => Ok((r, true))
  | None =>
    switch (Store.reconstruct(store, h)) {
    | None => Error(Dangling_hash(h))
    | Some(ast) =>
      let r = check_top(ast);
      attach_result(~store, att, ~target=h, r);
      Ok((r, false));
    }
  };
