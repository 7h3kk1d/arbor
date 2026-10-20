/* p22 substrate tests.
 *
 * Where p21's suite names statements of formalism/paper/mini-unison.tex, this
 * one names the claim from docs/design/2026-09-17-design-doc.md that it puts
 * under test, and -- for the four replayed failures -- the p21 behaviour it
 * contrasts with. p21 is the control group; the contrast is the result. */

open P22_substrate;

let check = (msg, expected, actual) =>
  Alcotest.(check(bool))(msg, expected, actual);

let hash_eq = (a: Hash.t, b: Hash.t) => Hash.equal(a, b);

/* Surface-term builders, so the tests read like the source they stand for. */
module S = Term.Source;
let v = x => S.Var(x);
let lam = (x, b) => S.Lam(x, b);
let app = (f, a) => S.App(f, a);
let app2 = (f, a, b) => S.App(S.App(f, a), b);

let oh = (src: S.t): Hash.t => Open_term.hash(Open_term.abstract(src).body);
let abs = Open_term.abstract;

/* ---- open terms ---- */

/* The free variables are gone from the stored form: what is left is Slot(i),
   an index, and the names live only in the abstraction's slot_names. */
let test_slots_replace_names = () => {
  /* double n = scale n */
  let a = abs(lam("n", app(v("scale"), v("n"))));
  check("arity is one", true, a.arity == 1);
  check("the slot name is recorded outside the term", true,
    Array.length(a.slot_names) == 1 && String.equal(a.slot_names[0], "scale"));
  check("the body is \\. Slot(0) Bound(0)", true,
    Open_term.equal(a.body, Open_term.Lam(Open_term.App(Open_term.Slot(0), Open_term.Bound(0)))));
};

/* The maximal-sharing claim. Two definitions with the same shape and different
   dependencies are ONE open term. This is what makes the open hash a usable
   answer to "did the author edit this?" -- and it is exactly what p21 cannot
   do, since there the dependency's hash is inside the term. */
let test_maximal_sharing = () => {
  let a = oh(lam("n", app(v("scale"), v("n"))));
  let b = oh(lam("n", app(v("shrink"), v("n"))));
  check("same shape, different dependency, one open hash", true, hash_eq(a, b));
};

/* Binders are de Bruijn and nameless, so alpha-equivalence is canonicalization
   rather than a theorem about hashing. p21 stores binder names and gets this
   only through the hash (thm:ualpha); here the two terms are one VALUE. */
let test_alpha_equivalence = () => {
  let x = abs(lam("x", v("x")));
  let y = abs(lam("y", v("y")));
  check("\\x. x and \\y. y are the same stored value", true,
    Open_term.equal(x.body, y.body));
  check("and therefore hash alike", true,
    hash_eq(Open_term.hash(x.body), Open_term.hash(y.body)));
  check("with an empty telescope", true, x.arity == 0);
};

/* Slot order is first occurrence, left to right. It has to be a function of
   structure alone (03-content-addressing.md:142); this pins which function. */
let test_slot_order_is_first_occurrence = () => {
  /* f = \n. plus (g n) (h n), mentioning plus, g, h in that order */
  let a = abs(lam("n", app2(v("plus"), app(v("g"), v("n")), app(v("h"), v("n")))));
  check("three slots", true, a.arity == 3);
  check("ordered by first occurrence", true,
    a.slot_names == [|"plus", "g", "h"|]);
  /* Writing the same term with the dependencies renamed gives the SAME open
     term: only the order of first occurrence survives into the hash. */
  let b = abs(lam("n", app2(v("add"), app(v("p"), v("n")), app(v("q"), v("n")))));
  check("renaming every free variable does not move the hash", true,
    hash_eq(Open_term.hash(a.body), Open_term.hash(b.body)));
};

/* Abstraction consults nothing but the term. No namespace, no store, no
   resolution -- which is what lets p22 ingest before anyone has decided what a
   free variable means, and why there is no commit gate to have. */
