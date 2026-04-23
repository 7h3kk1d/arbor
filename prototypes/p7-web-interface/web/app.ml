(* Top-level Bonsai computation. Holds the State.t model in a
   state_machine0; every substrate-mutating action bumps [version] so
   views depending on substrate content re-render. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open State

module Action = struct
  type t = State.action [@@deriving sexp_of]
end

let apply_action ~inject:_ ~schedule_event:_ (m : State.t) (a : State.action) :
    State.t =
  let bump (m : State.t) = { m with version = m.version + 1 } in
  match a with
  | Set_view v ->
      if State.equal_view v m.view then m
      else { m with view = v; nav_back = m.view :: m.nav_back }
  | Go_back -> (
      match m.nav_back with
      | [] -> m
      | prev :: rest -> { m with view = prev; nav_back = rest })
  | Set_filter f -> { m with filter = f }
  | Set_author_lang l ->
      (* Re-run feedback against the current buffer under the new parser,
         so stale hashes / errors don't linger after a language switch. *)
      let fb =
        Feedback.compute ~s:Substrate.global ~lang:l ~buffer:m.author_buffer
      in
      let m = { m with author_lang = l; feedback = fb } in
      (match fb with
       | Ok_ { hash; _ } ->
           if m.auto_eval then (
             match l with
             | Lc ->
                 ignore
                   (P7_web_interface_substrate.Lc_eval.eval
                      ~store:Substrate.global.store
                      ~att:Substrate.global.att
                      ~step_limit:Substrate.global.step_limit hash
                     : P7_web_interface_substrate.Lc_eval.result)
             | Stlc ->
                 ignore
                   (P7_web_interface_substrate.Stlc_eval.eval
                      ~store:Substrate.global.store
                      ~att:Substrate.global.att
                      ~step_limit:Substrate.global.step_limit hash
                     : P7_web_interface_substrate.Stlc_eval.result));
           if m.auto_typecheck && State.equal_author_lang l Stlc then
             ignore
               (P7_web_interface_substrate.Stlc_typecheck.check
                  ~store:Substrate.global.store ~att:Substrate.global.att
                  hash);
           bump m
       | _ -> m)
  | Set_author_buffer s -> { m with author_buffer = s }
  | Set_bind_as s -> { m with author_bind_as = s }
  | Set_auto_eval b -> (
      let m = { m with auto_eval = b } in
      match b, m.feedback with
      | true, Ok_ { hash; _ } ->
          let tag =
            P7_web_interface_substrate.Pretty.language_tag_of_hash
              Substrate.global.store hash
          in
          if String.is_substring tag ~substring:"stlc" then
            ignore
              (P7_web_interface_substrate.Stlc_eval.eval
                 ~store:Substrate.global.store ~att:Substrate.global.att
                 ~step_limit:Substrate.global.step_limit hash
                : P7_web_interface_substrate.Stlc_eval.result)
          else
            ignore
              (P7_web_interface_substrate.Lc_eval.eval
                 ~store:Substrate.global.store ~att:Substrate.global.att
                 ~step_limit:Substrate.global.step_limit hash
                : P7_web_interface_substrate.Lc_eval.result);
          bump m
      | _ -> m)
  | Filter_by_type t ->
      {
        m with
        filter = { m.filter with type_filter = t };
        view = Author;
      }
  | Set_auto_typecheck b -> (
      let m = { m with auto_typecheck = b } in
      match b, m.feedback, m.author_lang with
      | true, Ok_ { hash; _ }, Stlc ->
          ignore
            (P7_web_interface_substrate.Stlc_typecheck.check
               ~store:Substrate.global.store ~att:Substrate.global.att hash);
          bump m
      | _ -> m)
  | Feedback_updated fb ->
      let m = { m with feedback = fb } in
      (match fb with
       | Ok_ { was_new = true; _ } -> bump m
       | _ -> m)
  | Bind_current -> (
      match m.feedback, m.author_bind_as with
      | Ok_ { hash; _ }, name when not (String.is_empty (String.strip name)) ->
          (try
             P7_web_interface_substrate.Namespace.bind
               Substrate.global.ns ~name hash;
             bump
               {
                 m with
                 author_bind_as = "";
                 view = Detail hash;
               }
           with _ -> m)
      | _ -> m)
  | Bind_existing { name; hash } -> (
      try
        P7_web_interface_substrate.Namespace.bind
          Substrate.global.ns ~name hash;
        bump m
      with _ -> m)
  | Unbind name ->
      if P7_web_interface_substrate.Namespace.unbind
           Substrate.global.ns ~name
      then bump m
      else m
  | Request_rebind { name; new_hash } -> (
      match
        P7_web_interface_substrate.Namespace.resolve Substrate.global.ns name
      with
      | Some old -> { m with pending_rebind = Some (name, old, new_hash) }
      | None -> m)
  | Confirm_rebind -> (
      match m.pending_rebind with
      | Some (name, _old, new_hash) ->
          P7_web_interface_substrate.Namespace.rebind Substrate.global.ns
            ~name new_hash;
          bump { m with pending_rebind = None; view = Detail new_hash }
      | None -> m)
  | Cancel_rebind -> { m with pending_rebind = None }
  | Evaluate h ->
      let _result =
        match P7_web_interface_substrate.Pretty.language_tag_of_hash
                Substrate.global.store h with
        | tag when String.is_prefix tag ~prefix:"[lc]" ->
            ignore
              (P7_web_interface_substrate.Lc_eval.eval
                 ~store:Substrate.global.store
                 ~att:Substrate.global.att
                 ~step_limit:Substrate.global.step_limit
                 h : P7_web_interface_substrate.Lc_eval.result)
        | _ ->
            ignore
              (P7_web_interface_substrate.Stlc_eval.eval
                 ~store:Substrate.global.store
                 ~att:Substrate.global.att
                 ~step_limit:Substrate.global.step_limit
                 h : P7_web_interface_substrate.Stlc_eval.result)
      in
      bump m
  | Typecheck h ->
      ignore
        (P7_web_interface_substrate.Stlc_typecheck.check
           ~store:Substrate.global.store ~att:Substrate.global.att h);
      bump m
  | Translate_stlc_to_lc h -> (
      match
        P7_web_interface_substrate.Stlc_to_lc_erase_church.translate
          ~store:Substrate.global.store ~att:Substrate.global.att h
      with
      | Ok (target, _was_cached) ->
          let m' = bump m in
          { m' with view = Detail target; nav_back = m.view :: m.nav_back }
      | Error _ -> m)
  | Translate_lc_to_stlc (h, ty_src) -> (
      let lexbuf = Lexing.from_string ty_src in
      match
        P7_web_interface_substrate.Stlc_parser.main_ty
          P7_web_interface_substrate.Stlc_lexer.token lexbuf
      with
      | exception _ -> m
      | ty -> (
          match
            P7_web_interface_substrate.Lc_to_stlc_check.translate
              ~store:Substrate.global.store ~att:Substrate.global.att
              h ty
          with
          | Ok
              ( P7_web_interface_substrate.Lc_to_stlc_check.Translated target
              , _was_cached ) ->
              let m' = bump m in
              {
                m' with
                view = Detail target;
                nav_back = m.view :: m.nav_back;
              }
          | Ok (Untypable _, _) ->
              (* Refusal row will appear in the aspects panel. *)
              bump m
          | Error _ -> m))

let component : Vdom.Node.t Bonsai.Computation.t =
  let%sub state_and_inject =
    Bonsai.state_machine0
      (module struct
        type t = State.t [@@deriving sexp, equal]
      end)
      (module Action)
      ~default_model:State.initial
      ~apply_action
  in
  let%sub state =
    let%arr (s, _) = state_and_inject in
    s
  in
  let%sub inject =
    let%arr (_, f) = state_and_inject in
    f
  in
  let%sub browser = Browser.view ~state ~inject in
  let%sub right_pane = Right_pane.view ~state ~inject in
  let%sub rebind_overlay = Rebind_dialog.view ~state ~inject in
  let%arr browser = browser
  and right_pane = right_pane
  and rebind_overlay = rebind_overlay in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "app" ]
    [ browser; right_pane; rebind_overlay ]
