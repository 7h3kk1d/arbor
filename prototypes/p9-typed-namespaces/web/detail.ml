(* Right-pane detail view for a selected hash. Shows: short hash, all
   names bound to it, name-aware pretty-printed body, and aspect badges. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P9_typed_namespaces_substrate

let render_for_hash
    ~(state : State.t)
    ~(inject : State.action -> unit Vdom.Effect.t) (h : Hash.t) :
    Vdom.Node.t =
  let _ = state in
  let names = Namespace.names_of Substrate.global.ns h in
  let body = Pretty.print_named ~namespace:Substrate.global.ns
               Substrate.global.store h
  in
  let header =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "detail-header" ]
      [
        Vdom.Node.h2
          ~attrs:[ Vdom.Attr.class_ "panel-title" ]
          [ Vdom.Node.text "detail" ];
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
  let aspect_rows =
    Aspects_view.render_aspects ~inject ~att:Substrate.global.att
      ~store:Substrate.global.store ~ns:Substrate.global.ns
      ~filter:state.filter h
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
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "detail-pane" ]
    [
      header;
      names_row;
      body_row;
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "detail-aspects" ]
        aspect_rows;
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "detail-actions" ]
        unbind_buttons;
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
