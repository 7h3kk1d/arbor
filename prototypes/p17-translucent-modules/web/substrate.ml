(* Module-level singleton: the one Store + Namespace the whole app shares, plus
   the mint source for new abstract types. The editing context's open set is UI
   state and lives in the Bonsai model, not here. Bootstrap seeds the Counter
   worked example so the app is demonstrable on load. *)

open P17_substrate

type t = {
  store : Store.t;
  ns : Namespace.t;
  att : Attachment.t;
  mint_src : Mint.source;
}

(* A curated tour of the substrate, one clean example per feature (see
   WALKTHROUGH.md). Reading order in the namespace:
     Counter           — abstract type, editor-enforced opacity, minimal sealing
     Celsius/Kelvin/Temp — two distinct abstract types + ops that span both
     Tally + step      — System-F: one polymorphic functor over two reps
     CounterSig / counter.impl / mk_counter / Box — translucent modules:
                         sig + struct + ascription (:>) + n-ary open
     CalSig / mk_calendar / Cal — ONE sig hiding TWO types (date, span)
     VecSig / Vec      — translucency: a manifest member stays transparent
     total             — local open: `open m as c in ...` INSIDE a function
     Geom.Point        — records as plain data + #-projection
     demo.nums         — lists + fold
   Tests for each feature carry the `test` aspect and show in the tally. *)
