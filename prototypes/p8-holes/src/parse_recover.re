/* Error-recovering parser. The central guarantee: for every string
   input, `parse input` returns a Surface_ast.t without raising.
   Errant subterms become `Hole` nodes. This is the p8 prototype's
   headline property; every other module downstream assumes parse
   results are total.

   Implementation: menhir's incremental API (enabled by --table in
   src/dune). The algorithm is:

   1. Pre-tokenize the whole input. The lexer is total — unknown
      bytes emit HOLE tokens, so tokenization never raises.

   2. Drive the incremental parser by feeding tokens from a queue.
      On every `InputNeeded` checkpoint, remember the checkpoint and
      the next token offered ("last_inp"); this is our rewind anchor.

   3. On `HandlingError`, rewind to the last_inp state and offer a
      synthetic HOLE token in place of the offending one. Because the
      grammar admits HOLE as an atom, a single injection often
      unblocks the parser at any position where an atom/app_expr/expr
      was expected.
      - If HOLE fits: the offending token is DROPPED; continue from
        the post-HOLE checkpoint. We do not re-queue the offender:
        HOLE is an atom-starter, so re-queuing a non-atom-starter
        offender (say, RPAREN in the middle of an app_expr) would
        keep triggering HandlingError after each HOLE insertion and
        produce unbounded runs of spurious holes.
      - If HOLE also doesn't fit: drop the offender and retry from
        the same last_inp state with the next queued token.

   4. `max_errors` caps total recoveries, protecting against
      pathological inputs that can't be reconciled at all (e.g.,
      `\` with no identifier — the grammar demands IDENT and HOLE
      isn't IDENT). When the cap trips we return a bare `Hole`,
      still honoring totality. */

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

  /* Absorb Shifting / AboutToReduce states until a caller-visible
     checkpoint. Never resumes HandlingError — that's the caller's
     responsibility. */
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
        | None =>
          /* Error before any input was offered. The only way this
             happens is if the parser starts in a state that rejects
             without a lookahead — not possible for our grammar, but
             honor it. */
          Surface_ast.Hole
        | Some((inp_cp, bad_triple)) =>
          let with_hole = advance(MI.offer(inp_cp, hole_triple));
          switch (with_hole) {
          | MI.HandlingError(_) =>
            /* HOLE doesn't fit either. If the offender is EOF we've
               exhausted input without the parser accepting — there's
               no way forward. Bail out with a bare Hole. Otherwise,
               drop the offender and retry with the next queued token. */
            let (bad_tok, _, _) = bad_triple;
            if (bad_tok == Parser.EOF) {
              Surface_ast.Hole;
            } else {
              let next = take();
              drive(Some((inp_cp, next)), MI.offer(inp_cp, next));
            };
          | _ =>
            /* HOLE was absorbed; the offender is dropped. Continue
               from the post-HOLE checkpoint. */
            drive(last_inp, with_hole);
          };
        };
      };
    | MI.Shifting(_, _, _)
    | MI.AboutToReduce(_, _) =>
      /* `advance` absorbs these — unreachable. */
      Surface_ast.Hole
    };
  };

  drive(None, Parser.Incremental.main(dummy));
};