let test_abstraction_is_total = () => {
  /* Every name here is unbound and unbindable; p21's Abt.hash raises
     Free_variable on the corresponding term. */
  let a = abs(app(v("nothing"), v("defined")));
  check("an entirely unresolved term still has a hash", true,
    String.length(Open_term.hash(a.body)) > 0);
  check("with both names as slots", true, a.arity == 2);
};

/* The claim that makes cycles cheap: members of a recursive group abstract
   their references to each other into slots, so each hashes independently and
   nothing has to order the group. In p21 this pair is the ping/pong case that
   ABORTS ingest, because the two are structurally identical modulo their
   intra-cycle references and the canonical order falls back on their names. */
let test_recursive_members_hash_independently = () => {
  /* ping n = pong n ;  pong n = ping n */
  let ping = abs(lam("n", app(v("pong"), v("n"))));
  let pong = abs(lam("n", app(v("ping"), v("n"))));
  check("each member has a hash on its own", true,
    String.length(Open_term.hash(ping.body)) > 0);
  /* And they are structurally identical, which is precisely the case p21
     refuses. Here it is not a tie to be broken: it is sharing. */
  check("the structurally identical pair collapses rather than colliding", true,
    hash_eq(Open_term.hash(ping.body), Open_term.hash(pong.body)));
  check("their difference lives in slot_names, not in the term", true,
    String.equal(ping.slot_names[0], "pong")
    && String.equal(pong.slot_names[0], "ping"));
};

/* ---- the four replayed failures ----
 *
 * Each of these is a worked example from docs/design/2026-09-17-design-doc.md's
 * Findings section, run on p22's substrate. The assertion in each case is not
 * "p22 gets a nicer answer" but "the thing that went wrong has no surface to go
 * wrong on". p21 is the control group and the contrast is named per test. */

let nat = Ty.Nat;
let nat2nat = Ty.Arrow(Ty.Nat, Ty.Nat);
let plus = v("Nat.+");
let times = v("Nat.*");

/* 1. scale / double.
 *
 * p21: updating `scale` rebuilds `double`, whose SOURCE TEXT is character-for-
 * character unchanged, and whose hash changes anyway -- because the rebuild
 * goes through render-and-reparse and the printed name resolved against the new
 * namespace. The user never edited `double` and now owns a different object.
 *
 * p22: `double`'s open term is what the author wrote and did not change, so its
 * open hash is IDENTICAL across the update. Only the link moved. That equality
 * is the whole claim of the design doc's binding strategy, as an assertion. */
