open P5_multi_language;

let banner = "p5-multi-language REPL — arithmetic + untyped λ-calculus in one Store, with Church-encoding translation (Ctrl-D or :quit to exit)";

let help_text = "Commands:
  <expr>                 parse, resolve names, ingest (hash-cons), evaluate
                         in the CURRENT language (:lang to switch)
  :lang [arith|lc]       get or set the current language (default: lc)
  :register-only <e>     parse, resolve, ingest without evaluating
  :eval-expr <e>         evaluate without binding a name
  :bind <name> <e|#h>    bind <name> to the hash of an expression or to an
                         already-stored hash prefix (e.g. '#1ec244c78f42')
  :rebind <name> <e|#h>  like :bind, but overwrites an existing binding
  :bind-hash <n> <pfx>   (alias) bind a name to an already-stored hash prefix
  :unbind <name>         remove a binding
  :rename <old> <new>    rename a binding (unbind + bind to the same hash)
  :names                 list all [lang] (name, hash) bindings, sorted by name
  :name-of <name|#pfx>   list names pointing at a stored hash
  :hash-of <name>        print the hash bound to <name>
  :list                  enumerate stored nodes; rows show
                         '[lang] hash [names] — body' (body uses named children)
  :list closed           filter :list to hashes whose term is closed
                         (arith rows are always closed; useful for lc)
  :list raw              :list without name substitution or name brackets
  :dag                   enumerate stored nodes, children shown as hashes
  :lookup <name|#pfx>    print '[lang] <reconstructed body>'
  :show <name|#pfx>      print '[lang] <shallow node>'
  :eval <name|#pfx>      re-run evaluation (dispatches by language)
  :step-limit [n]        get or set the λ-calculus β-reduction step limit
  :translate <name|#pfx> run arith→lc Church translator on an arith
                         definition; record and cache the result
  :translations [arg]    list all cached translations (arg empty) or the
                         translation for a specific arith source
  :stats                 counts for definitions (per language), eval cache,
                         translation cache, names
  :load <path>           execute REPL commands from a file; '#' starts comments
  :help                  show this message
  :quit | :exit          leave the REPL

Arithmetic syntax: true, false, 0, succ e, pred e, iszero e, if c then t else e
λ-calculus syntax: x, \\x. t, t t (left-associative), (t)
Hashes display with a leading '#'. Inputs accept '#<hex>' or legacy 'h:<hex>'.";

/* ==================== Parsing ==================== */

let parse_arith =
    (input: string): result(Arith_surface_ast.t, string) => {
  let lexbuf = Lexing.from_string(input);
  try(Ok(Arith_parser.main(Arith_lexer.token, lexbuf))) {
  | Arith_parser.Error => Error("parse error")
  | Arith_lexer.Lex_error(msg) => Error(msg)
  };
};

let parse_lc = (input: string): result(Lc_surface_ast.t, string) => {
  let lexbuf = Lexing.from_string(input);
  try(Ok(Lc_parser.main(Lc_lexer.token, lexbuf))) {
  | Lc_parser.Error => Error("parse error")
  | Lc_lexer.Lex_error(msg) => Error(msg)
  };
};

let parse_resolve_ingest_arith =
    (~namespace: Namespace.t, ~store: Store.t, input: string)
    : result(Hash.t, string) =>
  switch (parse_arith(input)) {
  | Error(msg) => Error(msg)
  | Ok(surface) =>
    switch (Resolver.resolve_arith(~namespace, ~store, surface)) {
    | Error(e) => Error(Resolver.error_to_string(e))
    | Ok(ast) =>
      Ok(Store.ingest_arith(store, Arith_canonicalize.canonicalize(ast)))
    }
  };

let parse_resolve_ingest_lc =
    (~namespace: Namespace.t, ~store: Store.t, input: string)
    : result(Hash.t, string) =>
  switch (parse_lc(input)) {
  | Error(msg) => Error(msg)
  | Ok(surface) =>
    switch (Resolver.resolve_lc(~namespace, ~store, surface)) {
    | Error(e) => Error(Resolver.error_to_string(e))
    | Ok(ast) =>
      Ok(Store.ingest_lc(store, Lc_canonicalize.canonicalize(ast)))
    }
  };

let parse_resolve_ingest =
    (~lang: string, ~namespace: Namespace.t, ~store: Store.t, input: string)
    : result(Hash.t, string) =>
  switch (lang) {
  | "arith" => parse_resolve_ingest_arith(~namespace, ~store, input)
  | "lc" => parse_resolve_ingest_lc(~namespace, ~store, input)
  | other => Error("unknown language: " ++ other)
  };

