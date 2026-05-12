(* Top-level Bonsai computation. Three-pane layout: namespace tree
   (left), editor + recovered-AST panel + feedback (center), detail of
   selected hash (right). The state machine bumps [version] on every
   substrate-mutating action so views re-render. *)

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
  | Toggle_path path ->
      let already =
        List.exists m.expanded_paths ~f:(String.equal path)
      in
      let next =
        if already then
          List.filter m.expanded_paths ~f:(fun p -> not (String.equal p path))
        else path :: m.expanded_paths
      in
      { m with expanded_paths = next }
  | Set_filter f -> { m with filter = f }
  | Set_editor_mode mode -> { m with editor_mode = mode }
  | Set_author_buffer s -> { m with author_buffer = s }
  | Set_bind_as s -> { m with author_bind_as = s }
  | Set_author_ty_buffer s -> { m with author_ty_buffer = s }
  | Set_ty_bind_as s -> { m with author_ty_bind_as = s }
  | Set_auto_eval b -> (
      let m = { m with auto_eval = b } in
      match b, m.feedback with
      | true, Recovered { ingest = Ingested { hash; _ }; _ } ->
          ignore
            (P10_minted_labels_substrate.Eval.eval
               ~store:Substrate.global.store ~att:Substrate.global.att
               ~step_limit:Substrate.global.step_limit hash
              : P10_minted_labels_substrate.Eval.result);
          bump m
      | _ -> m)
  | Feedback_updated fb ->
      let m = { m with feedback = fb } in
      (match fb with
       | Recovered { ingest = Ingested { was_new = true; _ }; _ } -> bump m
       | _ -> m)
  | Ty_feedback_updated fb ->
      let m = { m with ty_feedback = fb } in
      (match fb with
       | Ty_recovered { ingest = Ty_ingested { was_new = true; _ }; _ } ->
           bump m
       | _ -> m)
  | Bind_current_ty -> (
      match m.ty_feedback, m.author_ty_bind_as with
      | Ty_recovered { ingest = Ty_ingested { hash; _ }; _ }, name
        when not (String.is_empty (String.strip name)) -> (
          (* Wrap with a fresh Named_type before binding. The minted
             hash is what the namespace sees and what the detail view
             opens. *)
          let bound_hash =
            P10_minted_labels_substrate.Store.register_named_type
              Substrate.global.store hash
          in
          try
            P10_minted_labels_substrate.Namespace.bind
              Substrate.global.ns ~name bound_hash;
            bump
              {
                m with
                author_ty_bind_as = "";
                view = Detail bound_hash;
              }
          with
          | P10_minted_labels_substrate.Namespace.Name_already_bound _ -> (
              match
                P10_minted_labels_substrate.Namespace.resolve
                  Substrate.global.ns name
              with
              | Some old ->
                  { m with pending_rebind = Some (name, old, bound_hash) }
              | None -> m)
          | _ -> m)
      | _ -> m)
  | Bind_current -> (
      match m.feedback, m.author_bind_as with
      | Recovered { ingest = Ingested { hash; _ }; _ }, name
        when not (String.is_empty (String.strip name)) -> (
          (* Wrap the substructure body with a fresh Named_term so the
             namespace binding is minted. Two `bind` actions for the
             same source mint distinct Named_term hashes. *)
          let bound_hash =
            P10_minted_labels_substrate.Store.register_named_term
              Substrate.global.store hash
          in
          try
            P10_minted_labels_substrate.Namespace.bind
              Substrate.global.ns ~name bound_hash;
            bump
              {
                m with
                author_bind_as = "";
                view = Detail bound_hash;
              }
          with
          | P10_minted_labels_substrate.Namespace.Name_already_bound _ -> (
              (* Show the rebind dialog. *)
              match
                P10_minted_labels_substrate.Namespace.resolve
                  Substrate.global.ns name
              with
              | Some old ->
                  { m with pending_rebind = Some (name, old, bound_hash) }
              | None -> m)
          | _ -> m)
      | _ -> m)
  | Bind_existing { name; hash } -> (
      try
        P10_minted_labels_substrate.Namespace.bind
          Substrate.global.ns ~name hash;
        bump m
      with _ -> m)
  | Unbind name ->
      if P10_minted_labels_substrate.Namespace.unbind
           Substrate.global.ns ~name
      then bump m
      else m
  | Request_rebind { name; new_hash } -> (
      match
        P10_minted_labels_substrate.Namespace.resolve Substrate.global.ns
          name
      with
      | Some old -> { m with pending_rebind = Some (name, old, new_hash) }
      | None -> m)
  | Confirm_rebind -> (
      match m.pending_rebind with
      | Some (name, _old, new_hash) ->
          P10_minted_labels_substrate.Namespace.rebind Substrate.global.ns
            ~name new_hash;
          bump
            {
              m with
              pending_rebind = None;
              view = Detail new_hash;
              author_bind_as = "";
            }
      | None -> m)
  | Cancel_rebind -> { m with pending_rebind = None }
  | Evaluate h ->
      ignore
        (P10_minted_labels_substrate.Eval.eval
           ~store:Substrate.global.store ~att:Substrate.global.att
           ~step_limit:Substrate.global.step_limit h
          : P10_minted_labels_substrate.Eval.result);
      bump m
  | Open_in_editor h -> (
      let store = Substrate.global.store in
      let ns = Substrate.global.ns in
      match P10_minted_labels_substrate.Store.kind_of store h with
      | Some P10_minted_labels_substrate.Definition.Term_kind ->
          (* Pretty-print the term's body. print_named already follows
             Named_term wrappers via Store.reconstruct, so it produces
             the same source whether `h` is a minted wrapper or a
             substructure body. *)
          let src =
            try
              P10_minted_labels_substrate.Pretty.print_named
                ~namespace:ns store h
            with _ -> ""
          in
          let feedback =
            Feedback.compute ~s:Substrate.global ~buffer:src
          in
          bump
            {
              m with
              editor_mode = State.Term_mode;
              author_buffer = src;
              author_bind_as = "";
              feedback;
              view = State.Author;
            }
      | Some P10_minted_labels_substrate.Definition.Type_kind ->
          let src =
            try
              P10_minted_labels_substrate.Pretty.print_named_ty
                ~namespace:ns store h
            with _ -> ""
          in
          let ty_feedback =
            Feedback.compute_ty ~s:Substrate.global ~buffer:src
          in
          bump
            {
              m with
              editor_mode = State.Type_mode;
              author_ty_buffer = src;
              author_ty_bind_as = "";
              ty_feedback;
              view = State.Author;
            }
      | Some P10_minted_labels_substrate.Definition.Label_kind ->
          (* A label has no editable body — leave the state alone. *)
          m
      | None -> m)
  | Set_detail_bind_buffer s -> { m with detail_bind_buffer = s }
  | Detail_bind h ->
      let name = String.strip m.detail_bind_buffer in
      if String.is_empty name then m
      else begin
        try
          P10_minted_labels_substrate.Namespace.bind
            Substrate.global.ns ~name h;
          bump { m with detail_bind_buffer = "" }
        with
        | P10_minted_labels_substrate.Namespace.Name_already_bound _ -> (
            (* Same rebind UX as the editor's bind action: surface the
               conflict so the user can confirm. *)
            match
              P10_minted_labels_substrate.Namespace.resolve
                Substrate.global.ns name
            with
            | Some old ->
                { m with pending_rebind = Some (name, old, h) }
            | None -> m)
        | _ -> m
      end