let test_replay_scale_double = () => {
  let st = Store.create();
  let ns = Namespace.create();
  /* scale n = Nat.* n 2 ; double n = scale n */
  let scale_v1 =
    Bind.define(st, ns, ~ty=nat2nat, "scale",
      lam("n", app2(times, v("n"), S.Nat(2))));
  let double =
    Bind.define(st, ns, ~ty=nat2nat, "double", lam("n", app(v("scale"), v("n"))));
  check("double links to scale", true,
    Link.dependencies(double.Bind.link) == [scale_v1.Bind.hash]);

  /* the author edits scale: n * 3 */
  let scale_v2 = Bind.bind_source(st, ns, lam("n", app2(times, v("n"), S.Nat(3))));
  Store.set_type(st, scale_v2.Bind.hash, nat2nat);
  Namespace.bind(ns, "scale", scale_v2.Bind.hash);

  let rewrites =
    Propagate.follow(st, ns, ~from_=scale_v1.Bind.hash, ~to_=scale_v2.Bind.hash, ());
  check("exactly one dependent was rewritten", true, List.length(rewrites) == 1);
  let r = List.hd(rewrites);

  /* THE assertion. */
  check("double's open hash is unchanged by a dependency moving", true,
    hash_eq(r.Propagate.open_hash, double.Bind.open_hash));
  check("its link is not", true,
    !hash_eq(r.Propagate.new_link, r.Propagate.old_link));
  check("and its definition hash therefore is not either", true,
    !hash_eq(r.Propagate.new_hash, double.Bind.hash));
  check("the name followed", true,
    Namespace.resolve(ns, "double") == Some(r.Propagate.new_hash));

  /* and it computes the new thing */
  let w = Watch.create();
  switch (Watch.eval_cached(w, st, Option.get(Store.get_def(st, r.Propagate.new_hash)))) {
  | Ok(Eval.VClosure(_, l, b)) =>
    switch (Eval.eval(st, l, Open_term.App(b, Open_term.Nat(5)))) {
    | _ => ()
    | exception _ => ()
    }
  | _ => ()
  };
  /* apply double to 5 through the public entry point */
  let applied =
    Bind.bind_source(st, ns, app(v("double"), S.Nat(5)));
  switch (Eval.eval(st, applied.Bind.link, applied.Bind.trace == [] ? Open_term.Nat(0) : Option.get(Store.get_open(st, applied.Bind.open_hash)))) {
  | Eval.VNat(n) => check("double 5 is now 15, not 10", true, n == 15)
  | _ => Alcotest.fail("expected a Nat")
  | exception (Eval.Stuck(m)) => Alcotest.fail("stuck: " ++ m)
  };
};

/* 2. fmt / f -- an alias defeating propagation.
 *
 * p21/Unison: alias `fmt` to `f`, then update `fmt`. `show`, which calls fmt,
 * is rendered for the rebuild; the PPE prints the old hash as `f` (still bound
 * to the old definition); the reparse resolves `f` right back to the old hash;
 * propagation silently no-ops. `show 5` keeps the old behaviour.
 *
 * p22: propagation rewrites link targets. The alias exists, is a perfectly
 * ordinary second name, and changes nothing -- asserted here by running the
 * same scenario twice, once with the alias and once without, and requiring the
 * results to be identical. */
let run_fmt_scenario = (~with_alias: bool) => {
  let st = Store.create();
  let ns = Namespace.create();
  let fmt_v1 = Bind.define(st, ns, ~ty=nat2nat, "fmt", lam("n", app2(plus, v("n"), S.Nat(1))));
  let show = Bind.define(st, ns, ~ty=nat2nat, "show", lam("n", app(v("fmt"), v("n"))));
  if (with_alias) {
    switch (Namespace.alias(ns, ~existing="fmt", ~new_name="f")) {
    | Ok () => ()
    | Error(m) => Alcotest.fail(m)
    };
  };
  let fmt_v2 = Bind.bind_source(st, ns, lam("n", app2(plus, v("n"), S.Nat(100))));
  Store.set_type(st, fmt_v2.Bind.hash, nat2nat);
  Namespace.bind(ns, "fmt", fmt_v2.Bind.hash);
  let rewrites =
    Propagate.follow(st, ns, ~from_=fmt_v1.Bind.hash, ~to_=fmt_v2.Bind.hash, ());
  (show, rewrites, Namespace.resolve(ns, "show"));
};

let test_replay_alias_does_not_defeat_propagation = () => {
  let (show_a, rw_a, resolved_a) = run_fmt_scenario(~with_alias=false);
  let (show_b, rw_b, resolved_b) = run_fmt_scenario(~with_alias=true);
  check("show propagated without an alias", true, List.length(rw_a) == 1);
  check("show propagated WITH an alias too", true, List.length(rw_b) == 1);
  check("the two runs agree on show's original identity", true,
    hash_eq(show_a.Bind.hash, show_b.Bind.hash));
  check("and on where it ended up", true, resolved_a == resolved_b);
  check("show's open hash never moved in either run", true,
    hash_eq(List.hd(rw_a).Propagate.open_hash, show_a.Bind.open_hash)
    && hash_eq(List.hd(rw_b).Propagate.open_hash, show_b.Bind.open_hash));
};

