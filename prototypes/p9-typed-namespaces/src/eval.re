/* CBV evaluator. Walks an Ast.t (reconstructed from a hash) to a value;
   then ingests the value-Ast into the Store and returns its hash.

   Values: Int_lit, Bool_lit, String_lit, Lam, Pair(value, value).
   Stuck on: Hole anywhere; If with non-Bool guard (impossible if
   typecheck rejected; harmless to be defensive); App with non-Lam
   function value; Fst/Snd with non-Pair argument value; primitive ops
   with wrong-typed arguments.

   Step budget bounds non-termination. Each β-step (App or Let) costs
   one step. Hitting the budget yields a StepLimit result; this result
   is NOT cached because a future call with a larger budget may produce
   a Value. */

let aspect_id: Attachment.aspect_id = "eval";
let procedure_id: Attachment.procedure_id = "eval:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["p9"],
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
  | _ => Stuck("") /* unreachable for this aspect */;

let aspect_value_of_result =
  fun
  | Value(h) => Attachment.Eval_value(h)
  | Stuck(h) => Attachment.Eval_stuck(h)
  | StepLimit(h) => Attachment.Eval_step_limit(h);

exception Dangling_hash(Hash.t);

let peek_cache = (att: Attachment.t, h: Hash.t): option(result) =>
  switch (
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(Attachment.Eval_value(h)) => Some(Value(h))
  | Some(Attachment.Eval_stuck(h)) => Some(Stuck(h))
  | Some(Attachment.Eval_step_limit(h)) => Some(StepLimit(h))
  | _ => None
  };

type budget = {mutable remaining: int};

let default_step_limit = 10000;

/* AST-level normal-form result. Mirrors `result` but carries the
   value-Ast directly so we can substitute, decompose, and ingest at the
   top of eval_hash. */
type ast_result =
  | NF(Ast.t)
  | StuckAst
  | StepLimAst;

