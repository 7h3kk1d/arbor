open P2_structural_sharing;

let banner = "p2-structural-sharing REPL — arithmetic with content-addressed DAG storage + eval-cache aspect (Ctrl-D or :quit to exit)";

let help_text = "Commands:
  <expr>              parse, ingest (hash-cons), and evaluate
  :register-only <e>  parse and ingest without evaluating
  :eval-expr <e>      evaluate without keeping a fresh top-level hash
  :list               enumerate stored DAG nodes (reconstructed view)
  :dag                enumerate stored DAG nodes (as-stored, children shown as hashes)
  :lookup <prefix>    print the reconstructed term for a stored hash
  :show <prefix>      print one node as stored (children shown as hashes)
  :eval <prefix>      re-run evaluation on a stored hash
  :stats              definition count + eval hits/misses/entries
  :help               show this message
  :quit | :exit       leave the REPL";

type parse_result =
  | Parsed(Ast.t)
  | ParseError(string);

let parse = (input: string): parse_result => {
  let lexbuf = Lexing.from_string(input);
  try(Parsed(Parser.main(Lexer.token, lexbuf))) {
  | Parser.Error => ParseError("parse error")
  | Lexer.Lex_error(msg) => ParseError(msg)
  };
};

let split_command = (line: string): option((string, string)) => {
  let line = String.trim(line);
  if (String.length(line) == 0 || line.[0] != ':') {
    None;
  } else {
    switch (String.index_opt(line, ' ')) {
    | None => Some((line, ""))
    | Some(i) =>
      let cmd = String.sub(line, 0, i);
      let rest = String.trim(String.sub(line, i + 1, String.length(line) - i - 1));
      Some((cmd, rest));
    };
  };
};

let eval_with_cache_marker = (store, att, h): string => {
  let was_cached = Option.is_some(Eval.peek_cache(att, h));
  let r = Eval.eval(~store, ~att, h);
  let base = Eval.print_result(store, r);
  if (was_cached) {
    base ++ " (cached)";
  } else {
    base;
  };
};

let register_and_eval = (store, att, src) =>
  switch (parse(src)) {
  | ParseError(msg) => print_endline("!! " ++ msg)
  | Parsed(ast) =>
    let h = Store.ingest(store, Canonicalize.canonicalize(ast));
    print_endline(Hash.short(h));
    print_endline("⇒ " ++ eval_with_cache_marker(store, att, h));
  };

let register_only = (store, src) =>
  switch (parse(src)) {
  | ParseError(msg) => print_endline("!! " ++ msg)
  | Parsed(ast) =>
    let h = Store.ingest(store, Canonicalize.canonicalize(ast));
    print_endline(Hash.short(h));
  };

let eval_expr = (store, att, src) =>
  switch (parse(src)) {
  | ParseError(msg) => print_endline("!! " ++ msg)
  | Parsed(ast) =>
    let h = Store.ingest(store, Canonicalize.canonicalize(ast));
    print_endline("⇒ " ++ eval_with_cache_marker(store, att, h));
  };

let list_store = store => {
  let entries = Store.entries(store);
  if (entries == []) {
    print_endline("(store empty)");
  } else {
    List.iter(
      ((h, _)) => print_endline(Hash.short(h) ++ " — " ++ Pretty.print(store, h)),
      entries,
    );
  };
};

let dag_store = store => {
  let entries = Store.entries(store);
  if (entries == []) {
    print_endline("(store empty)");
  } else {
    List.iter(
      ((h, n)) => print_endline(Hash.short(h) ++ " — " ++ Pretty.print_shallow(n)),
      entries,
    );
  };
};

let with_resolved_prefix = (store, prefix, k) =>
  switch (Store.resolve_prefix(store, prefix)) {
  | NotFound => print_endline("!! no match for prefix: " ++ prefix)
  | Ambiguous(hs) =>
    print_endline("!! ambiguous prefix (" ++ string_of_int(List.length(hs)) ++ " matches):");
    List.iter(h => print_endline("   " ++ Hash.short(h)), hs);
  | Found(h) => k(h)
  };

let lookup_cmd = (store, prefix) =>
  with_resolved_prefix(store, prefix, h => print_endline(Pretty.print(store, h)));

let show_cmd = (store, prefix) =>
  with_resolved_prefix(store, prefix, h =>
    switch (Store.lookup(store, h)) {
    | None => print_endline("!! internal: resolved hash not in store")
    | Some(node) => print_endline(Pretty.print_shallow(node))
    }
  );

let eval_cmd = (store, att, prefix) =>
  with_resolved_prefix(store, prefix, h =>
    print_endline("⇒ " ++ eval_with_cache_marker(store, att, h))
  );

let stats_cmd = (store, att) => {
  let s = Attachment.stats(att);
  print_endline(
    Printf.sprintf(
      "definitions: %d\neval entries: %d\nhits: %d, misses: %d",
      Store.size(store),
      s.entries,
      s.hits,
      s.misses,
    ),
  );
};

let run_line = (store: Store.t, att: Attachment.t, line: string): bool =>
  switch (split_command(line)) {
  | None =>
    let trimmed = String.trim(line);
    if (trimmed != "") {
      register_and_eval(store, att, trimmed);
    };
    true;
  | Some((":quit", _))
  | Some((":exit", _)) => false
  | Some((":help", _)) =>
    print_endline(help_text);
    true;
  | Some((":list", _)) =>
    list_store(store);
    true;
  | Some((":dag", _)) =>
    dag_store(store);
    true;
  | Some((":stats", _)) =>
    stats_cmd(store, att);
    true;
  | Some((":register-only", rest)) =>
    if (rest == "") {
      print_endline("!! :register-only needs an expression");
    } else {
      register_only(store, rest);
    };
    true;
  | Some((":eval-expr", rest)) =>
    if (rest == "") {
      print_endline("!! :eval-expr needs an expression");
    } else {
      eval_expr(store, att, rest);
    };
    true;
  | Some((":lookup", prefix)) =>
    if (prefix == "") {
      print_endline("!! :lookup needs a hash prefix");
    } else {
      lookup_cmd(store, prefix);
    };
    true;
  | Some((":show", prefix)) =>
    if (prefix == "") {
      print_endline("!! :show needs a hash prefix");
    } else {
      show_cmd(store, prefix);
    };
    true;
  | Some((":eval", prefix)) =>
    if (prefix == "") {
      print_endline("!! :eval needs a hash prefix");
    } else {
      eval_cmd(store, att, prefix);
    };
    true;
  | Some((cmd, _)) =>
    print_endline("!! unknown command: " ++ cmd ++ "  (try :help)");
    true;
  };

let rec loop = (store, att) => {
  print_string("> ");
  switch (read_line()) {
  | exception End_of_file =>
    print_newline();
    ();
  | line =>
    if (run_line(store, att, line)) {
      loop(store, att);
    }
  };
};

let () = {
  print_endline(banner);
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Eval.descriptor);
  loop(store, att);
};