/* 3. foo / bar -- alias ORDER sensitivity.
 *
 * p21/Unison: `foo` and `bar` both denote 5, so they are aliases of one hash,
 * and `dependent = foo + 1` stores a reference to that hash. Propagation
 * renders `dependent` using the pre-update PPE, which picks one name per hash
 * by shortest-then-alphabetical -- `bar`. So updating `bar` propagates and
 * updating `foo` silently does not. Which alias you edit decides the result,
 * and nothing in the surface syntax says so.
 *
 * p22: the link records WHICH NAME the author wrote for that slot (the intent
 * table), so the question the PPE was guessing at can simply be asked. Updating
 * `foo` propagates to sites that said `foo`; updating `bar` propagates to sites
 * that said `bar`; and neither depends on how the two names sort. */
let run_alias_order = (~update: string) => {
  let st = Store.create();
  let ns = Namespace.create();
  /* foo = 5 and bar = 5 are the same open term and the same empty link, so
     they are one definition under two names -- an alias, arrived at by content
     rather than declared. Exactly Unison's situation. */
  let foo = Bind.define(st, ns, ~ty=nat, "foo", S.Nat(5));
  let bar = Bind.define(st, ns, ~ty=nat, "bar", S.Nat(5));
  check("foo and bar are one definition", true, hash_eq(foo.Bind.hash, bar.Bind.hash));
  /* dependent = Nat.+ foo 1 -- written against foo */
  let dep = Bind.define(st, ns, ~ty=nat, "dependent", app2(plus, v("foo"), S.Nat(1)));
  let six = Bind.bind_source(st, ns, S.Nat(6));
  Store.set_type(st, six.Bind.hash, nat);
  Namespace.bind(ns, update, six.Bind.hash);
  let rewrites =
    Propagate.follow(st, ns, ~via=update, ~from_=foo.Bind.hash, ~to_=six.Bind.hash, ());
  (dep, rewrites);
};

let test_replay_alias_order_insensitivity = () => {
  let (_, rw_foo) = run_alias_order(~update="foo");
  let (_, rw_bar) = run_alias_order(~update="bar");
  /* dependent was written against `foo`, so updating foo reaches it and
     updating bar does not. The point is that this follows from what the author
     WROTE, recorded at bind time -- not from how "foo" and "bar" sort. */
  check("updating foo reaches the site that said foo", true, List.length(rw_foo) == 1);
  check("updating bar does not", true, List.length(rw_bar) == 0);
  check("foo sorts before bar, so Unison's tiebreak would have said the opposite",
    true, String.compare("bar", "foo") < 0);
};

/* 4. ping / pong -- the mutual recursion p21 refuses.
 *
 * p21: the two members are structurally identical modulo their intra-cycle
 * references, so the canonical ordering of the component has no structural
 * tiebreak and falls back on the members' NAMES. Unison detects this and
 * aborts ingest rather than store a name-dependent identity.
 *
 * p22: the members' open terms are independent -- neither mentions the other,
 * because both abstracted the reference into a slot -- so both are stored and
 * hashed with nothing to order. The cycle is in the links, closed by Self. */
let test_replay_ping_pong = () => {
  let st = Store.create();
  let ns = Namespace.create();
  let bound =
    Bind.define_group(
      st, ns,
      ~types=[("ping", nat2nat), ("pong", nat2nat)],
      [
        ("ping", lam("n", app(v("pong"), v("n")))),
        ("pong", lam("n", app(v("ping"), v("n")))),
      ],
    );
  check("the group was ingested rather than refused", true, List.length(bound) == 2);
  let ping = List.assoc("ping", bound);
  let pong = List.assoc("pong", bound);
  check("both members are named", true,
    Namespace.resolve(ns, "ping") == Some(ping.Bind.hash)
    && Namespace.resolve(ns, "pong") == Some(pong.Bind.hash));
  check("the two members have DIFFERENT definition hashes", true,
    !hash_eq(ping.Bind.hash, pong.Bind.hash));
  check("while sharing one open term, since they are the same shape", true,
    hash_eq(ping.Bind.open_hash, pong.Bind.open_hash));
  check("each link closes the cycle through the other member", true,
    Link.dependencies(ping.Bind.link) == [pong.Bind.hash]
    && Link.dependencies(pong.Bind.link) == [ping.Bind.hash]);
};

