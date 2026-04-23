(* Pure-tier + eager-ingest compute for the author pane.
   No Bonsai here — this file runs the substrate pipeline and produces a
   [State.feedback] record. It's called from a debounced Bonsai computation.

   Key design decision (see decisions.md §Ingest-on-keystroke): we ingest
   directly into [Substrate.global.store]. Safe because ingest is
   idempotent under content addressing and Resolver enforces every store
   invariant. *)

open! Core
open P7_web_interface_substrate

let format_error : Resolver.error -> string = function
  | Unbound_name n -> "unbound name: " ^ n
  | Missing_hash (n, h) ->
      Printf.sprintf "name %s resolves to missing hash %s" n (Hash.to_string h)
  | Language_mismatch (n, expected, actual) ->
      Printf.sprintf "name %s: expected %s, got %s" n expected actual
  | Type_error msg -> "type error: " ^ msg

let collect_resolved_names_lc ~(ns : Namespace.t)
    (surface : Lc_surface_ast.t) : (string * Hash.t) list =
  let rec walk ~in_scope acc = function
    | Lc_surface_ast.Var n ->
        if List.mem in_scope n ~equal:String.equal then acc
        else (
          match Namespace.resolve ns n with
          | Some h ->
              if List.exists acc ~f:(fun (n', _) -> String.equal n n') then acc
              else (n, h) :: acc
          | None -> acc)
    | Lam (x, body) -> walk ~in_scope:(x :: in_scope) acc body
    | App (f, a) ->
        let acc = walk ~in_scope acc f in
        walk ~in_scope acc a
  in
  List.rev (walk ~in_scope:[] [] surface)

let collect_resolved_names_stlc ~(ns : Namespace.t)
    (surface : Stlc_surface_ast.t) : (string * Hash.t) list =
  let rec walk ~in_scope acc = function
    | Stlc_surface_ast.Var n ->
        if List.mem in_scope n ~equal:String.equal then acc
        else (
          match Namespace.resolve ns n with
          | Some h ->
              if List.exists acc ~f:(fun (n', _) -> String.equal n n') then acc
              else (n, h) :: acc
          | None -> acc)
    | Lam (x, _ty, body) -> walk ~in_scope:(x :: in_scope) acc body
    | App (f, a) ->
        let acc = walk ~in_scope acc f in
        walk ~in_scope acc a
    | True | False -> acc
    | If (c, t, e) ->
        let acc = walk ~in_scope acc c in
        let acc = walk ~in_scope acc t in
        walk ~in_scope acc e
  in
  List.rev (walk ~in_scope:[] [] surface)

let feedback_lc ~(s : Substrate.t) (buffer : string) : State.feedback =
  if String.is_empty (String.strip buffer) then State.Empty
  else
    let lexbuf = Lexing.from_string buffer in
    match Lc_parser.main Lc_lexer.token lexbuf with
    | exception _ -> State.Parse_error "parse error"
    | surface -> (
        match Resolver.resolve_lc ~namespace:s.ns ~store:s.store surface with
        | Error e -> State.Resolve_error (format_error e)
        | Ok ast ->
            let canonical = Lc_canonicalize.canonicalize ast in
            let size_before = Store.size s.store in
            let hash = Store.ingest_lc s.store canonical in
            let was_new = Store.size s.store > size_before in
            let resolved = collect_resolved_names_lc ~ns:s.ns surface in
            State.Ok_ { hash; was_new; typ_str = None; resolved })

let feedback_stlc ~(s : Substrate.t) (buffer : string) : State.feedback =
  if String.is_empty (String.strip buffer) then State.Empty
  else
    let lexbuf = Lexing.from_string buffer in
    match Stlc_parser.main Stlc_lexer.token lexbuf with
    | exception _ -> State.Parse_error "parse error"
    | surface -> (
        match Resolver.resolve_stlc ~namespace:s.ns ~store:s.store surface with
        | Error e -> State.Resolve_error (format_error e)
        | Ok ast ->
            let canonical = Stlc_canonicalize.canonicalize ast in
            let typ_str =
              match Stlc_typecheck.infer ~ctx:[] canonical with
              | Ok ty -> Some (Ty.print ty)
              | Error _ -> None
            in
            let size_before = Store.size s.store in
            let hash = Store.ingest_stlc s.store canonical in
            let was_new = Store.size s.store > size_before in
            let resolved = collect_resolved_names_stlc ~ns:s.ns surface in
            State.Ok_ { hash; was_new; typ_str; resolved })

let compute ~(s : Substrate.t) ~(lang : State.author_lang) ~(buffer : string) :
    State.feedback =
  match lang with
  | Lc -> feedback_lc ~s buffer
  | Stlc -> feedback_stlc ~s buffer
