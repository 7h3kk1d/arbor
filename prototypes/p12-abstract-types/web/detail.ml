(* Right pane: the selected definition. For an abstract type, its derived
   implementation set (the sealed ops that open it) — never its witness exposed
   as a usable equation. For a sealed op, what it opens. For a term, its type
   and a best-effort evaluated value. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P12_substrate

let label_of h =
  match Namespace.name_of Substrate.global.ns h with
  | Some n -> n
  | None -> Hash.short h

let view ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state and inject = inject in
  let s = Substrate.global in
  let body =
    match state.selected with
    | None ->
        [ Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "detail-empty" ] [ Vdom.Node.text "select a definition" ] ]
    | Some h ->
        let name_chips =
          List.map (Namespace.names_of s.ns h) ~f:(fun n ->
              Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "chip chip-name" ] [ Vdom.Node.text n ])
        in
        let header =
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "detail-header" ]
            (Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "mono hash" ] [ Vdom.Node.text (Hash.short h) ]
            :: name_chips)
        in
        let rows =
          match Store.find s.store h with
          | None -> [ Vdom.Node.text "dangling hash" ]
          | Some (Definition.Type (Tnode.Opaque { mint; witness })) ->
              let impl = Store.impl_set s.store h in
              [
                Vdom.Node.div [ Vdom.Node.text (Printf.sprintf "abstract type — opaque(%s)" (Mint.short mint)) ];
                Vdom.Node.div
                  ~attrs:[ Vdom.Attr.class_ "detail-note" ]
                  [ Vdom.Node.text (Printf.sprintf "witness: %s (hidden from consumers)" (Pretty.ty ~ns:s.ns ~st:s.store witness)) ];
                Vdom.Node.h3
                  ~attrs:[ Vdom.Attr.class_ "section-title" ]
                  [ Vdom.Node.text (Printf.sprintf "implementation set (%d)" (List.length impl)) ];
                Vdom.Node.div
                  ~attrs:[ Vdom.Attr.class_ "impl-list" ]
                  (List.map impl ~f:(fun o ->
                       let ty =
                         match Store.type_of s.store o with
                         | Some t -> Pretty.ty ~ns:s.ns ~st:s.store t
                         | None -> "?"
                       in
                       Vdom.Node.div
                         ~attrs:
                           [
                             Vdom.Attr.classes [ "impl-row"; "clickable" ];
                             Vdom.Attr.on_click (fun _ -> inject (State.Select o));
                           ]
                         [
                           Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "chip chip-name" ] [ Vdom.Node.text (label_of o) ];
                           Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "feedback-type" ] [ Vdom.Node.text (" : " ^ ty) ];
                         ]));
              ]
          | Some (Definition.Type _) ->
              [ Vdom.Node.div [ Vdom.Node.text ("concrete type = " ^ Pretty.ty ~ns:s.ns ~st:s.store h) ] ]
          | Some (Definition.Term node) ->
              let ty =
                match Store.type_of s.store h with
                | Some t -> Pretty.ty ~ns:s.ns ~st:s.store t
                | None -> "?"
              in
              let kind_row =
                match node with
                | Node.Seal { opens; _ } ->
                    Vdom.Node.div
                      ~attrs:[ Vdom.Attr.class_ "detail-note" ]
                      [ Vdom.Node.text ("sealed — opens " ^ String.concat ~sep:", " (List.map opens ~f:label_of)) ]
                | _ -> Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "detail-note" ] [ Vdom.Node.text "ordinary term" ]
              in
              let source_block label src =
                Vdom.Node.div
                  ~attrs:[ Vdom.Attr.class_ "detail-source" ]
                  [
                    Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "detail-note" ] [ Vdom.Node.text label ];
                    Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "source" ] [ Vdom.Node.text src ];
                  ]
              in
              let source_row =
                match node with
                | Node.Seal { impl; _ } -> (
                    match Store.find s.store impl with
                    | Some (Definition.Term inode) ->
                        source_block "implementation (over the witness):"
                          (Pretty.term ~ns:s.ns ~st:s.store inode)
                    | _ -> Vdom.Node.none)
                | _ -> source_block "definition:" (Pretty.term ~ns:s.ns ~st:s.store node)
              in
              let eval_row =
                match Eval.eval_top s.store (Node.Ref h) with
                | Ok v -> Vdom.Node.div [ Vdom.Node.text ("eval: " ^ Eval.to_string v) ]
                | Error _ -> Vdom.Node.none
              in
              [ Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-type" ] [ Vdom.Node.text (": " ^ ty) ]; kind_row; source_row; eval_row ]
        in
        [ header; Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "detail-body" ] rows ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "detail-pane" ]
    (Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "detail" ] :: body)
