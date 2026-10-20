/* The object language as WRITTEN. (p21's def:uterm, unchanged)
 *
 *   e ::= x | \x. e | e e | n | "s" | (e : T)
 *
 * p21 had two grammars here: source terms with names, and "resolved" terms
 * extended with references, because resolution replaced a name with a hash
 * before anything was stored. p22 has one, because resolution no longer
 * produces a term.
 *
 * What replaces the resolved grammar is the pair (Open_term.t, Link.t): the
 * open term is the source term with its free variables abstracted to slots, and
 * the link says what those slots point at. Nothing in between is a term, so
 * there is nothing in between to name. That is the p22 delta in one sentence,
 * and the reason this file is a third of p21's. */

module Source = {
  type t =
    | Var(string)
    | Lam(string, t)
    | App(t, t)
    | Nat(int)
    | Text(string)
    | Ann(t, Ty.t);
};

/* Free variables, in first-occurrence order -- the same order Open_term.abstract
   assigns slots in, so the two agree by construction. Kept here because the
   parser and the editor both want it before anything is abstracted. */
let free_vars = (tm: Source.t): list(string) => {
  let seen = Hashtbl.create(16);
  let acc = ref([]);
  let rec go = (bound, tm: Source.t) =>
    switch (tm) {
    | Source.Var(x) =>
      if (!List.mem(x, bound) && !Hashtbl.mem(seen, x)) {
        Hashtbl.add(seen, x, ());
        acc := [x, ...acc^];
      }
    | Source.Lam(x, b) => go([x, ...bound], b)
    | Source.App(f, a) =>
      go(bound, f);
      go(bound, a);
    | Source.Ann(e, _) => go(bound, e)
    | Source.Nat(_)
    | Source.Text(_) => ()
    };
  go([], tm);
  List.rev(acc^);
};
