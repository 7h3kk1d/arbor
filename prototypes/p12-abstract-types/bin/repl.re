/* p12 REPL — drives the editing-context model by hand.

   Two opening gestures: `:abstract` (create + open) and `:open` (re-open to add
   ops later). Bindings made while a type is open auto-seal iff they need the
   representation (minimal sealing); everything else is an ordinary term. The
   default context opens nothing — that is where consumers are written and
   abstract types stay opaque. */

open P12_substrate;

let st = Store.create();
let ns = Namespace.create();
let mint_src = Mint.make_source();
let ctx = ref(Editing_context.make());

let err = m => Printf.printf("error: %s\n", m);
let pty = h => Pretty.ty(~ns, ~st, h);

let name_or_hash = h =>
  switch (Namespace.name_of(ns, h)) {
  | Some(n) => n
  | None => Hash.short(h)
  };

let rebind = (name, h) =>
  try(Ok(Namespace.rebind(ns, ~name, h))) {
  | Namespace.Name_reserved(n) => Error("'" ++ n ++ "' is reserved")
  };

/* ---- tiny string helpers ---- */

let starts_with = (s, p) =>
  String.length(s) >= String.length(p) && String.sub(s, 0, String.length(p)) == p;

let drop_prefix = (s, p) =>
  String.trim(String.sub(s, String.length(p), String.length(s) - String.length(p)));

let find_sub = (s, sub) => {
  let n = String.length(s);
  let m = String.length(sub);
  let rec go = i =>
    if (i + m > n) {
      None;
    } else if (String.sub(s, i, m) == sub) {
      Some(i);
    } else {
      go(i + 1);
    };
  go(0);
};

let split_first = (s, sub) =>
  switch (find_sub(s, sub)) {
  | None => None
  | Some(i) =>
    Some((
      String.sub(s, 0, i),
      String.sub(s, i + String.length(sub), String.length(s) - i - String.length(sub)),
    ))
  };

/* ---- commands ---- */

let cmd_abstract = rest =>
  switch (split_first(rest, " = ")) {
  | None => print_endline("usage: :abstract <name> = <witness-type>")
  | Some((name, wty)) =>
    let name = String.trim(name);
    switch (Parse.parse_ty(String.trim(wty))) {
    | Error(e) => err(e)
    | Ok(sty) =>
      switch (Resolver.resolve_ty(~ns, ~st, sty)) {
      | Error(e) => err(e)
      | Ok(witness_h) =>
        let m = Mint.fresh(mint_src);
        let opaque = Store.ingest_type(st, Tnode.Opaque({mint: m, witness: witness_h}));
        switch (rebind(name, opaque)) {
        | Error(e) => err(e)
        | Ok(_) =>
          Editing_context.open_type(ctx^, opaque);
          Printf.printf(
            "abstract type %s = opaque(%s) over %s  [opened]\n",
            name,
            Mint.short(m),
            pty(witness_h),
          );
        }
      }
    };
  };

let cmd_type = rest =>
  switch (split_first(rest, " = ")) {
  | None => print_endline("usage: :type <name> = <type>")
  | Some((name, tys)) =>
    let name = String.trim(name);
    switch (Parse.parse_ty(String.trim(tys))) {
    | Error(e) => err(e)
    | Ok(sty) =>
      switch (Resolver.resolve_ty(~ns, ~st, sty)) {
      | Error(e) => err(e)
      | Ok(h) =>
        switch (rebind(name, h)) {
        | Error(e) => err(e)
        | Ok(_) => Printf.printf("type %s = %s\n", name, pty(h))
        }
      }
    };
  };

let cmd_open = name => {
  let name = String.trim(name);
  switch (Namespace.resolve(ns, name)) {
  | None => err("unbound: " ++ name)
  | Some(h) =>
    switch (Store.find(st, h)) {
    | Some(Definition.Type(Tnode.Opaque(_))) =>
      Editing_context.open_type(ctx^, h);
      Printf.printf("opened %s\n", name);
    | Some(Definition.Type(_)) => err(name ++ " is a concrete type, not abstract")
    | _ => err(name ++ " is not a type")
    }
  };
};

