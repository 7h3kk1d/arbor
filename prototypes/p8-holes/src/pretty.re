/* Rendering from stored hashes back to readable λ-calculus syntax.

   Two printers live here:

   - A raw printer (print / print_hash / print_node) that shows the
     stored structure with de Bruijn indices literal. Binders render as
     `\.` (no name), variables render as numbers (`0`, `1`, …). Useful
     for debugging the Store.

   - A name-aware printer (print_named / surface_of_hash) that assigns
     fresh display names to each λ-binder it crosses and produces a
     Surface_ast.t. Child subterms whose hash has a namespace binding
     collapse to a Var(name) leaf (p3 behavior); the top-level hash is
     never collapsed. The renderer is a pure function of the Store, the
     Namespace, and the hash — the same inputs always produce the same
     output. */

/* ===== Raw (de Bruijn literal) printer ===== */

let rec print_node = (store: Store.t, node: Node.t): string =>
  switch (node) {
  | Node.Var(k) => string_of_int(k)
  | Node.Hole => "?"
  | Node.Lam(h) => "\\. " ++ print_hash(store, h)
  | Node.App(f, a) => print_app_left(store, f) ++ " " ++ print_atom(store, a)
  }
and print_hash = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(n) => print_node(store, n)
  }
and print_atom = (store: Store.t, h: Hash.t): string =>
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Node.Var(k)) => string_of_int(k)
  | Some(Node.Hole) => "?"
  | Some(n) => "(" ++ print_node(store, n) ++ ")"
  }
and print_app_left = (store: Store.t, h: Hash.t): string =>
  /* Application is left-associative; the left side of an App renders
     without parens unless its head is a Lam. */
  switch (Store.lookup(store, h)) {
  | None => "<missing " ++ Hash.short(h) ++ ">"
  | Some(Node.Var(k)) => string_of_int(k)
  | Some(Node.Hole) => "?"
  | Some(Node.App(_, _) as n) => print_node(store, n)
  | Some(Node.Lam(_) as n) => "(" ++ print_node(store, n) ++ ")"
  };

let print = (store: Store.t, h: Hash.t): string => print_hash(store, h);

let print_shallow = (node: Node.t): string =>
  switch (node) {
  | Node.Var(k) => "Var " ++ string_of_int(k)
  | Node.Hole => "Hole"
  | Node.Lam(h) => "Lam " ++ Hash.short(h)
  | Node.App(f, a) => "App " ++ Hash.short(f) ++ " " ++ Hash.short(a)
  };

/* ===== Fresh-name generation ===== */

/* Preferred single-letter alphabet. First x/y/z (conventional for
   λ-calculus), then a..w. Beyond 26, append a counter: x1, y1, …, x2,
   … Deterministic given the in-scope set. */
let alphabet = [
  "x",
  "y",
  "z",
  "a",
  "b",
  "c",
  "d",
  "e",
  "f",
  "g",
  "h",
  "i",
  "j",
  "k",
  "l",
  "m",
  "n",
  "o",
  "p",
  "q",
  "r",
  "s",
  "t",
  "u",
  "v",
  "w",
];

let fresh_name = (~in_scope: list(string)): string => {
  let taken = s => List.exists(n => n == s, in_scope);
  let rec try_round = (round: int) => {
    let candidates =
      List.map(
        base => round == 0 ? base : base ++ string_of_int(round),
        alphabet,
      );
    switch (List.find_opt(c => !taken(c), candidates)) {
    | Some(n) => n
    | None => try_round(round + 1)
    };
  };
  try_round(0);
};

/* ===== Closedness ===== */

/* A stored hash is closed iff the term it reconstructs to is closed
   (has no free de Bruijn indices). Used by the name-aware printer to
   decide whether a namespace-bound child is legible as a name or
   should be expanded inline. */
let is_closed_hash = (store: Store.t, h: Hash.t): bool =>
  switch (Store.reconstruct(store, h)) {
  | None => false
  | Some(ast) => Ast.is_closed(ast)
  };

