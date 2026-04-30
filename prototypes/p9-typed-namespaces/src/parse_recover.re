/* Error-recovering parser. Same incremental-API pattern as p8: every
   string input parses into a Surface_ast.t without raising; subterms
   that don't fit the grammar become `Hole` nodes.

   Strategy:
   1. Pre-tokenize the whole input. Lexer is total — unknown bytes emit
      HOLE tokens, never raises.
   2. Drive Parser.MenhirInterpreter token-by-token. On every InputNeeded
      checkpoint, remember the checkpoint and offered token (rewind anchor).
   3. On HandlingError, rewind and offer a synthetic HOLE in place of the
      offender. Because HOLE is admitted as both an atom and a binder,
      this unblocks most positions.
      - HOLE fits: drop the offender, continue.
      - HOLE doesn't fit either: drop the offender, try the next token.
   4. max_errors caps recoveries to bound pathological inputs. */

module MI = Parser.MenhirInterpreter;

type triple = (Parser.token, Lexing.position, Lexing.position);

let parse = (input: string): Surface_ast.t => {
  let lexbuf = Lexing.from_string(input);

  let rec collect = (acc: list(triple)): list(triple) => {
    let tok = Lexer.token(lexbuf);
    let start_p = lexbuf.Lexing.lex_start_p;
    let end_p = lexbuf.Lexing.lex_curr_p;
    let triple = (tok, start_p, end_p);
    switch (tok) {
    | Parser.EOF => List.rev([triple, ...acc])
    | _ => collect([triple, ...acc])
    };
  };
  let queue: ref(list(triple)) = ref(collect([]));

  let dummy = Lexing.dummy_pos;
  let hole_triple: triple = (Parser.HOLE, dummy, dummy);
  let eof_triple: triple = (Parser.EOF, dummy, dummy);

  let take = (): triple =>
    switch (queue^) {
    | [] => eof_triple
    | [t, ...rest] =>
      queue := rest;
      t;
    };

  let rec advance = (cp: MI.checkpoint(Surface_ast.t)): MI.checkpoint(Surface_ast.t) =>
    switch (cp) {
    | MI.Shifting(_, _, _)
    | MI.AboutToReduce(_, _) => advance(MI.resume(cp))
    | _ => cp
    };

  let max_errors = 1024;
  let errors = ref(0);

  let rec drive =
          (
            last_inp: option((MI.checkpoint(Surface_ast.t), triple)),
            cp: MI.checkpoint(Surface_ast.t),
          )
          : Surface_ast.t => {
    let cp = advance(cp);
    switch (cp) {
    | MI.Accepted(v) => v
    | MI.Rejected => Surface_ast.Hole
    | MI.InputNeeded(_) =>
      let triple = take();
      drive(Some((cp, triple)), MI.offer(cp, triple));
    | MI.HandlingError(_) =>
      incr(errors);
      if (errors^ > max_errors) {
        Surface_ast.Hole;
      } else {
        switch (last_inp) {
        | None => Surface_ast.Hole
        | Some((inp_cp, bad_triple)) =>
          let with_hole = advance(MI.offer(inp_cp, hole_triple));
          switch (with_hole) {
          | MI.HandlingError(_) =>
            let (bad_tok, _, _) = bad_triple;
            if (bad_tok == Parser.EOF) {
              Surface_ast.Hole;
            } else {
              let next = take();
              drive(Some((inp_cp, next)), MI.offer(inp_cp, next));
            };
          | _ =>
            drive(last_inp, with_hole);
          };
        };
      };
    | MI.Shifting(_, _, _)
    | MI.AboutToReduce(_, _) =>
      Surface_ast.Hole
    };
  };

  drive(None, Parser.Incremental.main(dummy));
};
