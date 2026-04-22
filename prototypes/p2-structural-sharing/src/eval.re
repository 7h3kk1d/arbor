/* Big-step evaluator over hashes, memoized through the Attachment
   layer. Values produced during evaluation are registered in the
   Store; the eval-cache aspect records (target_hash → Eval_value |
   Eval_stuck) entries keyed by procedure identity. */

let aspect_id: Attachment.aspect_id = "arith:eval";
let procedure_id: Attachment.procedure_id = "arith:eval:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["arith"],
};

type result =
  | Value(Hash.t)
  | Stuck(Hash.t);

let result_of_aspect_value =
  fun
  | Attachment.Eval_value(h) => Value(h)
  | Attachment.Eval_stuck(h) => Stuck(h);

let aspect_value_of_result =
  fun
  | Value(h) => Attachment.Eval_value(h)
  | Stuck(h) => Attachment.Eval_stuck(h);

/* Signaled when the evaluator encounters a hash that isn't in the
   Store. Hashes produced by Store.ingest or Store.register_node are
   always resolvable, so this indicates a caller-side invariant
   violation. */
exception Dangling_hash(Hash.t);

let peek_cache = (att: Attachment.t, h: Hash.t): option(result) =>
  Option.map(
    result_of_aspect_value,
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id),
  );

let rec eval = (~store: Store.t, ~att: Attachment.t, h: Hash.t): result =>
  switch (Attachment.get(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)) {
  | Some(v) => result_of_aspect_value(v)
  | None =>
    let result = eval_miss(~store, ~att, h);
    Attachment.attach(
      att,
      ~target=h,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
      aspect_value_of_result(result),
    );
    result;
  }
and eval_miss = (~store, ~att, h) =>
  switch (Store.lookup(store, h)) {
  | None => raise(Dangling_hash(h))
  | Some(node) => eval_node(~store, ~att, ~host=h, node)
  }
and eval_node = (~store, ~att, ~host, node) =>
  switch (node) {
  | Node.True
  | Node.False
  | Node.Zero => Value(host)
  | Node.Succ(ch) =>
    switch (eval(~store, ~att, ch)) {
    | Stuck(_) as s => s
    | Value(v_hash) =>
      switch (Store.lookup(store, v_hash)) {
      | None => raise(Dangling_hash(v_hash))
      | Some(v_node) when Node.is_numeric_top(v_node) =>
        Value(Store.register_node(store, Node.Succ(v_hash)))
      | Some(_) => Stuck(host)
      }
    }
  | Node.Pred(ch) =>
    switch (eval(~store, ~att, ch)) {
    | Stuck(_) as s => s
    | Value(v_hash) =>
      switch (Store.lookup(store, v_hash)) {
      | None => raise(Dangling_hash(v_hash))
      | Some(Node.Zero) => Value(v_hash)
      | Some(Node.Succ(nv_hash)) => Value(nv_hash)
      | Some(_) => Stuck(host)
      }
    }
  | Node.IsZero(ch) =>
    switch (eval(~store, ~att, ch)) {
    | Stuck(_) as s => s
    | Value(v_hash) =>
      switch (Store.lookup(store, v_hash)) {
      | None => raise(Dangling_hash(v_hash))
      | Some(Node.Zero) => Value(Store.register_node(store, Node.True))
      | Some(Node.Succ(_)) => Value(Store.register_node(store, Node.False))
      | Some(_) => Stuck(host)
      }
    }
  | Node.If(c, thn, els) =>
    switch (eval(~store, ~att, c)) {
    | Stuck(_) as s => s
    | Value(c_hash) =>
      switch (Store.lookup(store, c_hash)) {
      | None => raise(Dangling_hash(c_hash))
      | Some(Node.True) => eval(~store, ~att, thn)
      | Some(Node.False) => eval(~store, ~att, els)
      | Some(_) => Stuck(host)
      }
    }
  };

let print_result = (store: Store.t, r: result): string =>
  switch (r) {
  | Value(h) => Pretty.print(store, h)
  | Stuck(h) => "⟂ stuck at: " ++ Pretty.print(store, h)
  };
