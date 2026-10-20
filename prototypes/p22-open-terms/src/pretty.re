/* Rendering a stored definition back to source -- for DISPLAY only.
 *
 * In p21 this module is load-bearing for correctness, because update renders
 * every dependent to text and parses it back, so the round trip has to be exact
 * (prop:print-parse) and Unison maintains that with a golden-file corpus named
 * `reparses-with-same-hash.u`.
 *
 * Here nothing propagates through text. Propagation rewrites a link's target
 * array (link.re), so printing is never on the path between a change and its
 * consequences. That demotes this file from a correctness obligation to a
 * convenience, and it is the single clearest measure of what the open/link
 * split buys: the property p21 must maintain, p22 does not have to state.
 *
 * Binders print with invented names, since none are stored -- p22 is de Bruijn
 * where p21 is a named ABT (rem:names), and it can afford to be precisely
 * because it never hands rendered text back to the machinery. Slot names come
 * from recorded intent, not from resolving anything. */

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
let rec term = (~prec=0, tm: Term.Source.t): string => {
  let wrap = (level, s) => level < prec ? "(" ++ s ++ ")" : s;
  switch (tm) {
  | Term.Source.Var(x) => x
  | Term.Source.Nat(n) => string_of_int(n)
  | Term.Source.Text(s) => "\"" ++ String.escaped(s) ++ "\""
  | Term.Source.Lam(x, b) => wrap(0, "\\" ++ x ++ ". " ++ term(~prec=0, b))
  | Term.Source.Ann(e, t) => wrap(1, term(~prec=2, e) ++ " : " ++ ty(t))
  | Term.Source.App(f, a) =>
    wrap(2, term(~prec=2, f) ++ " " ++ term(~prec=3, a))
  };
};

/* An open term, rendered with whatever names the caller knows for its slots.
   There is no pretty-print environment and no name resolution here: slot names
   are recorded intent (Store.intent_of), never something reconstructed. That
   absence is the point -- it is the pretty-print environment that makes
   Unison's propagation name-sensitive, and there is nothing here for it to
   steer. */
let open_term =
    (~slot_names: array(string)=[||], ~prec=0, o: Open_term.t): string =>
  term(~prec, Open_term.to_source(~slot_names, o));

/* Render a definition as the two stanzas a buffer holds. */
let definition =
    (~slot_names: array(string)=[||], name: string, o: Open_term.t, t: Ty.t)
    : string => {
  let body =
    switch (Open_term.to_source(~slot_names, o)) {
    | Term.Source.Ann(inner, _) => inner
    | other => other
    };
  name ++ " : " ++ ty(t) ++ "\n" ++ name ++ " = " ++ term(~prec=0, body);
};
