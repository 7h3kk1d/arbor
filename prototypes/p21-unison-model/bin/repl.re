/* Temporary: the two computations mini-unison.tex is waiting on. */
open P21_substrate;

let nat_plus = Term.Ref(Reference.Builtin("Nat.+"));
let app2 = (f, a, b) => Term.App(Term.App(f, a), b);

let () = {
  print_endline("=== rem:transparency-limit ===");
  /* x = 1 + 1 : Nat */
  let bx = app2(nat_plus, Term.Nat(1), Term.Nat(1));
  let x = Term.Ann(bx, Ty.Nat);
  let (hx, _, _) = Abt.hash_component([("x", x)]);
  let term_hash_x = Abt.hash_closed(x);
  Printf.printf("  term hash  of X   = %s\n", Hash.short(~len=16, term_hash_x));
  Printf.printf("  comphash   of C1  = %s\n", Hash.short(~len=16, hx));
  Printf.printf("  comphash = term hash?  %b\n", Hash.equal(hx, term_hash_x));

  /* y = x : Nat, with x already stored at position 0 of #x */
  let y = Term.Ann(Term.Ref(Reference.Derived(hx, 0)), Ty.Nat);
  let (hy, _, _) = Abt.hash_component([("y", y)]);
  Printf.printf("  comphash   of C2  = %s\n", Hash.short(~len=16, hy));
  Printf.printf("  `x = 1 + 1` and `y = x` hash the same?  %b\n", Hash.equal(hx, hy));

  /* and: does inlining preserve an enclosing term's hash? */
  let enclosing_ref = Term.App(Term.Ref(Reference.Derived(hx, 0)), Term.Nat(0));
  let enclosing_inlined = Term.App(bx, Term.Nat(0));
  Printf.printf(
    "  inlining preserves an enclosing hash?  %b\n",
    Hash.equal(Abt.hash_closed(enclosing_ref), Abt.hash_closed(enclosing_inlined)),
  );

  print_endline("");
  print_endline("=== thm:ordering(b): ping/pong ===");
  let mk = (other) =>
    Term.Ann(
      Term.Lam("n", Term.App(Term.Var(other), Term.Var("n"))),
      Ty.Arrow(Ty.Nat, Ty.Nat),
    );
  /* listed in name order: ping, pong */
  let c_pp = [("ping", mk("pong")), ("pong", mk("ping"))];
  let (h_pp, order_pp, warn_pp) = Abt.hash_component(c_pp);
  /* rename ping -> zing: name order is now pong, zing */
  let c_pz = [("pong", mk("zing")), ("zing", mk("pong"))];
  let (h_pz, order_pz, _) = Abt.hash_component(c_pz);
  Printf.printf("  comphash [ping,pong] = %s\n", Hash.short(~len=16, h_pp));
  Printf.printf("  comphash [pong,zing] = %s\n", Hash.short(~len=16, h_pz));
  Printf.printf("  rename changes comphash?  %b\n", !Hash.equal(h_pp, h_pz));
  Printf.printf("  storage order before: %s\n", String.concat(", ", List.map(fst, order_pp)));
  Printf.printf("  storage order after:  %s\n", String.concat(", ", List.map(fst, order_pz)));
  Printf.printf("  warnings: %d\n", List.length(warn_pp));
  List.iter(w => print_endline("    " ++ Abt.warning_to_string(w)), warn_pp);

  print_endline("");
  print_endline("=== thm:ordering(b2): asymmetric tie (foo/bar) ===");
  /* foo = Nat.+ (foo 0) (bar 0);  bar = Nat.+ (foo 0) (bar 0)
     tied under L(V), but each mentions the cycle asymmetrically in HEnv. */
  let body = (a, b) =>
    Term.Ann(
      app2(nat_plus, Term.App(Term.Var(a), Term.Nat(0)), Term.App(Term.Var(b), Term.Nat(0))),
      Ty.Nat,
    );
  let c_fb = [("bar", body("foo", "bar")), ("foo", body("foo", "bar"))];
  let (h_fb, order_fb, warn_fb) = Abt.hash_component(c_fb);
  /* rename bar -> zar: name order is now foo, zar */
  let c_fz = [("foo", body("foo", "zar")), ("zar", body("foo", "zar"))];
  let (h_fz, order_fz, _) = Abt.hash_component(c_fz);
  Printf.printf("  comphash [bar,foo] = %s\n", Hash.short(~len=16, h_fb));
  Printf.printf("  comphash [foo,zar] = %s\n", Hash.short(~len=16, h_fz));
  Printf.printf("  rename changes comphash?  %b\n", !Hash.equal(h_fb, h_fz));
  Printf.printf("  storage order before: %s\n", String.concat(", ", List.map(fst, order_fb)));
  Printf.printf("  storage order after:  %s\n", String.concat(", ", List.map(fst, order_fz)));
  Printf.printf("  warnings: %d\n", List.length(warn_fb));
};
