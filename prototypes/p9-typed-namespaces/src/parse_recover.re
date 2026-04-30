/* Error-recovering parser. Same incremental-API pattern as p8: every
   string input parses into a Surface_ast.t (or Surface_ty.t for the
   type editor) without raising; subterms that don't fit the grammar
   become `Hole` nodes.

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
   4. max_errors caps recoveries to bound pathological inputs.

   The driver is parameterized over the start checkpoint and a fallback
   value (`Surface_ast.Hole` for terms, `Surface_ty.Hole` for types) so
   the same logic services both entry points. */

module MI = Parser.MenhirInterpreter;

type triple = (Parser.token, Lexing.position, Lexing.position);

let tokenize = (input: string): list(triple) => {
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
  collect([]);
};

let drive_recover =
    (~start: MI.checkpoint('a), ~fallback: 'a, input: string): 'a => {
  let queue: ref(list(triple)) = ref(tokenize(input));

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

  let rec advance = (cp: MI.checkpoint('a)): MI.checkpoint('a) =>
    switch (cp) {
    | MI.Shifting(_, _, _)
    | MI.AboutToReduce(_, _) => advance(MI.resume(cp))
    | _ => cp
    };

  let max_errors = 1024;
  let errors = ref(0);

  let rec drive =
          (
            last_inp: option((MI.checkpoint('a), triple)),
            cp: MI.checkpoint('a),
          )
          : 'a => {
    let cp = advance(cp);
    switch (cp) {
    | MI.Accepted(v) => v
    | MI.Rejected => fallback
    | MI.InputNeeded(_) =>
      let triple = take();
      drive(Some((cp, triple)), MI.offer(cp, triple));
    | MI.HandlingError(_) =>
      incr(errors);
      if (errors^ > max_errors) {
        fallback;
      } else {
        switch (last_inp) {
        | None => fallback
        | Some((inp_cp, bad_triple)) =>
          let with_hole = advance(MI.offer(inp_cp, hole_triple));
          switch (with_hole) {
          | MI.HandlingError(_) =>
            let (bad_tok, _, _) = bad_triple;
            if (bad_tok == Parser.EOF) {
              fallback;
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
    | MI.AboutToReduce(_, _) => fallback
    };
  };

  drive(None, start);
};

let parse = (input: string): Surface_ast.t =>
  drive_recover(
    ~start=Parser.Incremental.main(Lexing.dummy_pos),
    ~fallback=Surface_ast.Hole,
    input,
  );

let parse_ty = (input: string): Surface_ty.t =>
  drive_recover(
    ~start=Parser.Incremental.main_ty(Lexing.dummy_pos),
    ~fallback=Surface_ty.Hole,
    input,
  );
