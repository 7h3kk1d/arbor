open P3_naming_layer;

let banner = "p3-naming-layer REPL — arithmetic with content-addressed DAG storage + eval-cache aspect + namespace (Ctrl-D or :quit to exit)";

let help_text = "Commands:
  <expr>                 parse, resolve names, ingest (hash-cons), evaluate
  :register-only <e>     parse, resolve, ingest without evaluating
  :eval-expr <e>         evaluate without keeping a fresh top-level hash
  :bind <name> <e|#h>    bind <name> to the hash of an expression or to an
                         already-stored hash prefix (e.g. '#1ec244c78f42')
  :rebind <name> <e|#h>  like :bind, but overwrites an existing binding
  :bind-hash <n> <pfx>   (alias) bind a name to an already-stored hash prefix
  :unbind <name>         remove a binding
  :rename <old> <new>    rename a binding (unbind + bind to the same hash)
  :names                 list all (name, hash) bindings, sorted by name
  :name-of <name|#pfx>   list names pointing at a stored hash
  :hash-of <name>        print the hash bound to <name>
  :list                  enumerate stored nodes; rows show 'hash [names] — body'
                         where body substitutes names for named child subterms
  :list raw              :list without name substitution or name brackets
  :dag                   enumerate stored nodes, children shown as hashes
  :lookup <name|#pfx>    print the reconstructed term (accepts a name or hash)
  :show <name|#pfx>      print one node as stored (accepts a name or hash)
  :eval <name|#pfx>      re-run evaluation (accepts a name or hash)
  :stats                 counts for definitions, eval cache, names
  :load <path>           execute REPL commands from a file; '#' starts comments
  :help                  show this message
  :quit | :exit          leave the REPL

Hashes display with a leading '#'. Inputs accept '#<hex>' or legacy 'h:<hex>'.";

type parse_result =
  | Parsed(Surface_ast.t)
  | ParseError(string);

let parse = (input: string): parse_result => {
  let lexbuf = Lexing.from_string(input);
  try(Parsed(Parser.main(Lexer.token, lexbuf))) {
  | Parser.Error => ParseError("parse error")
  | Lexer.Lex_error(msg) => ParseError(msg)
  };
};

