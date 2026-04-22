open P1_arithmetic;

let banner = "p1-arithmetic REPL — TAPL Ch. 3 untyped arithmetic (Ctrl-D or :quit to exit)";

let help_text = "Commands:
  <expr>              parse, register, and evaluate
  :register-only <e>  parse and register without evaluating
  :eval-expr <e>      evaluate without storing
  :list               enumerate stored definitions
  :lookup <prefix>    print the AST for a stored hash
  :eval <prefix>      re-run evaluation on a stored hash
  :help               show this message
  :quit | :exit       leave the REPL";

/* Parsing helpers. */
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

/* Split a raw line into (command_or_empty, rest). Commands start with ':'. */
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

let register_and_eval = (store, src) =>
  switch (parse(src)) {
  | ParseError(msg) => print_endline("!! " ++ msg)
  | Parsed(ast) =>
    let h = Store.register(store, ast);
    print_endline(Hash.short(h));
    print_endline("⇒ " ++ Eval.print_result(Eval.eval(ast)));
  };

let register_only = (store, src) =>
  switch (parse(src)) {
  | ParseError(msg) => print_endline("!! " ++ msg)
  | Parsed(ast) =>
    let h = Store.register(store, ast);
    print_endline(Hash.short(h));
  };

let eval_expr = src =>
  switch (parse(src)) {
  | ParseError(msg) => print_endline("!! " ++ msg)
  | Parsed(ast) => print_endline("⇒ " ++ Eval.print_result(Eval.eval(ast)))
  };

let list_store = store => {
  let entries = Store.entries(store);
  if (entries == []) {
    print_endline("(store empty)");
  } else {
    List.iter(
      ((h, d)) =>
        print_endline(Hash.short(h) ++ " — " ++ Pretty.print(d)),
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
  with_resolved_prefix(store, prefix, h =>
    switch (Store.lookup(store, h)) {
    | None => print_endline("!! internal: resolved hash not in store")
    | Some(d) => print_endline(Pretty.print(d))
    }
  );

let eval_cmd = (store, prefix) =>
  with_resolved_prefix(store, prefix, h =>
    switch (Store.lookup(store, h)) {
    | None => print_endline("!! internal: resolved hash not in store")
    | Some(d) => print_endline("⇒ " ++ Eval.print_result(Eval.eval(d)))
    }
  );

let run_line = (store: Store.t, line: string): bool =>
  switch (split_command(line)) {
  | None =>
    let trimmed = String.trim(line);
    if (trimmed != "") {
      register_and_eval(store, trimmed);
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
      eval_expr(rest);
    };
    true;
  | Some((":lookup", prefix)) =>
    if (prefix == "") {
      print_endline("!! :lookup needs a hash prefix");
    } else {
      lookup_cmd(store, prefix);
    };
    true;
  | Some((":eval", prefix)) =>
    if (prefix == "") {
      print_endline("!! :eval needs a hash prefix");
    } else {
      eval_cmd(store, prefix);
    };
    true;
  | Some((cmd, _)) =>
    print_endline("!! unknown command: " ++ cmd ++ "  (try :help)");
    true;
  };

let rec loop = store => {
  print_string("> ");
  switch (read_line()) {
  | exception End_of_file =>
    print_newline();
    ();
  | line =>
    if (run_line(store, line)) {
      loop(store);
    }
  };
};

let () = {
  print_endline(banner);
  let store = Store.create();
  loop(store);
};
