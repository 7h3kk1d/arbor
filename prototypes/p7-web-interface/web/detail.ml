(* Right-column Detail view for a selected hash. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P7_web_interface_substrate

let lang_label = function `Lc -> "lc" | `Stlc -> "stlc"
let lang_class = function `Lc -> "tag-lc" | `Stlc -> "tag-stlc"

let render_name_chip
    ~(inject : State.action -> unit Vdom.Effect.t)
    (n : string) : Vdom.Node.t =
  Vdom.Node.span
    ~attrs:[ Vdom.Attr.class_ "chip chip-name" ]
    [
      Vdom.Node.text n;
      Vdom.Node.span
        ~attrs:
          [
            Vdom.Attr.class_ "chip-x";
            Vdom.Attr.on_click (fun _ -> inject (State.Unbind n));
          ]
        [ Vdom.Node.text "×" ];
    ]

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state
  and inject = inject in
  match state.view with
  | State.Author -> Vdom.Node.none
  | State.Detail h ->
      let store = Substrate.global.store in
      let ns = Substrate.global.ns in
      let att = Substrate.global.att in
      if not (Store.has store h) then
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "detail" ]
          [ Vdom.Node.text "(hash not in store)" ]
      else
        let lang = Aspects_view.lang_of_hash store h in
        let body_node = Child_chips.view ~ns ~store ~inject ~lang h in
        let names = Namespace.names_of ns h in
        let back_button =
          let has_back = not (List.is_empty state.nav_back) in
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.class_ "btn-secondary";
                Vdom.Attr.on_click (fun _ ->
                    if has_back then inject State.Go_back
                    else inject (State.Set_view Author));
              ]
            [ Vdom.Node.text (if has_back then "← back" else "← author") ]
        in
        let bind_controls =
          let bind_id = Printf.sprintf "detail-bind-%s" (Hash.short h) in
          [
            Vdom.Node.input
              ~attrs:
                [
                  Vdom.Attr.type_ "text";
                  Vdom.Attr.placeholder "bind as name…";
                  Vdom.Attr.class_ "bind-input";
                  Vdom.Attr.id bind_id;
                ]
              ();
            Vdom.Node.button
              ~attrs:
                [
                  Vdom.Attr.class_ "btn-action";
                  Vdom.Attr.on_click (fun _ ->
                      match
                        Js_of_ocaml.Dom_html.getElementById_coerce bind_id
                          Js_of_ocaml.Dom_html.CoerceTo.input
                      with
                      | Some e ->
                          let name =
                            String.strip (Js_of_ocaml.Js.to_string e##.value)
                          in
                          if String.is_empty name then Vdom.Effect.Ignore
                          else
                            let already =
                              Namespace.resolve Substrate.global.ns name
                            in
                            (match already with
                             | Some _old ->
                                 inject
                                   (State.Request_rebind
                                      { name; new_hash = h })
                             | None ->
                                 inject
                                   (State.Bind_existing { name; hash = h }))
                      | None -> Vdom.Effect.Ignore);
                ]
              [ Vdom.Node.text "bind/rebind" ];
          ]
        in
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "detail" ]
          [
            Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "detail-header" ]
              [
                back_button;
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.classes [ "chip"; lang_class lang ] ]
                  [ Vdom.Node.text (lang_label lang) ];
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "mono hash" ]
                  [ Vdom.Node.text (Hash.short h) ];
                Vdom.Node.div
                  ~attrs:[ Vdom.Attr.class_ "names" ]
                  (List.map names ~f:(render_name_chip ~inject));
              ];
            Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "detail-hash-full" ]
              [
                Vdom.Node.span
                  ~attrs:
                    [
                      Vdom.Attr.classes
                        [ "mono hash-full"; "clickable" ];
                      Vdom.Attr.title "click to copy full hash";
                      Vdom.Attr.on_click (fun _ ->
                          let full = Hash.to_string h in
                          (try
                             let nav =
                               Js_of_ocaml.Js.Unsafe.coerce
                                 Js_of_ocaml.Dom_html.window##.navigator
                             in
                             let clip = nav##.clipboard in
                             ignore
                               (clip##writeText
                                  (Js_of_ocaml.Js.string full))
                           with _ -> ());
                          Vdom.Effect.Ignore);
                    ]
                  [ Vdom.Node.text (Hash.to_string h) ];
              ];
            Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "detail-body" ]
              [ body_node ];
            Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "aspects-panel" ]
              (Vdom.Node.h3 [ Vdom.Node.text "aspects" ]
               :: Aspects_view.render_aspects ~inject ~att ~store ~ns ~lang h);
            Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "actions-bar" ]
              (Aspects_view.render_actions ~inject ~att ~lang ~id_ns:"detail" h);
            Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "bind-row" ] bind_controls;
          ]
