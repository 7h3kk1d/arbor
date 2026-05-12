(* Right-pane detail view for a selected hash. Shows: short hash, all
   names bound to it, name-aware pretty-printed body, and aspect badges. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P10_minted_labels_substrate

let render_for_hash
    ~(state : State.t)
    ~(inject : State.action -> unit Vdom.Effect.t) (h : Hash.t) :
    Vdom.Node.t =
  let store = Substrate.global.store in
  let ns = Substrate.global.ns in
  let kind = Store.kind_of store h in
  let kind_label =
    match kind with
    | Some Definition.Type_kind -> "type"
    | Some Definition.Term_kind -> "term"
    | Some Definition.Label_kind -> "label"
    | None -> "?"
  in
  let names = Namespace.names_of ns h in
  (* For labels, the "body" is just an opaque mint mark — we render a
     short hex prefix of the mark so it's visually distinct from any
     other definition. For records-and-types, fall back to the
     pretty-printer. *)
  let body =
    match Store.lookup store h with
    | Some (Definition.Label l) ->
        Printf.sprintf "label mint:%s" (Mint.short l.mint)
    | Some (Definition.Named_term (mint, body_h)) ->
        Printf.sprintf "minted term [mint:%s] body:%s"
          (Mint.short mint) (Hash.short body_h)
    | Some (Definition.Named_type (mint, body_h)) ->
        Printf.sprintf "minted type [mint:%s] body:%s"
          (Mint.short mint) (Hash.short body_h)
    | _ ->
        (match kind with
         | Some Definition.Type_kind ->
             Pretty.print_named_ty ~namespace:ns store h
         | _ -> Pretty.print_named ~namespace:ns store h)
  in
  let header =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "detail-header" ]
      [
        Vdom.Node.h2
          ~attrs:[ Vdom.Attr.class_ "panel-title" ]
          [ Vdom.Node.text "detail" ];
        Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "kind-badge" ]
          [ Vdom.Node.text kind_label ];
        Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "mono hash" ]
          [ Vdom.Node.text (Hash.short h) ];
      ]
  in
  let names_row =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "detail-names" ]
      (Vdom.Node.span
         ~attrs:[ Vdom.Attr.class_ "label" ]
         [ Vdom.Node.text "names" ]
       ::
       (if List.is_empty names then
          [ Vdom.Node.span [ Vdom.Node.text " (anonymous)" ] ]
        else
          List.map names ~f:(fun n ->
              Vdom.Node.span
                ~attrs:[ Vdom.Attr.class_ "chip chip-name" ]
                [ Vdom.Node.text n ])))
  in
  let body_row =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "detail-body" ]
      [
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "label" ]
          [ Vdom.Node.text "body" ];
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "mono detail-pretty" ]
          [ Vdom.Node.text body ];
      ]
  in
  (* Type definitions and labels don't participate in typecheck /
     has-holes / eval aspects today; aspect rows are only rendered
     for terms. The aspect query path for terms automatically follows
     Named_term wrappers via Store.unwrap_named, so minted top-level
     bindings still see the aspects of their substructure body. *)
  let aspect_rows =
    match kind with
    | Some Definition.Type_kind
    | Some Definition.Label_kind -> []
    | _ ->
        Aspects_view.render_aspects ~inject ~att:Substrate.global.att
          ~store ~ns ~filter:state.filter h
  in
  let unbind_buttons =
    if List.is_empty names then []
    else
      List.map names ~f:(fun n ->
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.class_ "btn-mini";
                Vdom.Attr.on_click (fun _ ->
                    inject (State.Unbind n));
              ]
            [ Vdom.Node.text ("unbind " ^ n) ])
  in
  (* "Bind another name" row — works for any sort (term, type, label).
     For labels this is the main rebind UX, since labels are only
     auto-bound at the type-declaration site where they're minted.
     For terms / types it lets the user add an alias. If the typed
     name is already bound elsewhere, the rebind confirmation dialog
     opens via the same path as the editor's bind action. *)
  let bind_row =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "detail-bind-row" ]
      [
        Vdom.Node.input
          ~attrs:
            [
              Vdom.Attr.type_ "text";
              Vdom.Attr.placeholder
                (match kind with
                 | Some Definition.Label_kind ->
                     "bind label as name (e.g. coord.x)"
                 | Some Definition.Type_kind ->
                     "bind type as name (e.g. alias.Foo)"
                 | _ -> "bind as name (e.g. math.foo)");
              Vdom.Attr.value_prop state.detail_bind_buffer;
              Vdom.Attr.on_input (fun _ s ->
                  inject (State.Set_detail_bind_buffer s));
              Vdom.Attr.on_keydown (fun ev ->
                  if ev##.keyCode = 13 then inject (State.Detail_bind h)
                  else Vdom.Effect.Ignore);
            ]
          ();
        Vdom.Node.button
          ~attrs:
            [
              Vdom.Attr.classes [ "btn-mini"; "btn-primary" ];
              Vdom.Attr.on_click (fun _ ->
                  inject (State.Detail_bind h));
            ]
          [ Vdom.Node.text "bind" ];
      ]
  in
  (* "Open in editor" — populate the matching editor buffer with the
     pretty-printed source of this definition so the user can modify
     and re-ingest. Hidden for Labels (no body). *)
  let open_button =
    match kind with
    | Some Definition.Label_kind | None -> []
    | _ ->
        [
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.classes [ "btn-mini"; "btn-open-editor" ];
                Vdom.Attr.title
                  "load this definition's source into the editor";
                Vdom.Attr.on_click (fun _ ->
                    inject (State.Open_in_editor h));
              ]
            [ Vdom.Node.text "open in editor" ];
        ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "detail-pane" ]
    [
      header;
      names_row;
      body_row;
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "detail-aspects" ]
        aspect_rows;
      bind_row;
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "detail-actions" ]
        (open_button @ unbind_buttons);
    ]

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state
  and inject = inject in
  match state.view with
  | State.Author ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "detail-pane empty" ]
        [
          Vdom.Node.h2
            ~attrs:[ Vdom.Attr.class_ "panel-title" ]
            [ Vdom.Node.text "detail" ];
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "detail-empty-hint" ]
            [
              Vdom.Node.text
                "click a binding in the namespace tree to see it here.";
            ];
        ]
  | State.Detail h -> render_for_hash ~state ~inject h
