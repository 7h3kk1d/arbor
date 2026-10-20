/* The front end: buffer text -> a file of stanzas.
 *
 * Fail-fast, no recovery, no holes. That is faithful -- Unison's parser is
 * monolithic and a file either parses or does not -- and it is the opposite of
 * p8/p9, whose error-recovering parser turns every keystroke into a valid AST.
 * Under Unison's model a definition has no identity until the file typechecks,
 * so there is nothing for a recovered AST to be the identity of.
 *
 * Stanzas are split by LAYOUT before they reach Menhir: a top-level stanza
 * begins at column 0 and its continuation lines are indented. That keeps the
 * grammar newline-insensitive while still letting a buffer hold many
 * definitions -- and it is Unison's own rule. */

type located_error = {
  line: int, /* 1-based, in the whole buffer */
  message: string,
};

/* Everything below a `---` line is inert scrollback. Returns (live, below). */
let split_fold = (text: string): (string, string) => {
  let lines = String.split_on_char('\n', text);
  let rec go = (acc, n, rest) =>
    switch (rest) {
    | [] => (text, "")
    | [l, ...tl] =>
      let trimmed = String.trim(l);
      if (String.length(trimmed) >= 3 && String.sub(trimmed, 0, 3) == "---") {
        (String.concat("\n", List.rev(acc)), String.concat("\n", tl));
      } else {
        go([l, ...acc], n + 1, tl);
      };
    };
  go([], 0, lines);
};

let is_blank = (l: string): bool => String.trim(l) == "";

let is_comment = (l: string): bool => {
  let t = String.trim(l);
  String.length(t) >= 2 && String.sub(t, 0, 2) == "--";
};

let starts_stanza = (l: string): bool =>
  !is_blank(l)
  && !is_comment(l)
  && String.length(l) > 0
  && l.[0] != ' '
  && l.[0] != '\t';

/* Chunk the live region into (first-line-number, text) pairs. */
let chunks = (live: string): list((int, string)) => {
  let lines = Array.of_list(String.split_on_char('\n', live));
  let out = ref([]);
  let cur = ref([]);
  let cur_line = ref(0);
  let flush = () =>
    if (cur^ != []) {
      let body = String.concat("\n", List.rev(cur^));
      if (String.trim(body) != "") {
        out := [(cur_line^, body), ...out^];
      };
      cur := [];
    };
  Array.iteri(
    (i, l) =>
      if (starts_stanza(l)) {
        flush();
        cur_line := i + 1;
        cur := [l];
      } else if (cur^ != []) {
        cur := [l, ...cur^];
      } else if (!is_blank(l) && !is_comment(l)) {
        /* an indented line with no stanza open */
        cur_line := i + 1;
        cur := [l];
      },
    lines,
  );
  flush();
  List.rev(out^);
};

let run = (entry, s: string) => {
  let lexbuf = Lexing.from_string(s);
  try(Ok(entry(Lexer.token, lexbuf))) {
  | Lexer.Error(m) => Error(m)
  | Parser.Error => Error("syntax error")
  };
};

let expr = (s: string): result(Term.Source.t, string) => run(Parser.expr_only, s);

let stanza = (s: string): result(Surface.stanza, string) =>
  run(Parser.stanza_only, s);

/* Parse a whole buffer. Fail-fast per the model, but errors are collected per
   stanza so the buffer can show which line is at fault -- reporting all of them
   is an interface choice, not a change to the model, since nothing is ingested
   either way. */
let file = (text: string): (Surface.file, list(located_error)) => {
  let (live, below) = split_fold(text);
  let errs = ref([]);
  let stanzas =
    List.filter_map(
      ((line, body)) =>
        switch (stanza(body)) {
        | Ok(s) => Some(s)
        | Error(m) =>
          errs := [{line, message: m}, ...errs^];
          None;
        },
      chunks(live),
    );
  ({Surface.stanzas, below_fold: below}, List.rev(errs^));
};