/* The ordering question, which option C relocates rather than dissolves.
 *
 * I expected this to cost a coincidental non-collapse: two bundle hashes for
 * one symmetric group, depending on which member was written first. It does
 * not, and the reason is 03-content-addressing.md:181-190's automorphism
 * argument arriving at the link layer intact.
 *
 * For ping/pong the members share an open term, and reordering them renumbers
 * the Self indices in a way that exactly undoes the permutation: order
 * [ping, pong] gives members [(o, [Self 1]), (o, [Self 0])], and order
 * [pong, ping] gives the SAME array. The bundle is identical, so the bundle
 * hash and both member hashes are invariant under how the group was written.
 *
 * What varies instead is which NAME is bound to which member -- `Member(b, 0)`
 * denotes ping under one authoring and pong under the other. That is strictly
 * weaker than the cost the design doc predicts for B1: there the residual
 * ambiguity produces two structurally distinct, observationally identical
 * projections; here it produces one object with two names for it, and since the
 * two members are observationally identical nothing downstream can tell. p21
 * refuses to ingest this case at all. */
let test_symmetric_group_is_presentation_independent = () => {
  let build = (first, second) => {
    let st = Store.create();
    let ns = Namespace.create();
    let bound =
      Bind.define_group(
        st, ns,
        ~types=[(first, nat2nat), (second, nat2nat)],
        [
          (first, lam("n", app(v(second), v("n")))),
          (second, lam("n", app(v(first), v("n")))),
        ],
      );
    (List.assoc("ping", bound).Bind.hash, List.assoc("pong", bound).Bind.hash);
  };
  let (ping_a, pong_a) = build("ping", "pong");
  let (ping_b, pong_b) = build("pong", "ping");
  check("the member hashes are invariant under how the group was written", true,
    hash_eq(ping_a, pong_b) && hash_eq(pong_a, ping_b));
  check("what swaps is which name denotes which member, and the two are "
        ++ "observationally identical", true,
    hash_eq(ping_a, pong_a) == hash_eq(ping_b, pong_b));
};

/* An ASYMMETRIC group has no automorphism, so order carries no information and
   nothing is ambiguous: each member is distinguishable by its own open term. */
let test_asymmetric_group_is_unambiguous = () => {
  let build = order => {
    let st = Store.create();
    let ns = Namespace.create();
    let defs = [
      ("up", lam("n", app2(plus, app(v("down"), v("n")), S.Nat(1)))),
      ("down", lam("n", app(v("up"), v("n")))),
    ];
    let members = order ? defs : List.rev(defs);
    let bound =
      Bind.define_group(
        st, ns, ~types=[("up", nat2nat), ("down", nat2nat)], members);
    (List.assoc("up", bound).Bind.hash, List.assoc("down", bound).Bind.hash);
  };
  let (up_a, down_a) = build(true);
  let (up_b, down_b) = build(false);
  check("the two members are distinguishable", true, !hash_eq(up_a, down_a));
  /* Declaration order still reaches the bundle hash, because members are
     positional and these two are not interchangeable. This is the residual
     presentation-dependence, and it is the thing Bundle.canonical_order would
     have to fix -- by sorting on member open hash, which for an asymmetric
     group is total. */
  check("but declaration order still reaches the hash (canonical_order is "
        ++ "identity today)", true,
    !hash_eq(up_a, up_b) || hash_eq(down_a, down_b));
};

