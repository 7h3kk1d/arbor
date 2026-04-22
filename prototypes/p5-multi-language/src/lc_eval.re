/* Big-step lambda-calculus evaluator over hashes, memoized through
   Attachment. Carried from p4 (renamed from Eval to Lc_eval) and
   updated to the multi-language Store API: lookups walk through
   Definition.Lc(...) to get at Lc_node variants. */

let aspect_id: Attachment.aspect_id = "lc:eval";
let procedure_id: Attachment.procedure_id = "lc:eval:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["lc"],
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
      "lc_eval: unexpected aspect value: " ++ Attachment.show_aspect_value(v),
    );

let aspect_value_of_result =
  fun
  | Value(h) => Attachment.Eval_value(h)
  | Stuck(h) => Attachment.Eval_stuck(h)
  | StepLimit(h) => Attachment.Eval_step_limit(h);

exception Dangling_hash(Hash.t);
exception Not_lc(Hash.t);

let peek_cache = (att: Attachment.t, h: Hash.t): option(result) =>
  Option.map(
    result_of_aspect_value,
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id),
  );

let lookup_lc = (store: Store.t, h: Hash.t): Lc_node.t =>
  switch (Store.lookup(store, h)) {
  | None => raise(Dangling_hash(h))
  | Some(Definition.Lc(n)) => n
  | Some(Definition.Arith(_)) => raise(Not_lc(h))
  };

type budget = {mutable remaining: int};

let default_step_limit = 10000;

let try_beta =
    (store: Store.t, ~fn_hash: Hash.t, ~arg_hash: Hash.t): option(Hash.t) =>
  switch (lookup_lc(store, fn_hash)) {
  | Lc_node.Lam(body_hash) =>
    switch (
      Store.reconstruct_lc(store, body_hash),
      Store.reconstruct_lc(store, arg_hash),
    ) {
    | (Some(body), Some(arg)) =>
      let reduced = Lc_ast.beta(~body, ~arg);
      Some(Store.ingest_lc(store, reduced));
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
  let node = lookup_lc(store, h);
  eval_node(~store, ~att, ~budget, ~host=h, node);
}
and eval_node = (~store, ~att, ~budget, ~host, node) =>
  switch (node) {
  | Lc_node.Var(_) => Stuck(host)
  | Lc_node.Lam(_) => Value(host)
  | Lc_node.App(f_hash, a_hash) =>
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
            Store.register_lc_node(
              store,
              Lc_node.App(fn_value_hash, arg_value_hash),
            ),
          );
        } else {
          switch (try_beta(store, ~fn_hash=fn_value_hash, ~arg_hash=arg_value_hash)) {
          | None => Stuck(host)
          | Some(reduced_hash) =>
            budget.remaining = budget.remaining - 1;
            eval_with(~store, ~att, ~budget, reduced_hash);
          };
        }
      }
    }
  };

let eval =
    (~store: Store.t, ~att: Attachment.t, ~step_limit=default_step_limit, h): result => {
  let budget = {remaining: step_limit};
  eval_with(~store, ~att, ~budget, h);
};