let render_rebind_dialog
    ~(state : State.t)
    ~(inject : State.action -> unit Vdom.Effect.t) : Vdom.Node.t =
  match state.pending_rebind with
  | None -> Vdom.Node.none
  | Some (name, old, new_hash) ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "rebind-overlay" ]
        [
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "rebind-dialog" ]
            [
              Vdom.Node.h3 [ Vdom.Node.text "rebind?" ];
              Vdom.Node.p
                [
                  Vdom.Node.text
                    (Printf.sprintf
                       "name '%s' is already bound to %s. Rebind to %s?"
                       name
                       (P10_minted_labels_substrate.Hash.short old)
                       (P10_minted_labels_substrate.Hash.short new_hash));
                ];
              Vdom.Node.div
                ~attrs:[ Vdom.Attr.class_ "rebind-buttons" ]
                [
                  Vdom.Node.button
                    ~attrs:
                      [
                        Vdom.Attr.class_ "btn-primary";
                        Vdom.Attr.on_click (fun _ ->
                            inject State.Confirm_rebind);
                      ]
                    [ Vdom.Node.text "rebind" ];
                  Vdom.Node.button
                    ~attrs:
                      [
                        Vdom.Attr.on_click (fun _ ->
                            inject State.Cancel_rebind);
                      ]
                    [ Vdom.Node.text "cancel" ];
                ];
            ];
        ]

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
    let%arr s, _ = state_and_inject in
    s
  in
  let%sub inject =
    let%arr _, f = state_and_inject in
    f
  in
  let%sub browser = Browser.view ~state ~inject in
  let%sub editor = Editor.view ~state ~inject in
  let%sub detail = Detail.view ~state ~inject in
  let%arr state = state
  and inject = inject
  and browser = browser
  and editor = editor
  and detail = detail in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "app" ]
    [
      browser;
      editor;
      detail;
      render_rebind_dialog ~state ~inject;
    ]