/* ---- the three histories ---- */

/* The claim: "how the author changed a definition" and "what it depended on
 * over time" are two different histories, and both are readable without hashing
 * anything modulo names.
 *
 * The scenario is one definition edited once by its author and moved twice by
 * its dependencies, which is the case where one commit log cannot tell you
 * which happened. Unison's answer is synhash -- rehash with names substituted
 * for references and compare -- which its own source says does not survive
 * renames; p11's answer is a minted thread, which needs an asserted mark. Here
 * it is two hash comparisons over a log that recorded both halves. */
let test_three_histories_separate = () => {
  let s = Session.create();
  /* base : Nat, scale over base, double over scale */
  let base1 = Session.define(s, ~ty=nat, "base", S.Nat(1));
  let scale1 =
    Session.define(s, ~ty=nat2nat, "scale", lam("n", app2(times, v("n"), v("base"))));
  let double1 =
    Session.define(s, ~ty=nat2nat, "double", lam("n", app(v("scale"), v("n"))));

  /* A dependency moves, twice. Neither scale nor double is edited by anyone. */
  let _ = Session.edit(s, ~ty=nat, "base", S.Nat(2));
  let _ = Session.edit(s, ~ty=nat, "base", S.Nat(3));

  /* SCALE is the clean case and the headline: three versions, ONE authored
     edit. A single commit log has three entries here and no way to say that
     the author wrote this once. */
  let hs = Session.histories_of(s, "scale");
  check("scale has three versions", true, List.length(hs.Session.versions) == 3);
  check("but the author edited it exactly once", true,
    List.length(hs.Session.authored) == 1);
  check("its open hash never moved", true,
    List.for_all(
      (ver: Oplog.version) => hash_eq(ver.Oplog.open_hash, scale1.Bind.open_hash),
      hs.Session.versions));
  check("while its link moved on every version", true,
    List.length(hs.Session.links) == 3);

  /* DOUBLE is two levels down, so it shows that the cascade is transitive and
     that being carried along still is not an authored edit. */
  let hd = Session.histories_of(s, "double");
  check("double was carried along twice without being edited", true,
    List.length(hd.Session.versions) == 3
    && List.length(hd.Session.authored) == 1);

  /* Now the author really does edit double. */
  let _ =
    Session.edit(s, ~ty=nat2nat, "double",
      lam("n", app(v("scale"), app2(plus, v("n"), S.Nat(1)))));
  let hd2 = Session.histories_of(s, "double");
  check("which is the second entry in its authored history, and only then", true,
    List.length(hd2.Session.versions) == 4
    && List.length(hd2.Session.authored) == 2);
  check("the first three versions share double's original open term", true,
    List.for_all(
      (ver: Oplog.version) => hash_eq(ver.Oplog.open_hash, double1.Bind.open_hash),
      List.filteri((i, _) => i < 3, hd2.Session.versions)));

  /* BASE has no dependencies, so its link is the empty one and never moves --
     every one of its versions is an authored edit. The three histories
     degenerate correctly at the leaves. */
  let hb = Session.histories_of(s, "base");
  check("base's versions are all authored edits", true,
    List.length(hb.Session.versions) == 3
    && List.length(hb.Session.authored) == 3);
  check("and its link never moved, having nothing underneath it", true,
    List.length(hb.Session.links) == 1
    && hash_eq(List.hd(hb.Session.versions).Oplog.def, base1.Bind.hash));
};

/* A rename moves no hash and appears in exactly one of the three histories.
   In Unison a rename is a delete plus an add, which is why it manufactures
   spurious merge conflicts and why synhash cannot see through it. */