let cmd_let = rest =>
  switch (split_first(rest, " = ")) {
  | None => print_endline("usage: :let <name> : <type> = <expr>")
  | Some((lhs, exprs)) =>
    switch (split_first(String.trim(lhs), " : ")) {
    | None => print_endline("usage: :let <name> : <type> = <expr>")
    | Some((name, tys)) =>
      let name = String.trim(name);
      switch (Parse.parse_ty(String.trim(tys)), Parse.parse_expr(String.trim(exprs))) {
      | (Error(e), _) => err(e)
      | (_, Error(e)) => err(e)
      | (Ok(sty), Ok(sexpr)) =>
        switch (Resolver.resolve_ty(~ns, ~st, sty)) {
        | Error(e) => err(e)
        | Ok(ann) =>
          switch (Resolver.resolve(~ctx=[], ~ns, ~st, sexpr)) {
          | Error(e) => err(e)
          | Ok(node) =>
            switch (Editing_context.commit(st, ctx^, ~term=node, ~ann)) {
            | Error(e) => err(e)
            | Ok((h, kind)) =>
              let _ = rebind(name, h);
              let ks =
                switch (kind) {
                | `Sealed => "sealed"
                | `Normal => "normal"
                };
              Printf.printf("%s : %s  [%s]  %s\n", name, pty(ann), ks, Hash.short(h));
            }
          }
        }
      };
    }
  };

let cmd_impl = name => {
  let name = String.trim(name);
  switch (Namespace.resolve(ns, name)) {
  | None => err("unbound: " ++ name)
  | Some(h) =>
    let ops = Store.impl_set(st, h);
    Printf.printf("implementation set of %s (%d):\n", name, List.length(ops));
    List.iter(
      o => {
        let t =
          switch (Store.type_of(st, o)) {
          | Some(t) => pty(t)
          | None => "?"
          };
        Printf.printf("  %s : %s  %s\n", name_or_hash(o), t, Hash.short(o));
      },
      ops,
    );
  };
};

let cmd_show = name => {
  let name = String.trim(name);
  switch (Namespace.resolve(ns, name)) {
  | None => err("unbound: " ++ name)
  | Some(h) =>
    switch (Store.find(st, h)) {
    | Some(Definition.Type(_)) =>
      Printf.printf("%s : type = %s  %s\n", name, pty(h), Hash.short(h))
    | Some(Definition.Term(node)) =>
      let t =
        switch (Store.type_of(st, h)) {
        | Some(t) => pty(t)
        | None => "?"
        };
      let extra =
        switch (node) {
        | Node.Seal({opens, _}) =>
          " [sealed, opens " ++ String.concat(", ", List.map(name_or_hash, opens)) ++ "]"
        | _ => " [normal]"
        };
      Printf.printf("%s : %s%s  %s\n", name, t, extra, Hash.short(h));
      let src =
        switch (node) {
        | Node.Seal({impl, _}) =>
          switch (Store.find(st, impl)) {
          | Some(Definition.Term(inode)) => "  impl = " ++ Pretty.term(~ns, ~st, inode)
          | _ => ""
          }
        | _ => "  = " ++ Pretty.term(~ns, ~st, node)
        };
      print_endline(src);
    | None => err("dangling: " ++ name)
    }
  };
};

let cmd_expr = line =>
  switch (Parse.parse_expr(line)) {
  | Error(e) => err(e)
  | Ok(se) =>
    switch (Resolver.resolve(~ctx=[], ~ns, ~st, se)) {
    | Error(e) => err(e)
    | Ok(node) =>
      let env = Store.build_env(st);
      switch (Typecheck.synth_top(env, Editing_context.opens(ctx^), node)) {
      | Error(e) => err("type error: " ++ e)
      | Ok(t) =>
        switch (Eval.eval_top(st, node)) {
        | Ok(v) => Printf.printf(": %s = %s\n", pty(t), Eval.to_string(v))
        | Error(m) => Printf.printf(": %s  (stuck: %s)\n", pty(t), m)
        }
      }
    }
  };

