(* Center pane: an indicator of the current editing context (what's open), three
   forms — new type, define a term, evaluate — and a feedback area. The Sealed /
   normal badge on a bind makes minimal sealing visible. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P14_substrate

let text_input ~placeholder ~value ~on_input =
  Vdom.Node.input
    ~attrs:
      [
        Vdom.Attr.type_ "text";
        Vdom.Attr.placeholder placeholder;
        Vdom.Attr.value_prop value;
        Vdom.Attr.create "spellcheck" "false";
        Vdom.Attr.create "autocomplete" "off";
        Vdom.Attr.on_input (fun _ s -> on_input s);
      ]
    ()

let render_feedback (fb : State.feedback) : Vdom.Node.t =
  match fb with
  | State.Empty -> Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-empty" ] [ Vdom.Node.text "—" ]
  | State.Info m -> Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-info" ] [ Vdom.Node.text m ]
  | State.Err m ->
      Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-error" ] [ Vdom.Node.text ("error: " ^ m) ]
  | State.Bound { name; kind; ty; hash } ->
      let kbadge =
        match kind with
        | State.Sealed ->
            Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; "badge-sealed" ] ] [ Vdom.Node.text "SEALED" ]
        | State.Normal ->
            Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; "badge-term" ] ] [ Vdom.Node.text "normal" ]
      in
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-ok" ]
        [
          Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "chip chip-name" ] [ Vdom.Node.text name ];
          Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "feedback-type" ] [ Vdom.Node.text (" : " ^ ty) ];
          kbadge;
          Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "mono hash" ] [ Vdom.Node.text hash ];
        ]
  | State.Typed { ty; value } ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-ok" ]
        [
          Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "feedback-type" ] [ Vdom.Node.text (": " ^ ty) ];
          Vdom.Node.span [ Vdom.Node.text (" = " ^ value) ];
        ]

let open_indicator (open_set : string list) : Vdom.Node.t =
  let s = Substrate.global in
  let label h = match Namespace.name_of s.ns h with Some n -> n | None -> Hash.short h in
  if List.is_empty open_set then
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.classes [ "open-indicator"; "open-default" ] ]
      [ Vdom.Node.text "context: default — abstract types are opaque" ]
  else
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.classes [ "open-indicator"; "open-active" ] ]
      (Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "open-label" ] [ Vdom.Node.text "open:" ]
      :: List.map open_set ~f:(fun h ->
             Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "chip"; "chip-open" ] ] [ Vdom.Node.text (label h) ]))

let section title body =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "editor-section" ]
    (Vdom.Node.h3 ~attrs:[ Vdom.Attr.class_ "section-title" ] [ Vdom.Node.text title ] :: body)

let view ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state and inject = inject in
  let type_section =
    section "new type"
      [
        text_input ~placeholder:"name (e.g. Counter.t)" ~value:state.ty_name
          ~on_input:(fun s -> inject (State.Set_ty_name s));
        text_input ~placeholder:"witness / body (e.g. Int, or Int * Int)" ~value:state.ty_body
          ~on_input:(fun s -> inject (State.Set_ty_body s));
        Vdom.Node.label
          ~attrs:[ Vdom.Attr.class_ "checkbox-label" ]
          [
            Vdom.Node.input
              ~attrs:
                [
                  Vdom.Attr.type_ "checkbox";
                  (if state.ty_abstract then Vdom.Attr.checked else Vdom.Attr.empty);
                  Vdom.Attr.on_click (fun _ -> inject State.Toggle_ty_abstract);
                ]
              ();
            Vdom.Node.text " abstract (mint an opaque type; auto-opens it)";
          ];
        Vdom.Node.button
          ~attrs:[ Vdom.Attr.class_ "btn-primary"; Vdom.Attr.on_click (fun _ -> inject State.Create_type) ]
          [ Vdom.Node.text "create" ];
      ]
  in
  let define_section =
    section "define a term"
      [
        text_input ~placeholder:"name (e.g. Counter.incr)" ~value:state.def_name
          ~on_input:(fun s -> inject (State.Set_def_name s));
        text_input ~placeholder:"type (e.g. Counter.t -> Counter.t)" ~value:state.def_ty
          ~on_input:(fun s -> inject (State.Set_def_ty s));
        text_input ~placeholder:{|expr (e.g. \x: Counter.t. x + 1)|} ~value:state.def_expr
          ~on_input:(fun s -> inject (State.Set_def_expr s));
        Vdom.Node.button
          ~attrs:[ Vdom.Attr.class_ "btn-primary"; Vdom.Attr.on_click (fun _ -> inject State.Define) ]
          [ Vdom.Node.text "bind" ];
      ]
  in
  let scratch_section =
    section "scratch — typecheck & evaluate live (no binding)"
      [
        Vdom.Node.textarea
          ~attrs:
            [
              Vdom.Attr.class_ "scratch-area";
              Vdom.Attr.create "spellcheck" "false";
              Vdom.Attr.create "autocomplete" "off";
              Vdom.Attr.create "autocapitalize" "off";
              Vdom.Attr.placeholder {|e.g. Counter.get (bump2 Counter.empty)|};
              Vdom.Attr.value_prop state.eval_expr;
              Vdom.Attr.on_input (fun _ s ->
                  let fb =
                    Ops.eval ~s:Substrate.global ~open_set:state.open_set ~expr:s
                  in
                  Vdom.Effect.Many
                    [ inject (State.Set_eval_expr s); inject (State.Set_scratch_fb fb) ]);
            ]
          [];
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "scratch-feedback" ]
          [ render_feedback state.scratch_fb ];
      ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "editor-pane" ]
    [
      Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "editor" ];
      open_indicator state.open_set;
      scratch_section;
      type_section;
      define_section;
      Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-pane" ] [ render_feedback state.feedback ];
    ]
