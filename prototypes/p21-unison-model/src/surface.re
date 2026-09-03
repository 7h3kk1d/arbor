/* What the parser produces: a whole FILE, not a term. (def:ufile)
 *
 * A scratch buffer holds several bindings and several watch expressions, and
 * the unit that gets ingested is the buffer's live region as one binding group
 * (def:bindgroup). So the parser's top-level nonterminal is a file. */

/* A stanza above the fold. */
type stanza =
  | Signature(string, Ty.t) /* name : T */
  | Binding(string, Term.Source.t) /* name = e */
  | Watch(Term.Source.t); /* > e */

type file = {
  stanzas: list(stanza),
  /* Everything below a `---` fold line. Inert: parsed as nothing, kept as
     text. This is Unison's working-set boundary, and it really is just a
     string in a text file. */
  below_fold: string,
};

let empty: file = {stanzas: [], below_fold: ""};