let show_ctx = () => {
  let opens = Editing_context.opens(ctx^);
  if (opens == []) {
    print_endline("context: nothing open (default — abstract types opaque)");
  } else {
    Printf.printf("context opens: %s\n", String.concat(", ", List.map(name_or_hash, opens)));
  };
};

let show_names = () =>
  List.iter(
    ((n, h)) => {
      let k =
        switch (Store.find(st, h)) {
        | Some(Definition.Type(_)) => "type"
        | Some(Definition.Term(_)) => "term"
        | None => "?"
        };
      Printf.printf("  %s  (%s)  %s\n", n, k, Hash.short(h));
    },
    Namespace.entries(ns),
  );

let print_help = () => {
  print_endline("commands:");
  print_endline("  :abstract <name> = <witness>     create an abstract type (mints; opens it)");
  print_endline("  :type <name> = <type>            create a concrete type alias");
  print_endline("  :open <name>                     open an existing abstract type here");
  print_endline("  :close                           reset context (nothing open)");
  print_endline("  :ctx                             show the current open set");
  print_endline("  :let <name> : <type> = <expr>    bind a term (auto-seals if it needs the rep)");
  print_endline("  :impl <name>                     show an abstract type's implementation set");
  print_endline("  :show <name>                     show a definition");
  print_endline("  :ls                              list the namespace");
  print_endline("  <expr>                           show an expression's type");
  print_endline("  :quit");
  print_endline("note: put a space after the lambda dot:  \\x: Int. x");
  print_endline("example:");
  print_endline("  :abstract Counter.t = Int");
  print_endline("  :let Counter.empty : Counter.t = 0");
  print_endline("  :let Counter.incr : Counter.t -> Counter.t = \\x: Counter.t. x + 1");
  print_endline("  :let Counter.get : Counter.t -> Int = \\x: Counter.t. x");
  print_endline("  :close");
  print_endline("  :let bump2 : Counter.t -> Counter.t = \\c: Counter.t. Counter.incr (Counter.incr c)");
};

let handle = line => {
  let line = String.trim(line);
  if (line == "") {
    ();
  } else if (line == ":quit" || line == ":q") {
    exit(0);
  } else if (line == ":help" || line == ":h") {
    print_help();
  } else if (line == ":close" || line == ":reset") {
    ctx := Editing_context.make();
    print_endline("context reset — nothing open.");
  } else if (line == ":ctx" || line == ":context") {
    show_ctx();
  } else if (line == ":ls" || line == ":names") {
    show_names();
  } else if (starts_with(line, ":abstract ")) {
    cmd_abstract(drop_prefix(line, ":abstract "));
  } else if (starts_with(line, ":type ")) {
    cmd_type(drop_prefix(line, ":type "));
  } else if (starts_with(line, ":open ")) {
    cmd_open(drop_prefix(line, ":open "));
  } else if (starts_with(line, ":let ")) {
    cmd_let(drop_prefix(line, ":let "));
  } else if (starts_with(line, ":impl ")) {
    cmd_impl(drop_prefix(line, ":impl "));
  } else if (starts_with(line, ":show ")) {
    cmd_show(drop_prefix(line, ":show "));
  } else if (starts_with(line, ":")) {
    print_endline("unknown command — :help for the list.");
  } else {
    cmd_expr(line);
  };
};

let () = {
  print_endline("p12 — abstract types REPL. :help for commands, :quit to exit.");
  let rec loop = () => {
    Printf.printf("p12> %!");
    switch (
      try(Some(input_line(stdin))) {
      | End_of_file => None
      }
    ) {
    | None => print_newline()
    | Some(line) =>
      (
        try(handle(line)) {
        | e => Printf.printf("uncaught: %s\n", Printexc.to_string(e))
        }
      );
      loop();
    };
  };
  loop();
};
