/* Big-step evaluator for untyped λ-calculus over hashes, memoized
   through the Attachment layer.

   Semantics: call-by-value, weak head normal form (WHNF). A term is a
   value when it has a Lam at the head; reduction stops under binders.
   Beta-redexes are reduced in the standard TAPL de-Bruijn-indices way
   (Ast.beta). A hash-level β does `reconstruct → beta → ingest` so the
   resulting term is in the Store and has its own hash.

   Stuck cases in untyped LC are narrow: applying a Var (a free or
   "unbound-after-substitution" variable) to something. For a closed,
   well-formed term this shouldn't arise — we classify it `Stuck`
   anyway so the evaluator is total.

   Non-termination is bounded by a step counter threaded through the
   call tree. Hitting the bound yields a `StepLimit` result; it is NOT
   written back to the cache (a partial result must not shadow a future
   successful evaluation under a higher bound).

   Step accounting: each β-reduction counts one step. Sub-evaluations
   of function and argument share the counter. */

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
  | Attachment.Eval_step_limit(h) => StepLimit(h);

let aspect_value_of_result =
  fun
  | Value(h) => Attachment.Eval_value(h)
  | Stuck(h) => Attachment.Eval_stuck(h)
  | StepLimit(h) => Attachment.Eval_step_limit(h);

exception Dangling_hash(Hash.t);

let peek_cache = (att: Attachment.t, h: Hash.t): option(result) =>
  Option.map(
    result_of_aspect_value,
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id),
  );

/* A step budget threaded through the evaluator. Mutable so that nested
   recursive calls share the same counter without plumbing it through
   every return. */
type budget = {mutable remaining: int};

let default_step_limit = 10000;

/* Attempt to β-reduce an App whose function part has been reduced to
   a value. Returns None if the value is not a Lam (stuck case). */
let try_beta =
    (store: Store.t, ~fn_hash: Hash.t, ~arg_hash: Hash.t): option(Hash.t) =>
  switch (Store.lookup(store, fn_hash)) {
  | Some(Node.Lam(body_hash)) =>
    switch (Store.reconstruct(store, body_hash), Store.reconstruct(store, arg_hash)) {
    | (Some(body), Some(arg)) =>
      let reduced = Ast.beta(~body, ~arg);
      Some(Store.ingest(store, reduced));
    | _ => raise(Dangling_hash(fn_hash))
    }
  | Some(_) => None
  | None => raise(Dangling_hash(fn_hash))
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
    /* Never cache StepLimit: a future call with a higher budget may
       turn this into a Value. */
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
and eval_miss = (~store, ~att, ~budget, h) =>
  switch (Store.lookup(store, h)) {
  | None => raise(Dangling_hash(h))
  | Some(node) => eval_node(~store, ~att, ~budget, ~host=h, node)
  }
and eval_node = (~store, ~att, ~budget, ~host, node) =>
  switch (node) {
  | Node.Var(_) => Stuck(host) /* free variable at top level */
  | Node.Lam(_) => Value(host) /* WHNF: Lam is a value */
  | Node.App(f_hash, a_hash) =>
    switch (eval_with(~store, ~att, ~budget, f_hash)) {
    | StepLimit(_) as r => r
    | Stuck(_) => Stuck(host)
    | Value(fn_value_hash) =>
      /* CBV: reduce the argument before β. */
      switch (eval_with(~store, ~att, ~budget, a_hash)) {
      | StepLimit(_) as r => r
      | Stuck(_) => Stuck(host)
      | Value(arg_value_hash) =>
        if (budget.remaining <= 0) {
          StepLimit(
            Store.register_node(
              store,
              Node.App(fn_value_hash, arg_value_hash),
            ),
          );
        } else {
          switch (try_beta(store, ~fn_hash=fn_value_hash, ~arg_hash=arg_value_hash)) {
          | None =>
            /* Applied a non-λ value (a Var that somehow survived to a
               value position). In untyped LC on closed terms this
               shouldn't happen; classify as stuck. */
            Stuck(host)
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

let print_result = (~pp: Hash.t => string, r: result): string =>
  switch (r) {
  | Value(h) => pp(h)
  | Stuck(h) => "⟂ stuck at: " ++ pp(h)
  | StepLimit(h) => "… step limit reached at: " ++ pp(h)
  };
