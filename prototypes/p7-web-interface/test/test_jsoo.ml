(* Smoke test: ingests a few canonical definitions and prints hashes.
   Built twice — natively and under js_of_ocaml. Run both and compare
   outputs line-for-line. Any divergence means digestif.ocaml and
   digestif.c disagree on BLAKE2B output for our inputs, which would
   invalidate the substrate under jsoo. *)

open P7_web_interface_substrate

let parse_stlc s =
  let lexbuf = Lexing.from_string s in
  Stlc_parser.main Stlc_lexer.token lexbuf

let parse_lc s =
  let lexbuf = Lexing.from_string s in
  Lc_parser.main Lc_lexer.token lexbuf

let () =
  let store = Store.create () in
  let ns = Namespace.create () in
  (* stlc: identity *)
  let id_stlc =
    match
      Resolver.resolve_stlc ~namespace:ns ~store (parse_stlc "\\x:Bool. x")
    with
    | Ok ast ->
        Store.ingest_stlc store (Stlc_canonicalize.canonicalize ast)
    | Error _ -> failwith "resolve id_stlc"
  in
  (* stlc: not *)
  let not_stlc =
    match
      Resolver.resolve_stlc ~namespace:ns ~store
        (parse_stlc "\\x:Bool. if x then false else true")
    with
    | Ok ast ->
        Store.ingest_stlc store (Stlc_canonicalize.canonicalize ast)
    | Error _ -> failwith "resolve not_stlc"
  in
  (* stlc: true *)
  let true_stlc =
    match Resolver.resolve_stlc ~namespace:ns ~store (parse_stlc "true") with
    | Ok ast ->
        Store.ingest_stlc store (Stlc_canonicalize.canonicalize ast)
    | Error _ -> failwith "resolve true_stlc"
  in
  (* lc: identity *)
  let id_lc =
    match Resolver.resolve_lc ~namespace:ns ~store (parse_lc "\\x. x") with
    | Ok ast -> Store.ingest_lc store (Lc_canonicalize.canonicalize ast)
    | Error _ -> failwith "resolve id_lc"
  in
  (* lc: \f. \x. f x *)
  let wrap_lc =
    match
      Resolver.resolve_lc ~namespace:ns ~store (parse_lc "\\f. \\x. f x")
    with
    | Ok ast -> Store.ingest_lc store (Lc_canonicalize.canonicalize ast)
    | Error _ -> failwith "resolve wrap_lc"
  in
  Printf.printf "id_stlc   %s\n" (Hash.to_string id_stlc);
  Printf.printf "not_stlc  %s\n" (Hash.to_string not_stlc);
  Printf.printf "true_stlc %s\n" (Hash.to_string true_stlc);
  Printf.printf "id_lc     %s\n" (Hash.to_string id_lc);
  Printf.printf "wrap_lc   %s\n" (Hash.to_string wrap_lc);
  Printf.printf "store_size %d\n" (Store.size store)
