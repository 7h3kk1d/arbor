/* Big-step STLC evaluator over hashes, memoized through Attachment.

   CBV weak head normal form, TAPL Ch. 9 semantics. Additions over the
   pure-lc evaluator:
     - Values are Lam(_, _), True, False.
     - If reduces its guard first (CBV-style), then picks the branch.

   Step budget, caching conventions mirror Lc_eval exactly:
     - Results cached as Eval_value / Eval_stuck (aspect_value).
     - Eval_step_limit is NOT cached — a higher budget might finish.

   Stored stlc definitions are guaranteed well-typed by the Store
   invariant, so Stuck is theoretically unreachable on closed
   definitions. We keep the case for defence in depth (evaluator is
   total). */

let aspect_id: Attachment.aspect_id = "stlc:eval";
let procedure_id: Attachment.procedure_id = "stlc:eval:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["stlc"],
};

type result =
  | Value(Hash.t)
  | Stuck(Hash.t)
  | StepLimit(Hash.t);

let result_of_aspect_value =
  fun
  | Attachment.Eval_value(h) => Value(h)
  | Attachment.Eval_stuck(h) => Stuck(h)
  | Attachment.Eval_step_limit(h) => StepLimit(h)
  | v =>
    failwith(
      "stlc_eval: unexpected aspect value: "
      ++ Attachment.show_aspect_value(v),
    );

let aspect_value_of_result =
  fun
  | Value(h) => Attachment.Eval_value(h)
  | Stuck(h) => Attachment.Eval_stuck(h)
  | StepLimit(h) => Attachment.Eval_step_limit(h);

exception Dangling_hash(Hash.t);
exception Not_stlc(Hash.t);

let peek_cache = (att: Attachment.t, h: Hash.t): option(result) =>
  Option.map(
    result_of_aspect_value,
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id),
  );

let lookup_stlc = (store: Store.t, h: Hash.t): Stlc_node.t =>
  switch (Store.lookup(store, h)) {
  | None => raise(Dangling_hash(h))
  | Some(Definition.Stlc(n)) => n
  | Some(Definition.Lc(_)) => raise(Not_stlc(h))
  };

type budget = {mutable remaining: int};

let default_step_limit = 10000;

let try_beta =
    (store: Store.t, ~fn_hash: Hash.t, ~arg_hash: Hash.t): option(Hash.t) =>
  switch (lookup_stlc(store, fn_hash)) {
  | Stlc_node.Lam(_ty, body_hash) =>
    switch (
      Store.reconstruct_stlc(store, body_hash),
      Store.reconstruct_stlc(store, arg_hash),
    ) {
    | (Some(body), Some(arg)) =>
      let reduced = Stlc_ast.beta(~body, ~arg);
      Some(Store.ingest_stlc(store, reduced));
    | _ => raise(Dangling_hash(fn_hash))
    }
  | _ => None
  };

let rec eval_with =
        (
          ~store: Store.t,
          ~att: Attachment.t,
          ~budget: budget,
          h: Hash.t,
        )
        : result =>
  switch (
    Attachment.get(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(v) => result_of_aspect_value(v)
  | None =>
    let result = eval_miss(~store, ~att, ~budget, h);
    switch (result) {
    | StepLimit(_) => ()
    | Value(_)
    | Stuck(_) =>
      Attachment.attach(
        att,
        ~target=h,
        ~aspect=aspect_id,
        ~procedure=procedure_id,
        aspect_value_of_result(result),
      )
    };
    result;
  }
and eval_miss = (~store, ~att, ~budget, h) => {
  let node = lookup_stlc(store, h);
  eval_node(~store, ~att, ~budget, ~host=h, node);
}
and eval_node = (~store, ~att, ~budget, ~host, node) =>
  switch (node) {
  | Stlc_node.Var(_) => Stuck(host)
  | Stlc_node.Lam(_, _) => Value(host)
  | Stlc_node.True => Value(host)
  | Stlc_node.False => Value(host)
  | Stlc_node.App(f_hash, a_hash) =>
    switch (eval_with(~store, ~att, ~budget, f_hash)) {
    | StepLimit(_) as r => r
    | Stuck(_) => Stuck(host)
    | Value(fn_value_hash) =>
      switch (eval_with(~store, ~att, ~budget, a_hash)) {
      | StepLimit(_) as r => r
      | Stuck(_) => Stuck(host)
      | Value(arg_value_hash) =>
        if (budget.remaining <= 0) {
          StepLimit(
            Store.register_stlc_node(
              store,
              Stlc_node.App(fn_value_hash, arg_value_hash),
            ),
          );
        } else {
          switch (
            try_beta(store, ~fn_hash=fn_value_hash, ~arg_hash=arg_value_hash)
          ) {
          | None => Stuck(host)
          | Some(reduced_hash) =>
            budget.remaining = budget.remaining - 1;
            eval_with(~store, ~att, ~budget, reduced_hash);
          };
        }
      }
    }
  | Stlc_node.If(c_hash, t_hash, e_hash) =>
    switch (eval_with(~store, ~att, ~budget, c_hash)) {
    | StepLimit(_) as r => r
    | Stuck(_) => Stuck(host)
    | Value(guard_hash) =>
      switch (lookup_stlc(store, guard_hash)) {
      | Stlc_node.True => eval_with(~store, ~att, ~budget, t_hash)
      | Stlc_node.False => eval_with(~store, ~att, ~budget, e_hash)
      | _ => Stuck(host)
      }
    }
  };

let eval =
    (~store: Store.t, ~att: Attachment.t, ~step_limit=default_step_limit, h): result => {
  let budget = {remaining: step_limit};
  eval_with(~store, ~att, ~budget, h);
};
