/* Big-step arithmetic evaluator over hashes, memoized through
   Attachment. Carried from p3 (renamed from Eval to Arith_eval) and
   updated to the multi-language Store API — pattern-matching now goes
   through Definition.Arith(...) on the way to Arith_node variants. */

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
  | Attachment.Eval_stuck(h) => Stuck(h)
  | v =>
    failwith(
      "arith_eval: unexpected aspect value: " ++ Attachment.show_aspect_value(v),
    );

let aspect_value_of_result =
  fun
  | Value(h) => Attachment.Eval_value(h)
  | Stuck(h) => Attachment.Eval_stuck(h);

exception Dangling_hash(Hash.t);
exception Not_arith(Hash.t);

let peek_cache = (att: Attachment.t, h: Hash.t): option(result) =>
  Option.map(
    result_of_aspect_value,
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id),
  );

let lookup_arith = (store: Store.t, h: Hash.t): Arith_node.t =>
  switch (Store.lookup(store, h)) {
  | None => raise(Dangling_hash(h))
  | Some(Definition.Arith(n)) => n
  | Some(Definition.Lc(_)) => raise(Not_arith(h))
  };

let rec eval = (~store: Store.t, ~att: Attachment.t, h: Hash.t): result =>
  switch (
    Attachment.get(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
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
and eval_miss = (~store, ~att, h) => {
  let node = lookup_arith(store, h);
  eval_node(~store, ~att, ~host=h, node);
}
and eval_node = (~store, ~att, ~host, node) =>
  switch (node) {
  | Arith_node.True
  | Arith_node.False
  | Arith_node.Zero => Value(host)
  | Arith_node.Succ(ch) =>
    switch (eval(~store, ~att, ch)) {
    | Stuck(_) as s => s
    | Value(v_hash) =>
      let v_node = lookup_arith(store, v_hash);
      if (Arith_node.is_numeric_top(v_node)) {
        Value(Store.register_arith_node(store, Arith_node.Succ(v_hash)));
      } else {
        Stuck(host);
      };
    }
  | Arith_node.Pred(ch) =>
    switch (eval(~store, ~att, ch)) {
    | Stuck(_) as s => s
    | Value(v_hash) =>
      switch (lookup_arith(store, v_hash)) {
      | Arith_node.Zero => Value(v_hash)
      | Arith_node.Succ(nv_hash) => Value(nv_hash)
      | _ => Stuck(host)
      }
    }
  | Arith_node.IsZero(ch) =>
    switch (eval(~store, ~att, ch)) {
    | Stuck(_) as s => s
    | Value(v_hash) =>
      switch (lookup_arith(store, v_hash)) {
      | Arith_node.Zero =>
        Value(Store.register_arith_node(store, Arith_node.True))
      | Arith_node.Succ(_) =>
        Value(Store.register_arith_node(store, Arith_node.False))
      | _ => Stuck(host)
      }
    }
  | Arith_node.If(c, thn, els) =>
    switch (eval(~store, ~att, c)) {
    | Stuck(_) as s => s
    | Value(c_hash) =>
      switch (lookup_arith(store, c_hash)) {
      | Arith_node.True => eval(~store, ~att, thn)
      | Arith_node.False => eval(~store, ~att, els)
      | _ => Stuck(host)
      }
    }
  };
