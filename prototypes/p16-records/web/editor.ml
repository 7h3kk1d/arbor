(* Center pane: an indicator of the current editing context (what's open), three
   forms — new type, define a term, evaluate — and a feedback area. The Sealed /
   normal badge on a bind makes minimal sealing visible. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P16_substrate

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
  | State.Typed { ty; value; _ } ->
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
  (* The merged work area: type an expression, see its type + value live, then
     bind it (annotation optional — synthesized if blank). When the expression is
     an existential package, an "open as module" block appears, with one name
     input per abstract type and one per operation field, populated from the
     package's structure (no comma-separated guessing). *)
  let open_shape =
    match state.work_fb with State.Typed { open_shape; _ } -> open_shape | _ -> None
  in
  let nth_or lst i = match List.nth lst i with Some x -> x | None -> "" in
  let open_block (shape : State.open_shape) =
    let field_row ~kind ~ty ~placeholder ~value ~on_input =
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "open-field-row" ]
        [
          Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "open-field-kind" ] [ Vdom.Node.text kind ];
          Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "open-field-type" ] [ Vdom.Node.text ty ];
          text_input ~placeholder ~value ~on_input;
        ]
    in
    let type_rows =
      List.init shape.type_arity ~f:(fun i ->
          let dflt = Pretty.tyvar_name i in
          field_row ~kind:"type" ~ty:dflt ~placeholder:("name (default " ^ dflt ^ ")")
            ~value:(nth_or state.open_type_names i)
            ~on_input:(fun v -> inject (State.Set_open_type_name (i, v))))
    in
    let field_rows =
      List.mapi shape.fields ~f:(fun j (f : State.open_field) ->
          (* a record interface pre-fills the field name from its label (leave it
             blank to bind by that label); a product field has no label *)
          let placeholder =
            match f.flabel with Some n -> n ^ " (from label)" | None -> "field name (optional)"
          in
          field_row ~kind:"op" ~ty:(": " ^ f.fty) ~placeholder
            ~value:(nth_or state.open_field_names j)
            ~on_input:(fun v -> inject (State.Set_open_field_name (j, v))))
    in
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "open-block" ]
      ((Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "detail-note" ]
          [ Vdom.Node.text "open as module — name the abstract types and operations:" ]
       :: type_rows)
      @ field_rows
      @ [
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.class_ "btn-secondary";
                Vdom.Attr.on_click (fun _ -> inject State.Open_existential);
              ]
            [ Vdom.Node.text "open as module" ];
        ])
  in
  let work_section =
    section "work — typecheck & evaluate live, then bind or open"
      ([
         Vdom.Node.textarea
           ~attrs:
             [
               Vdom.Attr.class_ "scratch-area";
               Vdom.Attr.create "spellcheck" "false";
               Vdom.Attr.create "autocomplete" "off";
               Vdom.Attr.create "autocapitalize" "off";
               Vdom.Attr.placeholder {|expr (e.g. Counter.get (bump2 Counter.empty), or mkCounter true)|};
               Vdom.Attr.value_prop state.work_expr;
               Vdom.Attr.on_input (fun _ s ->
                   let fb = Ops.eval ~s:Substrate.global ~open_set:state.open_set ~expr:s in
                   Vdom.Effect.Many
                     [ inject (State.Set_work_expr s); inject (State.Set_work_fb fb) ]);
             ]
           [];
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "scratch-feedback" ]
           [ render_feedback state.work_fb ];
         text_input ~placeholder:"name (e.g. Counter.incr, or a module name to open as)"
           ~value:state.work_name ~on_input:(fun s -> inject (State.Set_work_name s));
         text_input ~placeholder:"type (optional — synthesized if blank)" ~value:state.work_ty
           ~on_input:(fun s -> inject (State.Set_work_ty s));
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "btn-row" ]
           [
             Vdom.Node.button
               ~attrs:[ Vdom.Attr.class_ "btn-primary"; Vdom.Attr.on_click (fun _ -> inject State.Define) ]
               [ Vdom.Node.text "bind" ];
           ];
       ]
      @ (match open_shape with Some shape -> [ open_block shape ] | None -> []))
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "editor-pane" ]
    [
      Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "editor" ];
      open_indicator state.open_set;
      work_section;
      type_section;
      Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-pane" ] [ render_feedback state.feedback ];
    ]