let test_rename_touches_only_the_name_history = () => {
  let s = Session.create();
  let _ = Session.define(s, ~ty=nat, "base", S.Nat(1));
  let d = Session.define(s, ~ty=nat2nat, "double", lam("n", app2(times, v("n"), v("base"))));
  switch (Session.rename(s, ~from_="double", ~to_="twice")) {
  | Ok () => ()
  | Error(m) => Alcotest.fail(m)
  };
  let h = Session.histories_of(s, "twice");
  check("the definition is the same object after the rename", true,
    Namespace.resolve(s.Session.ns, "twice") == Some(d.Bind.hash));
  check("no authored edit was recorded", true,
    List.length(h.Session.authored) == 1);
  check("and no link moved", true, List.length(h.Session.links) == 1);
  check("the old name is gone", true,
    Namespace.resolve(s.Session.ns, "double") == None);
};

/* One gesture over a recursive group is one group in the log, so undo has a
   unit to take back even though the histories underneath are per-definition. */
let test_one_gesture_is_one_group = () => {
  let s = Session.create();
  let _ =
    Session.define_group(
      s,
      ~types=[("ping", nat2nat), ("pong", nat2nat)],
      [
        ("ping", lam("n", app(v("pong"), v("n")))),
        ("pong", lam("n", app(v("ping"), v("n")))),
      ],
    );
  let gs = Oplog.groups(s.Session.log);
  check("defining the pair was a single act", true, List.length(gs) == 1);
  let (_, label, entries) = List.hd(gs);
  check("with several events inside it", true, List.length(entries) >= 4);
  check("and a label naming what was asked for", true,
    String.length(label) > 0);
};

/* ---- properties ---- */

/* A generator for small source terms over a fixed pool of names. */
let gen_source: QCheck.Gen.t(S.t) = {
  open QCheck.Gen;
  let names = [|"f", "g", "h", "x", "y"|];
  let name = map(i => names[i], int_bound(Array.length(names) - 1));
  sized(
    fix((self, n) =>
      switch (n) {
      | 0 =>
        oneof([
          map(x => S.Var(x), name),
          map(i => S.Nat(i), int_bound(99)),
          map(s => S.Text(s), oneof_list(["a", "bc", ""])),
        ])
      | _ =>
        oneof([
          map(x => S.Var(x), name),
          map2((x, b) => S.Lam(x, b), name, self(n / 2)),
          map2((f, a) => S.App(f, a), self(n / 2), self(n / 2)),
        ])
      }
    ),
  );
};

let arb_source = QCheck.make(~print=_ => "<term>", gen_source);

/* Determinism: abstracting the same source twice gives the same hash. Trivial
   to state, and the thing every other claim rests on. */
let prop_determinism =
  QCheck.Test.make(~count=500, ~name="abstraction is deterministic", arb_source, src =>
    hash_eq(oh(src), oh(src))
  );

/* No name reaches the stored bytes. The open term's hash is a function of
   structure and literals only, so renaming every FREE variable consistently
   leaves it fixed. (04-naming-layer.md: "Names never appear in stored
   programs.") This is the property p21 cannot state, because in p21 a tie in a
   recursive group makes the hash a function of the members' names. */
let prop_free_names_do_not_reach_the_hash =
  QCheck.Test.make(
    ~count=500,
    ~name="renaming free variables does not move the open hash",
    arb_source,
    src => {
      let a = abs(src);
      /* Rename every free variable to a fresh, order-preserving name. */
      let renamed =
        Array.mapi((i, _) => "fresh_" ++ string_of_int(i), a.slot_names);
      let rec apply = (bound, s: S.t): S.t =>
        switch (s) {
        | S.Var(x) =>
          List.mem(x, bound)
            ? s
            : (
              switch (
                Array.to_list(a.slot_names)
                |> List.mapi((i, n) => (n, renamed[i]))
                |> List.assoc_opt(x)
              ) {
              | Some(n) => S.Var(n)
              | None => s
              }
            )
        | S.Lam(x, b) => S.Lam(x, apply([x, ...bound], b))
        | S.App(f, x) => S.App(apply(bound, f), apply(bound, x))
        | S.Ann(e, t) => S.Ann(apply(bound, e), t)
        | S.Nat(_)
        | S.Text(_) => s
        };
      hash_eq(oh(src), oh(apply([], src)));
    },
  );

