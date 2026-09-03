/* The pretty-print environment. (def:ppe)
 *
 * A ppe assigns each reference a name to render it by. Unison's carries two
 * different suffixification strategies -- definitions in the FILE are
 * suffixified by name, definitions in the CODEBASE by hash -- because aliases
 * sharing a suffix may be referred to by that suffix in the codebase but not in
 * a file, where the parser reports an ambiguity. The source's own example: with
 * `one.foo` and `two.foo` both in the file, `hey = foo + foo` fails to parse.
 *
 * Names here are atomic, so the asymmetry has nothing to bite on and the two
 * strategies collapse into one. The STRUCTURE is kept -- two sources, file
 * first -- because prop:print-parse's difficulty demonstrably lives here, and a
 * ppe with only one source could not exhibit it at all. */

type t = {
  /* Names the incoming file binds. These win. */
  file: list((string, Reference.t)),
  /* The codebase namespace. */
  ns: Names.t,
};

let make = (~file=[], ns: Names.t): t => {file, ns};

/* The name to render r by. A reference with no name has to be rendered as a
   hash, and a hash does not parse back to anything -- which is why def:uscope
   restricts the update scope to references that HAVE a name (rem:scopenaming).
   An unnamed definition is not a dependent to be updated. */
let name_of = (ppe: t, r: Reference.t): option(string) => {
  let from_file =
    List.fold_left(
      (acc, (n, r')) =>
        acc == None && Reference.equal(r, r') ? Some(n) : acc,
      None,
      ppe.file,
    );
  switch (from_file) {
  | Some(n) => Some(n)
  | None =>
    switch (Names.names_of(ppe.ns, Reference.Ref(r))) {
    | [n, ..._] => Some(n)
    | [] => None
    }
  };
};

let render_ref = (ppe: t, r: Reference.t): string =>
  switch (r) {
  | Reference.Builtin(b) => b
  | Reference.Derived(_, _) =>
    switch (name_of(ppe, r)) {
    | Some(n) => n
    | None => Reference.to_string(r)
    }
  };
