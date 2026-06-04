(* Left pane: the namespace, one row per binding. Abstract types carry an
   "open for edit" / "close" toggle (browser-driven editing context); the open
   ones are highlighted. Clicking a name opens it in the detail pane. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P12_substrate

let badge cls txt =
  Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; cls ] ] [ Vdom.Node.text txt ]

let row ~(inject : State.action -> unit Vdom.Effect.t) ~(open_set : string list)
    ~(selected : string option) (name : string) (h : Hash.t) : Vdom.Node.t =
  let s = Substrate.global in
  let is_open = List.mem open_set h ~equal:String.equal in
  let kind_badge, toggle =
    match Store.find s.store h with
    | Some (Definition.Type (Tnode.Opaque _)) ->
        ( badge "badge-abstract" "abstract",
          [
            Vdom.Node.button
              ~attrs:
                [
                  Vdom.Attr.classes
                    [ "btn-mini"; (if is_open then "btn-open-on" else "") ];
                  Vdom.Attr.on_click (fun _ -> inject (State.Toggle_open h));
                ]
              [ Vdom.Node.text (if is_open then "close" else "open for edit") ];
          ] )
    | Some (Definition.Type _) -> (badge "badge-type" "type", [])
    | Some (Definition.Term (Node.Seal _)) -> (badge "badge-sealed" "sealed", [])
    | Some (Definition.Term _) -> (badge "badge-term" "term", [])
    | None -> (badge "badge-term" "?", [])
  in
  let selected_cls =
    match selected with Some sel when String.equal sel h -> "row-selected" | _ -> ""
  in
  Vdom.Node.div
    ~attrs:
      [
        Vdom.Attr.classes
          [ "browser-row"; selected_cls; (if is_open then "row-open" else "") ];
      ]
    ([
       Vdom.Node.span
         ~attrs:
           [
             Vdom.Attr.classes [ "browser-name"; "clickable" ];
             Vdom.Attr.on_click (fun _ -> inject (State.Select h));
           ]
         [ Vdom.Node.text name ];
       kind_badge;
       Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "mono hash" ] [ Vdom.Node.text (Hash.short h) ];
     ]
    @ toggle)

let view ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state and inject = inject in
  let s = Substrate.global in
  let _ = state.version in
  let entries = Namespace.entries s.ns in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "browser-pane" ]
    (Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "namespace" ]
    :: List.map entries ~f:(fun (name, h) ->
           row ~inject ~open_set:state.open_set ~selected:state.selected name h))