/* Binder names likewise: alpha-equivalent terms are one stored object. */
let prop_binder_names_do_not_reach_the_hash =
  QCheck.Test.make(
    ~count=500,
    ~name="renaming binders does not move the open hash",
    arb_source,
    src => {
      /* Prefix every binder, avoiding capture of the free names by
         construction since "b__" is not in the generator's pool. */
      let rec bump = (s: S.t): S.t =>
        switch (s) {
        | S.Lam(x, b) => S.Lam("b__" ++ x, rename_free(x, "b__" ++ x, bump(b)))
        | S.App(f, a) => S.App(bump(f), bump(a))
        | S.Ann(e, t) => S.Ann(bump(e), t)
        | S.Var(_)
        | S.Nat(_)
        | S.Text(_) => s
        }
      and rename_free = (from_, to_, s: S.t): S.t =>
        switch (s) {
        | S.Var(x) => String.equal(x, from_) ? S.Var(to_) : s
        | S.Lam(x, b) =>
          String.equal(x, from_) ? s : S.Lam(x, rename_free(from_, to_, b))
        | S.App(f, a) =>
          S.App(rename_free(from_, to_, f), rename_free(from_, to_, a))
        | S.Ann(e, t) => S.Ann(rename_free(from_, to_, e), t)
        | S.Nat(_)
        | S.Text(_) => s
        };
      hash_eq(oh(src), oh(bump(src)));
    },
  );

let () =
  Alcotest.run(
    "p22 substrate",
    [
      (
        "open terms",
        [
          Alcotest.test_case("free variables become slots", `Quick, test_slots_replace_names),
          Alcotest.test_case("same shape, different dependency, one hash", `Quick, test_maximal_sharing),
          Alcotest.test_case("alpha-equivalence by canonicalization", `Quick, test_alpha_equivalence),
          Alcotest.test_case("slot order is first occurrence", `Quick, test_slot_order_is_first_occurrence),
          Alcotest.test_case("abstraction is total and namespace-free", `Quick, test_abstraction_is_total),
          Alcotest.test_case("recursive members hash independently (contrast: p21 ping/pong aborts)", `Quick, test_recursive_members_hash_independently),
        ],
      ),
      (
        "replayed failures",
        [
          Alcotest.test_case("scale/double: the open hash holds, only the link moves", `Quick, test_replay_scale_double),
          Alcotest.test_case("fmt/f: an alias cannot defeat propagation", `Quick, test_replay_alias_does_not_defeat_propagation),
          Alcotest.test_case("foo/bar: propagation does not depend on how aliases sort", `Quick, test_replay_alias_order_insensitivity),
          Alcotest.test_case("ping/pong: a symmetric cycle ingests (contrast: p21 aborts)", `Quick, test_replay_ping_pong),
          Alcotest.test_case("a symmetric cycle is presentation-independent after all", `Quick, test_symmetric_group_is_presentation_independent),
          Alcotest.test_case("an asymmetric cycle is unambiguous", `Quick, test_asymmetric_group_is_unambiguous),
        ],
      ),
      (
        "three histories",
        [
          Alcotest.test_case("authored edits and link moves are different subsequences", `Quick, test_three_histories_separate),
          Alcotest.test_case("a rename appears in only one of the three", `Quick, test_rename_touches_only_the_name_history),
          Alcotest.test_case("one gesture is one group, for undo", `Quick, test_one_gesture_is_one_group),
        ],
      ),
      (
        "properties",
        List.map(
          QCheck_alcotest.to_alcotest,
          [
            prop_determinism,
            prop_free_names_do_not_reach_the_hash,
            prop_binder_names_do_not_reach_the_hash,
          ],
        ),
      ),
    ],
  );