let rec eval_ast = (~budget: budget, t: Ast.t): ast_result =>
  switch (t) {
  | Ast.Int_lit(_)
  | Ast.Bool_lit(_)
  | Ast.String_lit(_)
  | Ast.Lam(_, _) => NF(t)
  | Ast.Hole => StuckAst
  | Ast.Var(_) => StuckAst /* free variable at top level */
  | Ast.Pair(a, b) =>
    switch (eval_ast(~budget, a)) {
    | StuckAst => StuckAst
    | StepLimAst => StepLimAst
    | NF(va) =>
      switch (eval_ast(~budget, b)) {
      | StuckAst => StuckAst
      | StepLimAst => StepLimAst
      | NF(vb) => NF(Ast.Pair(va, vb))
      }
    }
  | Ast.Fst(p) =>
    switch (eval_ast(~budget, p)) {
    | StuckAst => StuckAst
    | StepLimAst => StepLimAst
    | NF(Ast.Pair(va, _)) => NF(va)
    | NF(_) => StuckAst
    }
  | Ast.Snd(p) =>
    switch (eval_ast(~budget, p)) {
    | StuckAst => StuckAst
    | StepLimAst => StepLimAst
    | NF(Ast.Pair(_, vb)) => NF(vb)
    | NF(_) => StuckAst
    }
  | Ast.If(c, th, el) =>
    switch (eval_ast(~budget, c)) {
    | StuckAst => StuckAst
    | StepLimAst => StepLimAst
    | NF(Ast.Bool_lit(true)) => eval_ast(~budget, th)
    | NF(Ast.Bool_lit(false)) => eval_ast(~budget, el)
    | NF(_) => StuckAst
    }
  | Ast.Let(rhs, body) =>
    switch (eval_ast(~budget, rhs)) {
    | StuckAst => StuckAst
    | StepLimAst => StepLimAst
    | NF(va) =>
      if (budget.remaining <= 0) {
        StepLimAst;
      } else {
        budget.remaining = budget.remaining - 1;
        let body' = Ast.beta(~body, ~arg=va);
        eval_ast(~budget, body');
      }
    }
  | Ast.App(f, a) =>
    switch (eval_ast(~budget, f)) {
    | StuckAst => StuckAst
    | StepLimAst => StepLimAst
    | NF(Ast.Lam(_, body)) =>
      switch (eval_ast(~budget, a)) {
      | StuckAst => StuckAst
      | StepLimAst => StepLimAst
      | NF(va) =>
        if (budget.remaining <= 0) {
          StepLimAst;
        } else {
          budget.remaining = budget.remaining - 1;
          let body' = Ast.beta(~body, ~arg=va);
          eval_ast(~budget, body');
        }
      }
    | NF(_) => StuckAst
    }
  | Ast.Prim(op, args) => eval_prim(~budget, op, args)
  }

and eval_prim =
    (~budget: budget, op: Surface_ast.prim_op, args: list(Ast.t)) => {
  let rec eval_all = (acc, lst) =>
    switch (lst) {
    | [] => `All(List.rev(acc))
    | [a, ...rest] =>
      switch (eval_ast(~budget, a)) {
      | StuckAst => `Stuck
      | StepLimAst => `StepLim
      | NF(v) => eval_all([v, ...acc], rest)
      }
    };
  switch (eval_all([], args)) {
  | `Stuck => StuckAst
  | `StepLim => StepLimAst
  | `All(vs) => apply_prim(op, vs)
  };
}

and apply_prim = (op: Surface_ast.prim_op, vs: list(Ast.t)): ast_result => {
  let int_op2 = (f: (int, int) => int) =>
    switch (vs) {
    | [Ast.Int_lit(a), Ast.Int_lit(b)] => NF(Ast.Int_lit(f(a, b)))
    | _ => StuckAst
    };
  let bool_op2 = (f: (bool, bool) => bool) =>
    switch (vs) {
    | [Ast.Bool_lit(a), Ast.Bool_lit(b)] => NF(Ast.Bool_lit(f(a, b)))
    | _ => StuckAst
    };
  switch (op) {
  | Surface_ast.Add => int_op2((+))
  | Surface_ast.Sub => int_op2((-))
  | Surface_ast.Mul => int_op2(( * ))
  | Surface_ast.Div =>
    switch (vs) {
    | [Ast.Int_lit(_), Ast.Int_lit(0)] => StuckAst
    | [Ast.Int_lit(a), Ast.Int_lit(b)] => NF(Ast.Int_lit(a / b))
    | _ => StuckAst
    }
  | Surface_ast.Mod =>
    switch (vs) {
    | [Ast.Int_lit(_), Ast.Int_lit(0)] => StuckAst
    | [Ast.Int_lit(a), Ast.Int_lit(b)] => NF(Ast.Int_lit(a mod b))
    | _ => StuckAst
    }
  | Surface_ast.And => bool_op2((&&))
  | Surface_ast.Or => bool_op2((||))
  | Surface_ast.Not =>
    switch (vs) {
    | [Ast.Bool_lit(b)] => NF(Ast.Bool_lit(!b))
    | _ => StuckAst
    }
  | Surface_ast.Concat =>
    switch (vs) {
    | [Ast.String_lit(a), Ast.String_lit(b)] => NF(Ast.String_lit(a ++ b))
    | _ => StuckAst
    }
  | Surface_ast.Eq =>
    switch (vs) {
    | [Ast.Int_lit(a), Ast.Int_lit(b)] => NF(Ast.Bool_lit(a == b))
    | [Ast.Bool_lit(a), Ast.Bool_lit(b)] => NF(Ast.Bool_lit(a == b))
    | [Ast.String_lit(a), Ast.String_lit(b)] => NF(Ast.Bool_lit(a == b))
    | _ => StuckAst
    }
  };
};

let eval =
    (
      ~store: Store.t,
      ~att: Attachment.t,
      ~step_limit=default_step_limit,
      h: Hash.t,
    )
    : result =>
  switch (peek_cache(att, h)) {
  | Some(r) => r
  | None =>
    switch (Store.reconstruct(store, h)) {
    | None => raise(Dangling_hash(h))
    | Some(ast) =>
      let budget = {remaining: step_limit};
      let result =
        switch (eval_ast(~budget, ast)) {
        | NF(v) => Value(Store.ingest(store, v))
        | StuckAst => Stuck(h)
        | StepLimAst => StepLimit(h)
        };
      /* Cache only Value/Stuck — not StepLimit. */
      switch (result) {
      | Value(_)
      | Stuck(_) =>
        Attachment.attach(
          att,
          ~target=h,
          ~aspect=aspect_id,
          ~procedure=procedure_id,
          aspect_value_of_result(result),
        )
      | StepLimit(_) => ()
      };
      result;
    }
  };

let print_result = (~pp: Hash.t => string, r: result): string =>
  switch (r) {
  | Value(h) => pp(h)
  | Stuck(h) => "⟂ stuck at: " ++ pp(h)
  | StepLimit(h) => "… step limit reached at: " ++ pp(h)
  };
