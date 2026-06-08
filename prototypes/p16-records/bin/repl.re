/* p16 REPL — drives the editing-context model by hand.

   Two opening gestures: `:opaque` (create + open) and `:open` (re-open to add
   ops later). Bindings made while a type is open auto-seal iff they need the
   representation (minimal sealing); everything else is an ordinary term. The
   default context opens nothing — that is where consumers are written and
   opaque types stay sealed. */

open P16_substrate;

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
  | None => print_endline("usage: :opaque <name> = <witness-type>")
  | Some((name, wty)) =>
    let name = String.trim(name);
    switch (Parse.parse_ty(String.trim(wty))) {
    | Error(e) => err(e)
    | Ok(sty) =>
      switch (Resolver.resolve_ty(~ns, ~st, ~mint=Some(mint_src), sty)) {
      | Error(e) => err(e)
      | Ok(witness_h) =>
        let m = Mint.fresh(mint_src);
        let opaque = Store.ingest_type(st, Tnode.Opaque({mint: m, witness: witness_h}));
        switch (rebind(name, opaque)) {
        | Error(e) => err(e)
        | Ok(_) =>
          Editing_context.open_type(ctx^, opaque);
          Printf.printf(
            "opaque type %s = opaque(%s) over %s  [opened]\n",
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
      switch (Resolver.resolve_ty(~ns, ~st, ~mint=Some(mint_src), sty)) {
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
    | Some(Definition.Type(_)) => err(name ++ " is not an opaque type")
    | _ => err(name ++ " is not a type")
    }
  };
};

/* last dot-segment of a (possibly dotted) name — the field "leaf" */
let leaf_segment = s =>
  switch (String.rindex_opt(s, '.')) {
  | Some(i) => String.sub(s, i + 1, String.length(s) - i - 1)
  | None => s
  };

/* `:open <expr> as Name [t1, t2, ...] [providing f1, f2, ...]` — generative open
   of an existential package. Peels every leading `exists`, minting one fresh
   abstract type per level (bound `Name.t1`, `Name.t2`, ... — defaulting to the
   tyvar names t, u, s, ...), binds `Name` to the opened value, and binds each
   field: a `providing` name wins; otherwise a record interface's fields are
   named from their labels; an unnamed product field is left unbound. */
let cmd_open_existential = rest =>
  switch (split_first(rest, " as ")) {
  | None =>
    print_endline("usage: :open <expr> as <Name> [t1, t2] [providing f1, f2, ...]")
  | Some((exprs, tail)) =>
    let (head, provided) =
      switch (split_first(String.trim(tail), " providing ")) {
      | Some((h, fs)) => (
          String.trim(h),
          List.map(String.trim, String.split_on_char(',', fs)),
        )
      | None => (String.trim(tail), [])
      };
    let (modname, type_names) =
      switch (split_first(head, "[")) {
      | Some((n, br)) =>
        let names =
          switch (split_first(br, "]")) {
          | Some((inside, _)) =>
            List.filter(
              s => s != "",
              List.map(String.trim, String.split_on_char(',', inside)),
            )
          | None => []
          };
        (String.trim(n), names);
      | None => (head, [])
      };
    switch (Parse.parse_expr(String.trim(exprs))) {
    | Error(e) => err(e)
    | Ok(se) =>
      switch (Resolver.resolve(~ctx=[], ~ns, ~st, se)) {
      | Error(e) => err(e)
      | Ok(node) =>
        switch (Open_existential.open_package(st, mint_src, node)) {
        | Error(e) => err("open: " ++ e)
        | Ok({Open_existential.type_hashes, module_hash, fields}) =>
          let type_name = i =>
            i < List.length(type_names) ? List.nth(type_names, i) : Pretty.tyvar_name(i);
          List.iteri(
            (i, th) => ignore(rebind(modname ++ "." ++ type_name(i), th)),
            type_hashes,
          );
          ignore(rebind(modname, module_hash));
          /* a provided name wins; else a record field's label name (leaf); else
             the field is left unbound (a positional product with no name given) */
          let field_name = (i, label_opt) =>
            if (i < List.length(provided) && List.nth(provided, i) != "") {
              Some(List.nth(provided, i));
            } else {
              switch (label_opt) {
              | Some(lh) =>
                switch (Namespace.name_of(ns, lh)) {
                | Some(nm) => Some(leaf_segment(nm))
                | None => None
                }
              | None => None
              };
            };
          List.iteri(
            (i, (label_opt, fh)) =>
              switch (field_name(i, label_opt)) {
              | Some(nm) => ignore(rebind(modname ++ "." ++ nm, fh))
              | None => ()
              },
            fields,
          );
          let tystr =
            switch (Store.type_of(st, module_hash)) {
            | Some(t) => pty(t)
            | None => "?"
            };
          let tlist =
            String.concat(
              ", ",
              List.mapi((i, _) => modname ++ "." ++ type_name(i), type_hashes),
            );
          Printf.printf("opened %s [%s];  %s : %s\n", modname, tlist, modname, tystr);
        }
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
        switch (Resolver.resolve_ty(~ns, ~st, ~mint=Some(mint_src), sty)) {
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
    let ops = Store.unsealers(st, h);
    Printf.printf("definitions that unseal %s (%d):\n", name, List.length(ops));
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
    | Some(Definition.Label(_)) =>
      Printf.printf("%s : label  %s\n", name, Hash.short(h))
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
    print_endline("context: nothing open (default — opaque types sealed)");
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
        | Some(Definition.Label(_)) => "label"
        | None => "?"
        };
      Printf.printf("  %s  (%s)  %s\n", n, k, Hash.short(h));
    },
    Namespace.entries(ns),
  );

let print_help = () => {
  print_endline("commands:");
  print_endline("  :opaque <name> = <witness>     create an opaque type (mints; opens it)");
  print_endline("  :type <name> = <type>            create a concrete type alias");
  print_endline("  :open <name>                     open an existing opaque type here");
  print_endline("  :close                           reset context (nothing open)");
  print_endline("  :ctx                             show the current open set");
  print_endline("  :let <name> : <type> = <expr>    bind a term (auto-seals if it needs the rep)");
  print_endline("  :impl <name>                     show what unseals an opaque type");
  print_endline("  :show <name>                     show a definition");
  print_endline("  :ls                              list the namespace");
  print_endline("  <expr>                           show an expression's type");
  print_endline("  :quit");
  print_endline("note: put a space after the lambda dot:  \\x: Int. x");
  print_endline("example:");
  print_endline("  :opaque Counter.t = Int");
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
  } else if (starts_with(line, ":opaque ")) {
    cmd_abstract(drop_prefix(line, ":opaque "));
  } else if (starts_with(line, ":type ")) {
    cmd_type(drop_prefix(line, ":type "));
  } else if (starts_with(line, ":open ")) {
    let rest = drop_prefix(line, ":open ");
    switch (split_first(rest, " as ")) {
    | Some(_) => cmd_open_existential(rest) /* :open <expr> as Name */
    | None => cmd_open(rest) /* :open <opaque-type> into the editing context */
    };
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
  print_endline("p16 — type abstraction REPL. :help for commands, :quit to exit.");
  let rec loop = () => {
    Printf.printf("p16> %!");
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
