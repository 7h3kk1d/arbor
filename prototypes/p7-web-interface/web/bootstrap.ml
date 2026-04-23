(* Seeds the Store and Namespace with a canonical demo set at app start.
   Runs once, via Substrate.create.

   Goals:
   - Give the browser list non-empty content on first load so the UX
     story is visible without needing the user to author anything.
   - Demonstrate aliasing (one hash, multiple names).
   - Demonstrate structural sharing — `not_not` references `not`, whose
     body node is shared in the DAG.
   - Leave aspects (typechecks / evals / translations) mostly uncached so
     the user has something to click. *)

open! Core
open P7_web_interface_substrate

let ingest_lc ~ns ~store ~name src : Hash.t option =
  let lexbuf = Lexing.from_string src in
  match Lc_parser.main Lc_lexer.token lexbuf with
  | exception _ -> None
  | surface -> (
      match Resolver.resolve_lc ~namespace:ns ~store surface with
      | Error _ -> None
      | Ok ast ->
          let canonical = Lc_canonicalize.canonicalize ast in
          let h = Store.ingest_lc store canonical in
          (try Namespace.bind ns ~name h with _ -> ());
          Some h)

let ingest_stlc ~ns ~store ~name src : Hash.t option =
  let lexbuf = Lexing.from_string src in
  match Stlc_parser.main Stlc_lexer.token lexbuf with
  | exception _ -> None
  | surface -> (
      match Resolver.resolve_stlc ~namespace:ns ~store surface with
      | Error _ -> None
      | Ok ast ->
          let canonical = Stlc_canonicalize.canonicalize ast in
          let h = Store.ingest_stlc store canonical in
          (try Namespace.bind ns ~name h with _ -> ());
          Some h)

let alias ~ns ~name (h : Hash.t) : unit =
  try Namespace.bind ns ~name h with _ -> ()

let seed ~(store : Store.t) ~(att : Attachment.t) ~(ns : Namespace.t) : unit =
  let _ = att in
  (* Untyped lc combinators — classic, good for showing α-equivalence
     at a glance and for demonstrating the Church-encoded targets of
     stlc→lc translations. *)
  let _i_lc = ingest_lc ~ns ~store ~name:"i" "\\x. x" in
  let _k_lc = ingest_lc ~ns ~store ~name:"k" "\\x. \\y. x" in
  let _s_lc = ingest_lc ~ns ~store ~name:"s" "\\x. \\y. \\z. x z (y z)" in
  (* Alias: the identity has two names — "i" and "id_lc" both point at
     the same hash, demonstrating that names are a many-to-one mapping
     and that rebinding is an editing-layer operation. *)
  (match Namespace.resolve ns "i" with
   | Some h -> alias ~ns ~name:"id_lc" h
   | None -> ());
  (* Stlc — monomorphic over Bool. *)
  let _id_bool =
    ingest_stlc ~ns ~store ~name:"id_bool" "\\x:Bool. x"
  in
  let _not =
    ingest_stlc ~ns ~store ~name:"not"
      "\\x:Bool. if x then false else true"
  in
  let _and_ =
    ingest_stlc ~ns ~store ~name:"and"
      "\\x:Bool. \\y:Bool. if x then y else false"
  in
  let _or_ =
    ingest_stlc ~ns ~store ~name:"or"
      "\\x:Bool. \\y:Bool. if x then true else y"
  in
  (* Structural sharing: `not_not` references `not` in its body.
     Because p7's Resolver inlines referenced names at ingest time,
     the inner `not` collapses to the same hash as the top-level one.
     The Detail view's clickable child chip renders this as a chip. *)
  let _not_not =
    ingest_stlc ~ns ~store ~name:"not_not" "\\x:Bool. not (not x)"
  in
  (* A constant — lets the user see an If-less definition. *)
  let _const_true =
    ingest_stlc ~ns ~store ~name:"const_true" "\\x:Bool. true"
  in
  (* Two worked applications the user can evaluate:
     - not_true should normalize to false
     - and_true_false should normalize to false *)
  let _not_true =
    ingest_stlc ~ns ~store ~name:"not_true" "not true"
  in
  let _and_true_false =
    ingest_stlc ~ns ~store ~name:"and_true_false" "and true false"
  in
  (* A cross-language orphan: a lambda that doesn't simply-type.
     Useful for demonstrating the partial translator's refusal path.
     Named `omega_w` to match p6's scope-doc walkthrough. *)
  let _omega_w = ingest_lc ~ns ~store ~name:"omega_w" "\\x. x x" in
  ()