/* ==================== Small string helpers ==================== */

let split_command = (line: string): option((string, string)) => {
  let line = String.trim(line);
  if (String.length(line) == 0 || line.[0] != ':') {
    None;
  } else {
    switch (String.index_opt(line, ' ')) {
    | None => Some((line, ""))
    | Some(i) =>
      let cmd = String.sub(line, 0, i);
      let rest =
        String.trim(
          String.sub(line, i + 1, String.length(line) - i - 1),
        );
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

/* ==================== Evaluation helpers ==================== */

let eval_with_cache_marker =
    (
      store: Store.t,
      att: Attachment.t,
      namespace: Namespace.t,
      ~step_limit: int,
      h: Hash.t,
    )
    : string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Definition.Arith(_)) =>
    let was_cached = Option.is_some(Arith_eval.peek_cache(att, h));
    let r = Arith_eval.eval(~store, ~att, h);
    let rendered =
      switch (r) {
      | Arith_eval.Value(h') => Pretty.print_named(~namespace, store, h')
      | Arith_eval.Stuck(h') =>
        "⟂ stuck at: " ++ Pretty.print_named(~namespace, store, h')
      };
    was_cached ? rendered ++ " (cached)" : rendered;
  | Some(Definition.Lc(_)) =>
    let was_cached = Option.is_some(Lc_eval.peek_cache(att, h));
    let r = Lc_eval.eval(~store, ~att, ~step_limit, h);
    let rendered =
      switch (r) {
      | Lc_eval.Value(h') => Pretty.print_named(~namespace, store, h')
      | Lc_eval.Stuck(h') =>
        "⟂ stuck at: " ++ Pretty.print_named(~namespace, store, h')
      | Lc_eval.StepLimit(h') =>
        "… step limit reached at: "
        ++ Pretty.print_named(~namespace, store, h')
      };
    was_cached ? rendered ++ " (cached)" : rendered;
  };

let register_and_eval =
    (store, att, namespace, ~lang, ~step_limit, src) =>
  switch (parse_resolve_ingest(~lang, ~namespace, ~store, src)) {
  | Error(msg) => print_endline("!! " ++ msg)
  | Ok(h) =>
    print_endline(Hash.short(h));
    print_endline(
      "⇒ " ++ eval_with_cache_marker(store, att, namespace, ~step_limit, h),
    );
  };

let register_only = (store, namespace, ~lang, src) =>
  switch (parse_resolve_ingest(~lang, ~namespace, ~store, src)) {
  | Error(msg) => print_endline("!! " ++ msg)
  | Ok(h) => print_endline(Hash.short(h))
  };

let eval_expr = (store, att, namespace, ~lang, ~step_limit, src) =>
  switch (parse_resolve_ingest(~lang, ~namespace, ~store, src)) {
  | Error(msg) => print_endline("!! " ++ msg)
  | Ok(h) =>
    print_endline(
      "⇒ " ++ eval_with_cache_marker(store, att, namespace, ~step_limit, h),
    )
  };

/* ==================== Listings (language-labelled) ==================== */

let format_name_bracket = (names: list(string)): string =>
  switch (names) {
  | [] => ""
  | ns => " [" ++ String.concat(", ", ns) ++ "]"
  };

let row_prefix = (hash: Hash.t, names: list(string)): string =>
  Hash.short(hash) ++ format_name_bracket(names);

let pad_right = (s: string, n: int): string => {
  let w = String.length(s);
  if (w >= n) {
    s;
  } else {
    s ++ String.make(n - w, ' ');
  };
};

let list_store = (~closed_only: bool, store, namespace) => {
  let entries = Store.entries(store);
  let entries =
    if (closed_only) {
      List.filter(((h, _)) => Pretty.is_closed_hash(store, h), entries);
    } else {
      entries;
    };
  if (entries == []) {
    print_endline(closed_only ? "(no closed terms)" : "(store empty)");
  } else {
    let rows =
      List.map(
        ((h, def)) =>
          (Pretty.language_tag(def), h, Namespace.names_of(namespace, h)),
        entries,
      );
    let max_prefix =
      List.fold_left(
        (acc, (_, h, names)) => max(acc, String.length(row_prefix(h, names))),
        0,
        rows,
      );
    List.iter(
      ((tag, h, names)) => {
        let prefix = pad_right(row_prefix(h, names), max_prefix);
        let body = Pretty.print_named(~namespace, store, h);
        print_endline(tag ++ " " ++ prefix ++ " — " ++ body);
      },
      rows,
    );
  };
};

let list_store_raw = (store) => {
  let entries = Store.entries(store);
  if (entries == []) {
    print_endline("(store empty)");
  } else {
    List.iter(
      ((h, def)) =>
        print_endline(
          Pretty.language_tag(def)
          ++ " "
          ++ Hash.short(h)
          ++ " — "
          ++ Pretty.print(store, h),
        ),
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
      ((h, def)) =>
        print_endline(
          Pretty.language_tag(def)
          ++ " "
          ++ Hash.short(h)
          ++ " — "
          ++ Pretty.print_shallow(def),
        ),
      entries,
    );
  };
};

/* ==================== Hash-prefix / name-prefix plumbing ==================== */

let with_resolved_prefix = (store, prefix, k) =>
  switch (Store.resolve_prefix(store, prefix)) {
  | NotFound => print_endline("!! no match for prefix: " ++ prefix)
  | Ambiguous(hs) =>
    print_endline(
      "!! ambiguous prefix ("
      ++ string_of_int(List.length(hs))
      ++ " matches):",
    );
    List.iter(h => print_endline("   " ++ Hash.short(h)), hs);
  | Found(h) => k(h)
  };

let with_resolved_name_or_prefix = (store, namespace, arg, k) =>
  switch (Namespace.resolve(namespace, arg)) {
  | Some(h) => k(h)
  | None => with_resolved_prefix(store, arg, k)
  };

let lookup_cmd = (store, namespace, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    print_endline(
      Pretty.language_tag_of_hash(store, h)
      ++ " "
      ++ Pretty.print_named(~namespace, store, h),
    )
  );

let show_cmd = (store, namespace, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    switch (Store.lookup(store, h)) {
    | None => print_endline("!! internal: resolved hash not in store")
    | Some(def) =>
      print_endline(
        Pretty.language_tag(def) ++ " " ++ Pretty.print_shallow(def),
      )
    }
  );

let eval_cmd = (store, att, namespace, ~step_limit, arg) =>
  with_resolved_name_or_prefix(store, namespace, arg, h =>
    print_endline(
      "⇒ " ++ eval_with_cache_marker(store, att, namespace, ~step_limit, h),
    )
  );

/* ==================== Stats / misc ==================== */

let stats_cmd = (store, att, namespace) => {
  let s = Attachment.stats(att);
  /* Split definition counts by language. */
  let (arith_n, lc_n) =
    Store.entries(store)
    |> List.fold_left(
         ((a, l), (_, def)) =>
           switch (def) {
           | Definition.Arith(_) => (a + 1, l)
           | Definition.Lc(_) => (a, l + 1)
           },
         (0, 0),
       );
  let translations_n =
    List.length(Arith_to_lc_church.all_translations(att));
  print_endline(
    Printf.sprintf(
      "definitions: %d (arith %d, lc %d)\neval entries: %d\nhits: %d, misses: %d\ntranslations: %d\nnames: %d",
      Store.size(store),
      arith_n,
      lc_n,
      s.entries,
      s.hits,
      s.misses,
      translations_n,
      Namespace.size(namespace),
    ),
  );
};

let step_limit_cmd = (step_limit: ref(int), arg: string) =>
  if (arg == "") {
    print_endline("step limit: " ++ string_of_int(step_limit^));
  } else {
    switch (int_of_string_opt(arg)) {
    | None => print_endline("!! :step-limit expects a non-negative integer")
    | Some(n) when n < 0 =>
      print_endline("!! :step-limit expects a non-negative integer")
    | Some(n) =>
      step_limit := n;
      print_endline("step limit set to " ++ string_of_int(n));
    };
  };

/* ==================== Language mode ==================== */

let lang_cmd = (current_lang: ref(string), arg: string) =>
  switch (arg) {
  | "" => print_endline("current language: " ++ current_lang^)
  | "arith"
  | "lc" =>
    current_lang := arg;
    print_endline("language set to " ++ arg);
  | other =>
    print_endline(
      "!! unknown language: " ++ other ++ "  (expected 'arith' or 'lc')",
    )
  };

/* ==================== Namespace commands ==================== */

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
    print_endline(
      "!! name already bound: " ++ n ++ " (use :rebind to overwrite)",
    )
  };

let bind_cmd = (store, namespace, ~lang, rest, ~overwrite: bool) =>
  switch (split_once(rest)) {
  | None =>
    print_endline(
      "!! usage: "
      ++ (overwrite ? ":rebind" : ":bind")
      ++ " <name> <expr-or-hash>",
    )
  | Some((_name, body)) when body == "" =>
    print_endline(
      "!! "
      ++ (overwrite ? ":rebind" : ":bind")
      ++ " needs an expression or hash",
    )
  | Some((name, body)) =>
    if (Hash.looks_like_hash_prefix(body)) {
      with_resolved_prefix(store, body, h =>
        bind_with_hash(namespace, ~overwrite, ~name, h)
      );
    } else {
      switch (parse_resolve_ingest(~lang, ~namespace, ~store, body)) {
      | Error(msg) => print_endline("!! " ++ msg)
      | Ok(h) => bind_with_hash(namespace, ~overwrite, ~name, h)
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

let names_cmd = (store, namespace) => {
  let entries = Namespace.entries(namespace);
  if (entries == []) {
    print_endline("(no names bound)");
  } else {
    List.iter(
      ((name, h)) =>
        print_endline(
          Pretty.language_tag_of_hash(store, h)
          ++ " "
          ++ name
          ++ "\t"
          ++ Hash.short(h),
        ),
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

/* ==================== Translation commands ==================== */

let format_translation_row = (src, tgt): string =>
  Hash.short(src)
  ++ " -[arith-to-lc-church:v1]-> "
  ++ Hash.short(tgt);

let translate_cmd = (store, att, namespace, arg) =>
  if (arg == "") {
    print_endline("!! :translate needs a name or hash prefix");
  } else {
    with_resolved_name_or_prefix(store, namespace, arg, h => {
      let was_cached =
        Option.is_some(Arith_to_lc_church.peek_translation(att, h));
      switch (Arith_to_lc_church.translate(~store, ~att, h)) {
      | Error(e) =>
        print_endline("!! " ++ Arith_to_lc_church.error_to_string(e))
      | Ok((target, _)) =>
        let verb = was_cached ? "cached" : "translated";
        print_endline(
          verb ++ " " ++ format_translation_row(h, target),
        );
      };
    });
  };

let translations_cmd = (store, att, namespace, arg) =>
  if (arg == "") {
    let pairs = Arith_to_lc_church.all_translations(att);
    if (pairs == []) {
      print_endline("(no translations yet)");
    } else {
      List.iter(
        ((src, tgt)) => print_endline(format_translation_row(src, tgt)),
        pairs,
      );
    };
  } else {
    with_resolved_name_or_prefix(store, namespace, arg, h =>
      switch (Arith_to_lc_church.peek_translation(att, h)) {
      | None =>
        print_endline(
          "(no cached translation for " ++ Hash.short(h) ++ ")",
        )
      | Some(tgt) => print_endline(format_translation_row(h, tgt))
      }
    );
  };

/* ==================== Dispatcher ==================== */

let rec run_line =
        (
          store: Store.t,
          att: Attachment.t,
          namespace: Namespace.t,
          step_limit: ref(int),
          current_lang: ref(string),
          line: string,
        )
        : bool =>
  switch (split_command(line)) {
  | None =>
    let trimmed = String.trim(line);
    if (trimmed != "") {
      register_and_eval(
        store,
        att,
        namespace,
        ~lang=current_lang^,
        ~step_limit=step_limit^,
        trimmed,
      );
    };
    true;
  | Some((":quit", _))
  | Some((":exit", _)) => false
  | Some((":help", _)) =>
    print_endline(help_text);
    true;
  | Some((":lang", arg)) =>
    lang_cmd(current_lang, arg);
    true;
  | Some((":list", "raw")) =>
    list_store_raw(store);
    true;
  | Some((":list", "closed")) =>
    list_store(~closed_only=true, store, namespace);
    true;
  | Some((":list", "")) =>
    list_store(~closed_only=false, store, namespace);
    true;
  | Some((":list", arg)) =>
    print_endline(
      "!! unknown :list argument: "
      ++ arg
      ++ "  (try :list, :list closed, or :list raw)",
    );
    true;
  | Some((":dag", _)) =>
    dag_store(store);
    true;
  | Some((":stats", _)) =>
    stats_cmd(store, att, namespace);
    true;
  | Some((":step-limit", arg)) =>
    step_limit_cmd(step_limit, arg);
    true;
  | Some((":register-only", rest)) =>
    if (rest == "") {
      print_endline("!! :register-only needs an expression");
    } else {
      register_only(store, namespace, ~lang=current_lang^, rest);
    };
    true;
  | Some((":eval-expr", rest)) =>
    if (rest == "") {
      print_endline("!! :eval-expr needs an expression");
    } else {
      eval_expr(
        store,
        att,
        namespace,
        ~lang=current_lang^,
        ~step_limit=step_limit^,
        rest,
      );
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
      eval_cmd(store, att, namespace, ~step_limit=step_limit^, arg);
    };
    true;
  | Some((":bind", rest)) =>
    bind_cmd(store, namespace, ~lang=current_lang^, rest, ~overwrite=false);
    true;
  | Some((":rebind", rest)) =>
    bind_cmd(store, namespace, ~lang=current_lang^, rest, ~overwrite=true);
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
    names_cmd(store, namespace);
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
  | Some((":translate", arg)) =>
    translate_cmd(store, att, namespace, arg);
    true;
  | Some((":translations", arg)) =>
    translations_cmd(store, att, namespace, arg);
    true;
  | Some((":load", path)) =>
    if (path == "") {
      print_endline("!! :load needs a file path");
    } else {
      load_script(store, att, namespace, step_limit, current_lang, path);
    };
    true;
  | Some((cmd, _)) =>
    print_endline("!! unknown command: " ++ cmd ++ "  (try :help)");
    true;
  }
and load_script =
    (
      store: Store.t,
      att: Attachment.t,
      namespace: Namespace.t,
      step_limit: ref(int),
      current_lang: ref(string),
      path: string,
    )
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
          let _: bool =
            run_line(store, att, namespace, step_limit, current_lang, trimmed);
          ();
        };
        go();
      };
    go();
  };

let rec loop = (store, att, namespace, step_limit, current_lang) => {
  print_string("(" ++ current_lang^ ++ ") > ");
  switch (read_line()) {
  | exception End_of_file =>
    print_newline();
    ();
  | line =>
    if (run_line(store, att, namespace, step_limit, current_lang, line)) {
      loop(store, att, namespace, step_limit, current_lang);
    }
  };
};

let usage = "Usage: main.exe [--load FILE]... [--no-repl] [--step-limit N] [--lang LANG]
  --load FILE        Run FILE as a sequence of REPL commands before prompting
  --no-repl          After processing --load, exit instead of entering the REPL
  --step-limit N     Set the initial β-reduction step limit (default 10000)
  --lang LANG        Set the initial language: 'arith' or 'lc' (default 'lc')
  --help             Show this message";

type cli_opts = {
  loads: list(string),
  no_repl: bool,
  step_limit: int,
  lang: string,
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
      | "--step-limit" when i + 1 < Array.length(argv) =>
        switch (int_of_string_opt(argv[i + 1])) {
        | Some(n) when n >= 0 => go(i + 2, {...acc, step_limit: n})
        | _ =>
          print_endline("!! --step-limit expects a non-negative integer");
          exit(1);
        }
      | "--lang" when i + 1 < Array.length(argv) =>
        switch (argv[i + 1]) {
        | "arith"
        | "lc" => go(i + 2, {...acc, lang: argv[i + 1]})
        | other =>
          print_endline("!! --lang expects 'arith' or 'lc', got: " ++ other);
          exit(1);
        }
      | "--help" | "-h" =>
        print_endline(usage);
        exit(0);
      | other =>
        print_endline("!! ignoring unknown argument: " ++ other);
        go(i + 1, acc);
      };
    };
  let opts =
    go(
      1,
      {
        loads: [],
        no_repl: false,
        step_limit: Lc_eval.default_step_limit,
        lang: "lc",
      },
    );
  {...opts, loads: List.rev(opts.loads)};
};

let () = {
  let opts = parse_argv(Sys.argv);
  print_endline(banner);
  let store = Store.create();
  let att = Attachment.create();
  Attachment.register_descriptor(att, Arith_eval.descriptor);
  Attachment.register_descriptor(att, Lc_eval.descriptor);
  Attachment.register_descriptor(att, Arith_to_lc_church.descriptor);
  let namespace = Namespace.create();
  let step_limit = ref(opts.step_limit);
  let current_lang = ref(opts.lang);
  List.iter(
    load_script(store, att, namespace, step_limit, current_lang),
    opts.loads,
  );
  if (!opts.no_repl) {
    loop(store, att, namespace, step_limit, current_lang);
  };
};