/* ===== Name-aware printer ===== */

/* Walks a stored hash into a Surface_ast.t.

   - `in_scope`: list of names assigned to enclosing binders, most
     recent first. A Var(k) resolves to the k-th entry; a free index
     (k >= length) renders as a synthetic "$k" token.
   - `top`: true for the outermost call — the top-level hash is never
     collapsed to a namespace name.
   - `namespace`: used to collapse a non-Var child hash to a Var(name)
     leaf when it has a binding AND the subterm is closed. Var children
     are never collapsed (a bound variable's meaning is context-
     dependent). Open subterms are never collapsed either, even if
     they happen to be named — the name would misleadingly suggest a
     self-contained definition. */

let rec surface_of_hash_ctx =
        (
          ~namespace: Namespace.t,
          ~store: Store.t,
          ~in_scope: list(string),
          ~top: bool,
          h: Hash.t,
        )
        : Surface_ast.t => {
  let named = Namespace.names_of(namespace, h);
  let is_collapsible =
    switch (named, Store.lookup(store, h)) {
    | ([_, ..._], Some(Node.Lam(_) | Node.App(_, _))) =>
      is_closed_hash(store, h)
    | _ => false
    };
  if (!top && is_collapsible) {
    Surface_ast.Var(List.hd(named));
  } else {
    switch (Store.lookup(store, h)) {
    | None => Surface_ast.Var("<missing " ++ Hash.short(h) ++ ">")
    | Some(Node.Var(k)) =>
      if (k < List.length(in_scope)) {
        Surface_ast.Var(List.nth(in_scope, k));
      } else {
        Surface_ast.Var("$" ++ string_of_int(k));
      }
    | Some(Node.Hole) => Surface_ast.Hole
    | Some(Node.Lam(body_hash)) =>
      let x = fresh_name(~in_scope);
      let body =
        surface_of_hash_ctx(
          ~namespace,
          ~store,
          ~in_scope=[x, ...in_scope],
          ~top=false,
          body_hash,
        );
      Surface_ast.Lam(x, body);
    | Some(Node.App(f, a)) =>
      Surface_ast.App(
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, f),
        surface_of_hash_ctx(~namespace, ~store, ~in_scope, ~top=false, a),
      )
    };
  };
};

let surface_of_hash =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): Surface_ast.t =>
  surface_of_hash_ctx(~namespace, ~store, ~in_scope=[], ~top=true, h);

/* Surface printer. Convention:
   - Lam bodies extend as far right as possible: `\x. f x` not `(\x. f) x`.
   - Application is left-associative; its left side may be a Var, an
     App, or a parenthesized Lam.
   - Atoms (Var, paren-wrapped) need no surrounding parens. */

let rec print_surface = (s: Surface_ast.t): string =>
  switch (s) {
  | Surface_ast.Var(n) => n
  | Surface_ast.Hole => "?"
  | Surface_ast.Lam(x, body) => "\\" ++ x ++ ". " ++ print_surface(body)
  | Surface_ast.App(f, a) =>
    print_surface_app_left(f) ++ " " ++ print_surface_atom(a)
  }
and print_surface_app_left = (s: Surface_ast.t): string =>
  switch (s) {
  | Surface_ast.Var(n) => n
  | Surface_ast.Hole => "?"
  | Surface_ast.App(_, _) => print_surface(s)
  | Surface_ast.Lam(_, _) => "(" ++ print_surface(s) ++ ")"
  }
and print_surface_atom = (s: Surface_ast.t): string =>
  switch (s) {
  | Surface_ast.Var(n) => n
  | Surface_ast.Hole => "?"
  | _ => "(" ++ print_surface(s) ++ ")"
  };

let print_named =
    (~namespace: Namespace.t, store: Store.t, h: Hash.t): string =>
  print_surface(surface_of_hash(~namespace, store, h));
