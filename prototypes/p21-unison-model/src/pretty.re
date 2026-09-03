/* Rendering a stored definition back to source. (def:ppe, prop:print-parse)
 *
 * This is half of the operation update rests on. Step 2 of def:update renders
 * every dependent to text; step 3 parses that text back. The intended semantics
 * holds only if the round trip is exact except at the references being
 * retargeted (prop:print-parse) -- and Unison does not state that property
 * anywhere, maintaining it instead with a golden-file corpus named, precisely,
 * `reparses-with-same-hash.u`.
 *
 * p21's language is small enough to test the property directly, which is what
 * the round-trip qcheck does.
 *
 * Note that what is rendered uses the AUTHOR'S variable names, because binder
 * names survived storage (rem:names). A system that stored de Bruijn indices
 * and invented display names -- every arbor prototype from p4 on -- could state
 * its round trip only up to a choice of names, and would hand the user back
 * their own code with someone else's variables in it. */

let ty = (t: Ty.t): string => Ty.to_string(t);

/* Precedence levels, matching the grammar's nonterminals exactly:
 *
 *   0  expr      \x. e          binds loosest, extends as far right as it can
 *   1  ann_expr  e : T
 *   2  app_expr  f a
 *   3  atom      x, 1, "s", (e)
 *
 * `prec` is what the CONTEXT demands; a node wraps itself when its own level is
 * looser than that. Getting this wrong is the classic way to break
 * prop:print-parse, and it is how this printer was in fact first written: an
 * application in operand position went unparenthesized, so `f (g x y)` printed
 * as `f g x y` and parsed back as something else. The round-trip property
 * caught it on the first run, which is the argument for having the property
 * rather than a corpus. */
let rec term = (~prec=0, ppe: Ppe.t, tm: Term.t): string => {
  let wrap = (level, s) => level < prec ? "(" ++ s ++ ")" : s;
  switch (tm) {
  | Term.Var(x) => x
  | Term.Nat(n) => string_of_int(n)
  | Term.Text(s) => "\"" ++ String.escaped(s) ++ "\""
  | Term.Ref(r) => Ppe.render_ref(ppe, r)
  | Term.Lam(x, b) => wrap(0, "\\" ++ x ++ ". " ++ term(~prec=0, ppe, b))
  | Term.Ann(e, t) => wrap(1, term(~prec=2, ppe, e) ++ " : " ++ ty(t))
  | Term.App(f, a) =>
    wrap(2, term(~prec=2, ppe, f) ++ " " ++ term(~prec=3, ppe, a))
  };
};

/* Render a whole definition as the two stanzas a scratch buffer holds: a
   signature and a binding. The signature is not optional -- the language has no
   inference, so a definition that came back without one would not re-typecheck,
   and prop:print-parse would fail for a reason unrelated to references. */
let definition = (ppe: Ppe.t, name: string, tm: Term.t, t: Ty.t): string => {
  /* The stored form is wrapped in its type (def:typeinhash); unwrap it, since
     the signature line carries the type. */
  let body =
    switch (tm) {
    | Term.Ann(inner, _) => inner
    | other => other
    };
  name ++ " : " ++ ty(t) ++ "\n" ++ name ++ " = " ++ term(~prec=0, ppe, body);
};