let seed ~store ~ns ~att ~mint_src =
  let int_h = Store.int_type store in
  let bool_h = Store.bool_type store in
  let arrow a b = Store.ingest_type store (Tnode.Arrow (a, b)) in
  let prod a b = Store.ingest_type store (Tnode.Product (a, b)) in
  let abstract name witness =
    let h = Store.ingest_type store (Tnode.Opaque { mint = Mint.fresh mint_src; witness }) in
    (try Namespace.rebind ns ~name h with _ -> ());
    h
  in
  (* commit [term : ann] with [opens] transparent, bind it, return its hash *)
  let bind name opens term ann =
    let ctx = Editing_context.make () in
    List.iter (fun o -> Editing_context.open_type ctx o) opens;
    match Editing_context.commit store ctx ~term ~ann with
    | Ok (h, _) -> (try Namespace.rebind ns ~name h with _ -> ()); Some h
    | Error _ -> None
  in
  let var n = Node.Var n in
  let lit n = Node.Lit n in
  let app f x = Node.App (f, x) in
  (* parse + resolve + commit a definition from surface strings — used for the
     System-F functor below, where building nodes by hand is unwieldy *)
  let define_str name opens ty_s expr_s =
    match Parse.parse_ty ty_s, Parse.parse_expr expr_s with
    | Ok sty, Ok se -> (
        (* resolve the annotation FIRST (with minting) so a record interface mints
           its field labels before the body's record literal resolves them *)
        match Resolver.resolve_ty ~ns ~st:store ~mint:(Some mint_src) sty with
        | Error _ -> None
        | Ok ann -> (
            match Resolver.resolve ~ctx:[] ~mint:(Some mint_src) ~ns ~st:store se with
            | Error _ -> None
            | Ok node ->
                let ctx = Editing_context.make () in
                List.iter (fun o -> Editing_context.open_type ctx o) opens;
                (match Editing_context.commit store ctx ~term:node ~ann with
                 | Ok (h, _) -> (try Namespace.rebind ns ~name h with _ -> ()); Some h
                 | Error _ -> None)))
    | _ -> None
  in
  (* ---- Counter, over Int ---- *)
  let counter = abstract "Counter.t" int_h in
  ignore (bind "Counter.empty" [ counter ] (lit 0) counter);
  let incr =
    bind "Counter.incr" [ counter ]
      (Node.Lam (counter, Node.Prim (Node.Add, [ var 0; lit 1 ]))) (arrow counter counter)
  in
  let get = bind "Counter.get" [ counter ] (Node.Lam (counter, var 0)) (arrow counter int_h) in
  ignore
    (bind "Counter.decr" [ counter ]
       (Node.Lam (counter, Node.Prim (Node.Sub, [ var 0; lit 1 ]))) (arrow counter counter));
  ignore get;
  (match incr with
   | Some incr ->
       (* a consumer composed at the abstract level — stays an ordinary
          (unsealed) term, since it never touches the representation *)
       ignore
         (bind "bump2" []
            (Node.Lam (counter, app (Node.Ref incr) (app (Node.Ref incr) (var 0))))
            (arrow counter counter))
   | _ -> ());
  (* ---- Temperature: Celsius + Kelvin; conversions open BOTH ---- *)
  let celsius = abstract "Celsius.t" int_h in
  let kelvin = abstract "Kelvin.t" int_h in
  ignore (bind "Celsius.freezing" [ celsius ] (lit 0) celsius);
  ignore (bind "Kelvin.value" [ kelvin ] (Node.Lam (kelvin, var 0)) (arrow kelvin int_h));
  let c2k =
    bind "Temp.c_to_k" [ celsius; kelvin ]
      (Node.Lam (celsius, Node.Prim (Node.Add, [ var 0; lit 273 ]))) (arrow celsius kelvin)
  in
  let k2c =
    bind "Temp.k_to_c" [ kelvin; celsius ]
      (Node.Lam (kelvin, Node.Prim (Node.Sub, [ var 0; lit 273 ]))) (arrow kelvin celsius)
  in
  (match c2k, k2c with
   | Some c2k, Some k2c ->
       ignore
         (bind "Temp.round_trip" []
            (Node.Lam (celsius, app (Node.Ref k2c) (app (Node.Ref c2k) (var 0))))
            (arrow celsius celsius))
   | _ -> ());
  (* ---- tests: boolean expressions, marked with the `test` aspect ---- *)
  let r name =
    match Namespace.resolve ns name with Some h -> Node.Ref h | None -> lit 0
  in
  let eqn a b = Node.Prim (Node.Eq, [ a; b ]) in
  let test name opens term =
    match bind name opens term bool_h with
    | Some h -> Attachment.mark att ~aspect:"test" h
    | None -> ()
  in
  (* Public tests live in <Module>.Tests and use only the abstract API (open
     nothing). *)
  test "Counter.Tests.empty" [] (eqn (app (r "Counter.get") (r "Counter.empty")) (lit 0));
  test "Counter.Tests.bump2" []
    (eqn (app (r "Counter.get") (app (r "bump2") (r "Counter.empty"))) (lit 2));
  test "Counter.Tests.oops" [] (eqn (app (r "Counter.get") (r "Counter.empty")) (lit 1));
  test "Temp.Tests.c_to_k" []
    (eqn (app (r "Kelvin.value") (app (r "Temp.c_to_k") (r "Celsius.freezing"))) (lit 273));
  (* Internal tests live in <Module>.Tests.Internal and unseal the type — they
     compare an abstract value directly to its representation, so they open
     Counter.t (and seal accordingly). *)
  test "Counter.Tests.Internal.empty_is_zero" [ counter ] (eqn (r "Counter.empty") (lit 0));
  test "Counter.Tests.Internal.incr_is_one" [ counter ]
    (eqn (app (r "Counter.incr") (r "Counter.empty")) (lit 1));
  (* ---- a second counter over a Product representation ---- *)
  let tally = abstract "Tally.t" (prod int_h int_h) in
  ignore (define_str "Tally.start" [ tally ] "Tally.t" "(0, 0)");
  ignore (define_str "Tally.incr" [ tally ] "Tally.t -> Tally.t" "\\x: Tally.t. (fst x + 1, snd x)");
  ignore (define_str "Tally.decr" [ tally ] "Tally.t -> Tally.t" "\\x: Tally.t. (fst x - 1, snd x)");
  ignore (define_str "Tally.get" [ tally ] "Tally.t -> Int" "\\x: Tally.t. fst x");
  (* ---- the functor: one polymorphic `step`, applied to either counter ---- *)
  ignore
    (define_str "step" []
       "forall t. (t -> t) * (t -> t) -> t -> Bool -> t"
       "/\\t. \\ops: (t -> t) * (t -> t). \\x: t. \\b: Bool. if b then (fst ops) x else (snd ops) x");
  (* functor tests — the SAME step applied to two different representations *)
  let test_str name expr_s =
    match define_str name [] "Bool" expr_s with
    | Some h -> Attachment.mark att ~aspect:"test" h
    | None -> ()
  in
  test_str "Functor.Tests.step_up_counter"
    "Counter.get (step [Counter.t] (Counter.incr, Counter.decr) Counter.empty true) == 1";
  test_str "Functor.Tests.step_down_tally"
    "Tally.get (step [Tally.t] (Tally.incr, Tally.decr) Tally.start false) == 0 - 1";
  (* ---- p17: translucent modules — sig / struct / ascription / open ---- *)
  (* declare a type by name; a sig declaration is the mint site for its
     component labels (t, empty, incr, get share labels with every struct and
     sig that uses those names — ascription matches BY LABEL) *)
  let define_type name body_s =
    match Parse.parse_ty body_s with
    | Ok sty -> (
        match Resolver.resolve_ty ~ns ~st:store ~mint:(Some mint_src) sty with
        | Ok h -> (try Namespace.rebind ns ~name h with _ -> ()); Some h
        | Error _ -> None)
    | Error _ -> None
  in
  (* bind a term with no annotation (its synthesized type) — used for raw
     struct implementations, whose fully-manifest sig is the point *)
  let define_raw name expr_s =
    match Parse.parse_expr expr_s with
    | Ok se -> (
        match Resolver.resolve ~ctx:[] ~mint:(Some mint_src) ~ns ~st:store se with
        | Ok node -> (
            match Store.ingest_term store node with
            | Ok h -> (try Namespace.rebind ns ~name h with _ -> ()); Some h
            | Error _ -> None)
        | Error _ -> None)
    | Error _ -> None
  in
  ignore
    (define_type "CounterSig" "sig { type t, empty: t, incr: t -> t, get: t -> Int }");
  (* the implementation is a STRUCT: type members + term members in one value.
     `raw` is a private member — the ascription below drops it (width). The
     unascribed binding stays fully transparent (every type member manifest). *)
  ignore
    (define_raw "counter.impl"
       "struct { type t = Int, empty = 0, incr = \\x: t. x + 1, get = \\x: t. x, raw = \\x: t. x }");
  (* ascription `M :> S` is the sealing gesture (the p14 pack, recast): purely
     structural, no mint — both factory branches unify at the ONE sig type *)
  ignore
    (define_str "counter.sealed" [] "CounterSig" "counter.impl :> CounterSig");
  ignore
    (define_str "mk_counter" [] "Bool -> CounterSig"
       ("\\fast: Bool. if fast then counter.impl :> CounterSig"
        ^ " else (struct { type t = Int * Int, empty = (0, 0), incr = \\p: t. (fst p + 1, snd p), get = \\p: t. fst p }) :> CounterSig"));
  (* ---- n-ary top-level open: one gesture, names recovered from labels ---- *)
  let open_module name expr_s =
    let leaf nm = match String.rindex_opt nm '.' with Some i -> String.sub nm (i + 1) (String.length nm - i - 1) | None -> nm in
    let bind_labeled (lh, h) =
      match Namespace.name_of ns lh with
      | Some nm -> (try Namespace.rebind ns ~name:(name ^ "." ^ leaf nm) h with _ -> ())
      | None -> ()
    in
    match Parse.parse_expr expr_s with
    | Ok se -> (
        match Resolver.resolve ~ctx:[] ~ns ~st:store se with
        | Ok node -> (
            match Open_module.open_package store mint_src node with
            | Ok { Open_module.type_bindings; module_hash; fields } ->
                List.iter bind_labeled type_bindings;
                (try Namespace.rebind ns ~name module_hash with _ -> ());
                List.iter bind_labeled fields
            | Error _ -> ())
        | Error _ -> ())
    | Error _ -> ()
  in
  open_module "Box" "mk_counter true";
  test_str "Module.Tests.opened_box" "Box.get (Box.incr (Box.incr Box.empty)) == 2";
  (* generativity: a SECOND open of the same package mints a fresh, incompatible
     abstraction — Box.t and Box2.t render as distinct abstracts in the browser;
     try `Box.get Box2.empty` live to see the rejection *)
  open_module "Box2" "mk_counter true";
  (* ---- ONE sig hiding TWO types: date and span stay distinct ---- *)
  ignore
    (define_type "CalSig"
       "sig { type date, type span, origin: date, after: Int -> span, shift: date -> span -> date, between: date -> date -> span, length_of: span -> Int }");
  ignore
    (define_str "mk_calendar" [] "Int -> CalSig"
       {|\base: Int. (struct { type date = Int, type span = Int, origin = base, after = \n: Int. n, shift = \d: date. \s: span. d + s, between = \a: date. \b: date. b - a, length_of = \s: span. s }) :> CalSig|});
  open_module "Cal" "mk_calendar 0";
  test_str "Calendar.Tests.length"
    "Cal.length_of (Cal.between Cal.origin (Cal.shift Cal.origin (Cal.after 30))) == 30";
  (* ---- translucency: `scalar` stays manifest through sealing AND opening ---- *)
  ignore
    (define_type "VecSig"
       "sig { type scalar = Int, type v, zero: v, one: v, add: v -> v -> v, smul: scalar -> v -> v, norm1: v -> scalar }");
  ignore
    (define_str "vec.sealed" [] "VecSig"
       "(struct { type scalar = Int, type v = Int * Int, zero = (0, 0), one = (1, 1), add = \\a: v. \\b: v. (fst a + fst b, snd a + snd b), smul = \\k: scalar. \\a: v. (k mul fst a, k mul snd a), norm1 = \\a: v. fst a + snd a }) :> VecSig");
  open_module "Vec" "vec.sealed";
  (* Vec.scalar is bound to Int itself — a literal 3 feeds smul directly *)
  test_str "Vec.Tests.translucent_scalar" "Vec.norm1 (Vec.smul 3 Vec.one) == 6";
  (* ---- the p15 open-question, answered: open INSIDE a function body ----
     scoped like unpack (no mint; avoidance keeps the hidden type in); works
     over any CounterSig value, whichever witness it carries *)
  ignore
    (define_str "total" [] "CounterSig -> Int"
       "\\m: CounterSig. open m as c in c#get (c#incr (c#incr c#empty))");
  test_str "LocalOpen.Tests.int_rep" "total (mk_counter true) == 2";
  test_str "LocalOpen.Tests.pair_rep" "total (mk_counter false) == 2";
  (* ---- p16: records as plain data + #-projection ---- *)
  ignore (define_type "Geom.Point" "{ x: Int, y: Int }");
  ignore (define_str "Geom.origin" [] "Geom.Point" "{ x = 0, y = 0 }");
  ignore (define_str "Geom.example" [] "Geom.Point" "{ x = 3, y = 4 }");
  ignore
    (define_str "Geom.taxicab" [] "Geom.Point -> Int" "\\p: Geom.Point. p#x + p#y");
  test_str "Records.Tests.projection" "Geom.example#x + Geom.example#y == 7";
  test_str "Records.Tests.taxicab" "Geom.taxicab Geom.example == 7";
  (* ---- p15: lists, with [| ... |] literal syntax ---- *)
  ignore (define_str "demo.nums" [] "List Int" "[| 1, 2, 3 |]");
  test_str "Lists.Tests.sum" "fold demo.nums 0 (\\x: Int. \\acc: Int. x + acc) == 6"

let create () =
  let store = Store.create () in
  let ns = Namespace.create () in
  let att = Attachment.create () in
  let mint_src = Mint.make_source () in
  seed ~store ~ns ~att ~mint_src;
  { store; ns; att; mint_src }

let global : t = create ()
