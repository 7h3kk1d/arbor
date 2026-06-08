(* Module-level singleton: the one Store + Namespace the whole app shares, plus
   the mint source for new abstract types. The editing context's open set is UI
   state and lives in the Bonsai model, not here. Bootstrap seeds the Counter
   worked example so the app is demonstrable on load. *)

open P16_substrate

type t = {
  store : Store.t;
  ns : Namespace.t;
  att : Attachment.t;
  mint_src : Mint.source;
}

(* A small spread of "modules" so the namespace reads like a real codebase:
   Counter (over Int); a Temperature pair — Celsius.t and Kelvin.t, both over Int
   but distinct by mint — whose conversions open BOTH abstract types at once; and
   Range (over a Product witness). Consumers (bump2, readout, Temp.round_trip)
   compose sealed ops at the abstract level and stay ordinary terms. *)
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
        match
          Resolver.resolve_ty ~ns ~st:store sty,
          Resolver.resolve ~ctx:[] ~ns ~st:store se
        with
        | Ok ann, Ok node ->
            let ctx = Editing_context.make () in
            List.iter (fun o -> Editing_context.open_type ctx o) opens;
            (match Editing_context.commit store ctx ~term:node ~ann with
             | Ok (h, _) -> (try Namespace.rebind ns ~name h with _ -> ()); Some h
             | Error _ -> None)
        | _ -> None)
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
  (match incr, get with
   | Some incr, Some get ->
       ignore
         (bind "bump2" []
            (Node.Lam (counter, app (Node.Ref incr) (app (Node.Ref incr) (var 0))))
            (arrow counter counter));
       ignore
         (bind "readout" []
            (Node.Lam (counter, app (Node.Ref get) (app (Node.Ref incr) (var 0))))
            (arrow counter int_h))
   | _ -> ());
  (* ---- Temperature: Celsius + Kelvin; conversions open BOTH ---- *)
  let celsius = abstract "Celsius.t" int_h in
  let kelvin = abstract "Kelvin.t" int_h in
  ignore (bind "Celsius.freezing" [ celsius ] (lit 0) celsius);
  ignore
    (bind "Celsius.is_freezing" [ celsius ]
       (Node.Lam (celsius, Node.Prim (Node.Eq, [ var 0; lit 0 ]))) (arrow celsius bool_h));
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
  (* ---- Range, over a Product witness ---- *)
  let range = abstract "Range.t" (prod int_h int_h) in
  ignore
    (bind "Range.make" [ range ]
       (Node.Lam (int_h, Node.Lam (int_h, Node.Pair (var 1, var 0))))
       (arrow int_h (arrow int_h range)));
  ignore (bind "Range.lo" [ range ] (Node.Lam (range, Node.Fst (var 0))) (arrow range int_h));
  ignore (bind "Range.hi" [ range ] (Node.Lam (range, Node.Snd (var 0))) (arrow range int_h));
  ignore
    (bind "Range.width" [ range ]
       (Node.Lam (range, Node.Prim (Node.Sub, [ Node.Snd (var 0); Node.Fst (var 0) ])))
       (arrow range int_h));
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
  test "Range.Tests.width" []
    (eqn (app (r "Range.width") (app (app (r "Range.make") (lit 2)) (lit 5))) (lit 3));
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
  (* ---- existential: a factory that chooses and hides its representation ---- *)
  let ex = "exists t. t * ((t -> t) * (t -> Int))" in
  ignore
    (define_str "mkCounter" []
       ("Bool -> " ^ ex)
       ("\\fast: Bool. if fast "
        ^ "then pack [Int] (0, (\\x: Int. x + 1, \\x: Int. x)) as " ^ ex
        ^ " else pack [Int * Int] ((0, 0), (\\p: Int * Int. (fst p + 1, snd p), \\p: Int * Int. fst p)) as "
        ^ ex));
  (* the SAME factory observed through two hidden representations — both give 2 *)
  let observe b =
    "(unpack [t] c = mkCounter " ^ b
    ^ " in snd (snd c) ((fst (snd c)) ((fst (snd c)) (fst c)))) == 2"
  in
  test_str "Existential.Tests.int_rep" (observe "true");
  test_str "Existential.Tests.pair_rep" (observe "false");
  (* ---- p16: open a package into the namespace (n-ary: peels every `exists`) ---- *)
  let open_existential name type_names field_names expr_s =
    let nth_name names i =
      match List.nth_opt names i with Some x when x <> "" -> Some x | _ -> None
    in
    let leaf nm = match String.rindex_opt nm '.' with Some i -> String.sub nm (i + 1) (String.length nm - i - 1) | None -> nm in
    let label_name lh = match Namespace.name_of ns lh with Some nm -> Some (leaf nm) | None -> None in
    match Parse.parse_expr expr_s with
    | Ok se -> (
        match Resolver.resolve ~ctx:[] ~ns ~st:store se with
        | Ok node -> (
            match Open_existential.open_package store mint_src node with
            | Ok { Open_existential.type_hashes; module_hash; fields } ->
                List.iteri
                  (fun i th ->
                    let tn =
                      match nth_name type_names i with
                      | Some x -> x
                      | None -> Pretty.tyvar_name i
                    in
                    try Namespace.rebind ns ~name:(name ^ "." ^ tn) th with _ -> ())
                  type_hashes;
                (try Namespace.rebind ns ~name module_hash with _ -> ());
                List.iteri
                  (fun j (label_opt, fh) ->
                    let fname =
                      match nth_name field_names j with
                      | Some n -> Some n
                      | None -> ( match label_opt with Some lh -> label_name lh | None -> None )
                    in
                    match fname with
                    | Some n -> (try Namespace.rebind ns ~name:(name ^ "." ^ n) fh with _ -> ())
                    | None -> ())
                  fields
            | Error _ -> ())
        | Error _ -> ())
    | Error _ -> ()
  in
  open_existential "Box" [] [ "empty"; "incr"; "get" ] "mkCounter true";
  test_str "Existential.Tests.opened_box" "Box.get (Box.incr (Box.incr Box.empty)) == 2";
  (* a FUNCTOR (Int offset -> module) hiding TWO abstract types — a celsius `c`
     and a kelvin `k`, with conversions that differ by the offset. Apply and open
     in one gesture into Temp.cel / Temp.kel + the conversions. Try it live: type
     `mkScale 273` (or any offset) in the editor and open the result. *)
  let scale_out = "exists c. exists k. (Int -> c) * ((c -> k) * ((k -> c) * (c -> Int)))" in
  let scale_inner = "exists k. (Int -> Int) * ((Int -> k) * ((k -> Int) * (Int -> Int)))" in
  let scale_body = {|(\x: Int. x, (\x: Int. x + off, (\x: Int. x - off, \x: Int. x)))|} in
  ignore
    (define_str "mkScale" []
       (Printf.sprintf "Int -> %s" scale_out)
       (Printf.sprintf {|\off: Int. pack [Int] (pack [Int] (%s) as %s) as %s|} scale_body
          scale_inner scale_out));
  open_existential "Temp" [ "cel"; "kel" ] [ "fromC"; "toK"; "toC"; "readC" ] "mkScale 273";
  test_str "Existential.Tests.temp_round_trip"
    "Temp.readC (Temp.toC (Temp.toK (Temp.fromC 100))) == 100";
  (* a slightly more realistic two-abstract-type functor: a calendar where a
     `date` and a `span` (duration) are distinct types — so adding two dates, or
     measuring a date as a duration, is a type error. mkCalendar's Int is the
     epoch: the day-number of the origin. Try it live: type `mkCalendar 0`. *)
  let cal_out =
    "exists date. exists span. date * ((Int -> span) * ((date -> span -> date) * ((date -> date -> span) * (span -> Int))))"
  in
  let cal_inner =
    "exists span. Int * ((Int -> span) * ((Int -> span -> Int) * ((Int -> Int -> span) * (span -> Int))))"
  in
  let cal_val =
    {|(base, (\n: Int. n, (\d: Int. \s: Int. d + s, (\a: Int. \b: Int. b - a, \s: Int. s))))|}
  in
  ignore
    (define_str "mkCalendar" []
       (Printf.sprintf "Int -> %s" cal_out)
       (Printf.sprintf {|\base: Int. pack [Int] (pack [Int] (%s) as %s) as %s|} cal_val
          cal_inner cal_out));
  open_existential "Cal" [ "date"; "span" ]
    [ "origin"; "after"; "shift"; "between"; "lengthOf" ] "mkCalendar 0";
  test_str "Existential.Tests.calendar"
    "Cal.lengthOf (Cal.between Cal.origin (Cal.shift Cal.origin (Cal.after 30))) == 30";
  (* ---- p16: lists, with [| ... |] literal syntax ---- *)
  ignore (define_str "demo.nums" [] "List Int" "[| 1, 2, 3 |]");
  test_str "Lists.Tests.sum" "fold demo.nums 0 (\\x: Int. \\acc: Int. x + acc) == 6";
  test_str "Lists.Tests.literal" "fold [| 10, 20, 30 |] 0 (\\x: Int. \\acc: Int. x + acc) == 60"

let create () =
  let store = Store.create () in
  let ns = Namespace.create () in
  let att = Attachment.create () in
  let mint_src = Mint.make_source () in
  seed ~store ~ns ~att ~mint_src;
  { store; ns; att; mint_src }

let global : t = create ()