let parse_and_resolve =
    (~namespace: Namespace.t, ~store: Store.t, input: string)
    : result(Ast.t, string) =>
  switch (parse(input)) {
  | ParseError(msg) => Error(msg)
  | Parsed(s) =>
    switch (Resolver.resolve(~namespace, ~store, s)) {
    | Ok(ast) => Ok(ast)
    | Error(e) => Error(Resolver.error_to_string(e))
    }
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

let split_once = (s: string): option((string, string)) => {
  let s = String.trim(s);
  switch (String.index_opt(s, ' ')) {
  | None => None
  | Some(i) =>
    let first = String.sub(s, 0, i);
    let rest = String.trim(String.sub(s, i + 1, String.length(s) - i - 1));
    Some((first, rest));
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

let register_and_eval = (store, att, namespace, src) =>
  switch (parse_and_resolve(~namespace, ~store, src)) {
  | Error(msg) => print_endline("!! " ++ msg)
  | Ok(ast) =>
    let h = Store.ingest(store, Canonicalize.canonicalize(ast));
    print_endline(Hash.short(h));
    print_endline("⇒ " ++ eval_with_cache_marker(store, att, h));
  };

let register_only = (store, namespace, src) =>
  switch (parse_and_resolve(~namespace, ~store, src)) {
  | Error(msg) => print_endline("!! " ++ msg)
  | Ok(ast) =>
    let h = Store.ingest(store, Canonicalize.canonicalize(ast));
    print_endline(Hash.short(h));
  };

let eval_expr = (store, att, namespace, src) =>
  switch (parse_and_resolve(~namespace, ~store, src)) {
  | Error(msg) => print_endline("!! " ++ msg)
  | Ok(ast) =>
    let h = Store.ingest(store, Canonicalize.canonicalize(ast));
    print_endline("⇒ " ++ eval_with_cache_marker(store, att, h));
  };

let format_name_bracket = (names: list(string)): string =>
  switch (names) {
  | [] => ""
  | ns => " [" ++ String.concat(", ", ns) ++ "]"
  };

/* Compute the maximum printed-width of the "short-hash + bracketed names"
   prefix across all rows, so `:list` rows align. */
let row_prefix = (hash: Hash.t, names: list(string)): string =>
  Hash.short(hash) ++ format_name_bracket(names);

let string_width = String.length;

let pad_right = (s: string, n: int): string => {
  let w = string_width(s);
  if (w >= n) {
    s;
  } else {
    s ++ String.make(n - w, ' ');
  };
};

let list_store = (store, namespace) => {
  let entries = Store.entries(store);
  if (entries == []) {
    print_endline("(store empty)");
  } else {
    let rows =
      List.map(
        ((h, _)) => (h, Namespace.names_of(namespace, h)),
        entries,
      );
    let max_prefix =
      List.fold_left(
        (acc, (h, names)) => max(acc, string_width(row_prefix(h, names))),
        0,
        rows,
      );
    List.iter(
      ((h, names)) => {
        let prefix = pad_right(row_prefix(h, names), max_prefix);
        let body = Pretty.print_named(~namespace, store, h);
        print_endline(prefix ++ " — " ++ body);
      },
      rows,
    );
  };
};

let list_store_raw = store => {
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

/* Accept either a name (if bound in the namespace) or a hash prefix. Names
   win on collision with an otherwise valid prefix — this should be rare,
   since names are alphabetic and hash prefixes begin with 'h:' or hex. */
let with_resolved_name_or_prefix = (store, namespace, arg, k) =>
  switch (Namespace.resolve(namespace, arg)) {
  | Some(h) => k(h)
  | None => with_resolved_prefix(store, arg, k)
  };

let lookup_cmd = (store, namespace, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    print_endline(Pretty.print(store, h))
  );

let show_cmd = (store, namespace, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    switch (Store.lookup(store, h)) {
    | None => print_endline("!! internal: resolved hash not in store")
    | Some(node) => print_endline(Pretty.print_shallow(node))
    }
  );

let eval_cmd = (store, att, namespace, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    print_endline("⇒ " ++ eval_with_cache_marker(store, att, h))
  );

let stats_cmd = (store, att, namespace) => {
  let s = Attachment.stats(att);
  print_endline(
    Printf.sprintf(
      "definitions: %d\neval entries: %d\nhits: %d, misses: %d\nnames: %d",
      Store.size(store),
      s.entries,
      s.hits,
      s.misses,
      Namespace.size(namespace),
    ),
  );
};

/* ===== Namespace commands ===== */

let bind_with_hash = (namespace, ~overwrite, ~name, h) =>
  try(
    {
      if (overwrite) {
        Namespace.rebind(namespace, ~name, h);
        print_endline("rebound " ++ name ++ " -> " ++ Hash.short(h));
      } else {
        Namespace.bind(namespace, ~name, h);
        print_endline("bound " ++ name ++ " -> " ++ Hash.short(h));
      };
    }
  ) {
  | Namespace.Name_reserved(n) =>
    print_endline("!! cannot bind name '" ++ n ++ "': reserved keyword")
  | Namespace.Name_already_bound(n) =>
    print_endline("!! name already bound: " ++ n ++ " (use :rebind to overwrite)")
  };

let bind_cmd = (store, namespace, rest, ~overwrite: bool) =>
  switch (split_once(rest)) {
  | None =>
    print_endline(
      "!! usage: " ++ (overwrite ? ":rebind" : ":bind") ++ " <name> <expr-or-hash>",
    )
  | Some((_name, body)) when body == "" =>
    print_endline(
      "!! " ++ (overwrite ? ":rebind" : ":bind") ++ " needs an expression or hash",
    )
  | Some((name, body)) =>
    /* If the argument is a hash prefix (either prefixed with "#"/"h:" or
       a bare hex string), resolve it against the Store. Otherwise parse
       as an expression with edit-time name resolution. */
    if (Hash.looks_like_hash_prefix(body)) {
      with_resolved_prefix(store, body, h =>
        bind_with_hash(namespace, ~overwrite, ~name, h)
      );
    } else {
      switch (parse_and_resolve(~namespace, ~store, body)) {
      | Error(msg) => print_endline("!! " ++ msg)
      | Ok(ast) =>
        let h = Store.ingest(store, Canonicalize.canonicalize(ast));
        bind_with_hash(namespace, ~overwrite, ~name, h);
      };
    }
  };

let bind_hash_cmd = (store, namespace, rest) =>
  switch (split_once(rest)) {
  | None => print_endline("!! usage: :bind-hash <name> <hash-prefix>")
  | Some((_name, prefix)) when prefix == "" =>
    print_endline("!! :bind-hash needs a hash prefix")
  | Some((name, prefix)) =>
    with_resolved_prefix(store, prefix, h =>
      bind_with_hash(namespace, ~overwrite=false, ~name, h)
    )
  };

let unbind_cmd = (namespace, name) =>
  if (name == "") {
    print_endline("!! :unbind needs a name");
  } else if (Namespace.unbind(namespace, ~name)) {
    print_endline("unbound " ++ name);
  } else {
    print_endline("!! no binding for name: " ++ name);
  };

let rename_cmd = (namespace, rest) =>
  switch (split_once(rest)) {
  | None => print_endline("!! usage: :rename <old> <new>")
  | Some((_from, to_)) when to_ == "" =>
    print_endline("!! :rename needs old and new names")
  | Some((from, to_)) =>
    switch (Namespace.rename(namespace, ~from, ~to_)) {
    | Ok () => print_endline("renamed " ++ from ++ " -> " ++ to_)
    | Error(Namespace.Source_unbound) =>
      print_endline("!! no binding for name: " ++ from)
    | Error(Namespace.Target_already_bound) =>
      print_endline("!! target name already bound: " ++ to_)
    | Error(Namespace.Target_reserved) =>
      print_endline("!! target name is a reserved keyword: " ++ to_)
    }
  };

let names_cmd = namespace => {
  let entries = Namespace.entries(namespace);
  if (entries == []) {
    print_endline("(no names bound)");
  } else {
    List.iter(
      ((name, h)) => print_endline(name ++ "\t" ++ Hash.short(h)),
      entries,
    );
  };
};

let name_of_cmd = (store, namespace, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    switch (Namespace.names_of(namespace, h)) {
    | [] => print_endline("(no names bound to " ++ Hash.short(h) ++ ")")
    | names => List.iter(print_endline, names)
    }
  );

let hash_of_cmd = (namespace, name) =>
  if (name == "") {
    print_endline("!! :hash-of needs a name");
  } else {
    switch (Namespace.resolve(namespace, name)) {
    | None => print_endline("!! unbound name: " ++ name)
    | Some(h) => print_endline(Hash.to_string(h))
    };
  };

/* ===== Dispatcher ===== */

let rec run_line =
    (store: Store.t, att: Attachment.t, namespace: Namespace.t, line: string): bool =>
  switch (split_command(line)) {
  | None =>
    let trimmed = String.trim(line);
    if (trimmed != "") {
      register_and_eval(store, att, namespace, trimmed);
    };
    true;
  | Some((":quit", _))
  | Some((":exit", _)) => false
  | Some((":help", _)) =>
    print_endline(help_text);
    true;
  | Some((":list", "raw")) =>
    list_store_raw(store);
    true;
  | Some((":list", "")) =>
    list_store(store, namespace);
    true;
  | Some((":list", arg)) =>
    print_endline("!! unknown :list argument: " ++ arg ++ "  (try :list or :list raw)");
    true;
  | Some((":dag", _)) =>
    dag_store(store);
    true;
  | Some((":stats", _)) =>
    stats_cmd(store, att, namespace);
    true;
  | Some((":register-only", rest)) =>
    if (rest == "") {
      print_endline("!! :register-only needs an expression");
    } else {
      register_only(store, namespace, rest);
    };
    true;
  | Some((":eval-expr", rest)) =>
    if (rest == "") {
      print_endline("!! :eval-expr needs an expression");
    } else {
      eval_expr(store, att, namespace, rest);
    };
    true;
  | Some((":lookup", arg)) =>
    if (arg == "") {
      print_endline("!! :lookup needs a name or hash prefix");
    } else {
      lookup_cmd(store, namespace, arg);
    };
    true;
  | Some((":show", arg)) =>
    if (arg == "") {
      print_endline("!! :show needs a name or hash prefix");
    } else {
      show_cmd(store, namespace, arg);
    };
    true;
  | Some((":eval", arg)) =>
    if (arg == "") {
      print_endline("!! :eval needs a name or hash prefix");
    } else {
      eval_cmd(store, att, namespace, arg);
    };
    true;
  | Some((":bind", rest)) =>
    bind_cmd(store, namespace, rest, ~overwrite=false);
    true;
  | Some((":rebind", rest)) =>
    bind_cmd(store, namespace, rest, ~overwrite=true);
    true;
  | Some((":bind-hash", rest)) =>
    bind_hash_cmd(store, namespace, rest);
    true;
  | Some((":unbind", name)) =>
    unbind_cmd(namespace, name);
    true;
  | Some((":rename", rest)) =>
    rename_cmd(namespace, rest);
    true;
  | Some((":names", _)) =>
    names_cmd(namespace);
    true;
  | Some((":name-of", arg)) =>
    if (arg == "") {
      print_endline("!! :name-of needs a name or hash prefix");
    } else {
      name_of_cmd(store, namespace, arg);
    };
    true;
  | Some((":hash-of", name)) =>
    hash_of_cmd(namespace, name);
    true;
  | Some((":load", path)) =>
    if (path == "") {
      print_endline("!! :load needs a file path");
    } else {
      load_script(store, att, namespace, path);
    };
    true;
  | Some((cmd, _)) =>
    print_endline("!! unknown command: " ++ cmd ++ "  (try :help)");
    true;
  }
and load_script =
    (store: Store.t, att: Attachment.t, namespace: Namespace.t, path: string)
    : unit =>
  switch (open_in(path)) {
  | exception Sys_error(msg) => print_endline("!! " ++ msg)
  | ic =>
    let rec go = () =>
      switch (input_line(ic)) {
      | exception End_of_file => close_in(ic)
      | line =>
        let trimmed = String.trim(line);
        let is_blank = String.length(trimmed) == 0;
        let is_comment = !is_blank && trimmed.[0] == '#';
        if (!is_blank && !is_comment) {
          print_endline("▸ " ++ trimmed);
          let _: bool = run_line(store, att, namespace, trimmed);
          ();
        };
        go();
      };
    go();
  };

let rec loop = (store, att, namespace) => {
  print_string("> ");
  switch (read_line()) {
  | exception End_of_file =>
    print_newline();
    ();
  | line =>
    if (run_line(store, att, namespace, line)) {
      loop(store, att, namespace);
    }
  };
};

let usage = "Usage: main.exe [--load FILE]... [--no-repl]
  --load FILE   Run FILE as a sequence of REPL commands before prompting
  --no-repl     After processing --load, exit instead of entering the REPL
  --help        Show this message";

type cli_opts = {
  loads: list(string),
  no_repl: bool,
};

let parse_argv = (argv: array(string)): cli_opts => {
  let rec go = (i, acc) =>
    if (i >= Array.length(argv)) {
      acc;
    } else {
      switch (argv[i]) {
      | "--load" | "-l" when i + 1 < Array.length(argv) =>
        go(i + 2, {...acc, loads: [argv[i + 1], ...acc.loads]})
      | "--no-repl" => go(i + 1, {...acc, no_repl: true})
      | "--help" | "-h" =>
        print_endline(usage);
        exit(0);
      | other =>
        print_endline("!! ignoring unknown argument: " ++ other);
        go(i + 1, acc);
      };
    };
  let opts = go(1, {loads: [], no_repl: false});
  {...opts, loads: List.rev(opts.loads)};
};

let () = {
  let opts = parse_argv(Sys.argv);
  print_endline(banner);
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Eval.descriptor);
  let namespace = Namespace.create();
  List.iter(load_script(store, att, namespace), opts.loads);
  if (!opts.no_repl) {
    loop(store, att, namespace);
  };
};
