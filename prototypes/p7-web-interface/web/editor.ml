(* Author pane: language toggle, textarea, live feedback, bind input.
   On every keystroke: run Feedback.compute (which parses, resolves,
   canonicalizes, and ingests into the real Store) synchronously and
   dispatch both Set_author_buffer and Feedback_updated. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P7_web_interface_substrate

let render_resolved_names (items : (string * Hash.t) list) : Vdom.Node.t list =
  if List.is_empty items then [ Vdom.Node.text "(none)" ]
  else
    List.map items ~f:(fun (n, h) ->
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "resolved-item" ]
          [
            Vdom.Node.span
              ~attrs:[ Vdom.Attr.class_ "chip chip-name" ]
              [ Vdom.Node.text n ];
            Vdom.Node.span [ Vdom.Node.text " → " ];
            Vdom.Node.span
              ~attrs:[ Vdom.Attr.class_ "mono hash" ]
              [ Vdom.Node.text (Hash.short h) ];
          ])

let render_feedback
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(lang : State.author_lang)
    (fb : State.feedback) : Vdom.Node.t =
  let lang_tag : [ `Lc | `Stlc ] =
    match lang with Lc -> `Lc | Stlc -> `Stlc
  in
  match fb with
  | Empty ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-empty" ]
        [
          Vdom.Node.div [ Vdom.Node.text "— start typing —" ];
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-hint" ]
            [
              Vdom.Node.text
                "Parseable intermediates ingest into the Store \
                 immediately, unnamed. Bind a name to publish them. \
                 Toggle auto-evaluate/auto-typecheck to run procedures \
                 live.";
            ];
        ]
  | Parse_error msg ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
        [ Vdom.Node.text ("parse error: " ^ msg) ]
  | Resolve_error msg ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
        [ Vdom.Node.text msg ]
  | Ok_ { hash; was_new; typ_str = _; resolved } ->
      let status_label = if was_new then "new" else "already in store" in
      let status_class =
        if was_new then "feedback-new" else "feedback-cached"
      in
      let existing_names =
        Namespace.names_of Substrate.global.ns hash
      in
      let name_chips =
        List.map existing_names ~f:(fun n ->
            Vdom.Node.span
              ~attrs:[ Vdom.Attr.class_ "chip chip-name" ]
              [ Vdom.Node.text n ])
      in
      let aspects_rows =
        Aspects_view.render_aspects ~inject ~att:Substrate.global.att
          ~store:Substrate.global.store ~ns:Substrate.global.ns
          ~lang:lang_tag hash
      in
      let action_buttons =
        Aspects_view.render_actions ~inject ~att:Substrate.global.att
          ~lang:lang_tag ~id_ns:"author" hash
      in
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-ok" ]
        [
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-line" ]
            ([
               Vdom.Node.span
                 ~attrs:[ Vdom.Attr.class_ "label" ]
                 [ Vdom.Node.text "hash" ];
               Vdom.Node.span
                 ~attrs:
                   [
                     Vdom.Attr.classes [ "mono hash"; "clickable" ];
                     Vdom.Attr.title "open in detail";
                     Vdom.Attr.on_click (fun _ ->
                         inject (State.Set_view (Detail hash)));
                   ]
                 [ Vdom.Node.text (Hash.short hash) ];
               Vdom.Node.span
                 ~attrs:[ Vdom.Attr.class_ status_class ]
                 [ Vdom.Node.text ("· " ^ status_label) ];
             ]
             @ name_chips);
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-line" ]
            [
              Vdom.Node.span
                ~attrs:[ Vdom.Attr.class_ "label" ]
                [ Vdom.Node.text "resolved" ];
              Vdom.Node.div
                ~attrs:[ Vdom.Attr.class_ "resolved-list" ]
                (render_resolved_names resolved);
            ];
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-aspects" ]
            aspects_rows;
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-actions" ]
            action_buttons;
        ]

let lang_button
    ~(current : State.author_lang)
    ~(target : State.author_lang)
    ~(label : string)
    ~(inject : State.action -> unit Vdom.Effect.t) : Vdom.Node.t =
  let selected = State.equal_author_lang current target in
  Vdom.Node.button
    ~attrs:
      [
        Vdom.Attr.classes
          [ "btn-filter"; (if selected then "btn-selected" else "") ];
        Vdom.Attr.on_click (fun _ -> inject (State.Set_author_lang target));
      ]
    [ Vdom.Node.text label ]

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state
  and inject = inject in
  let bind_disabled =
    match state.feedback with State.Ok_ _ -> false | _ -> true
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "author" ]
    [
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "author-header" ]
        [
          Vdom.Node.h2 [ Vdom.Node.text "author" ];
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "lang-toggle" ]
            [
              lang_button ~current:state.author_lang ~target:Lc ~label:"lc"
                ~inject;
              lang_button ~current:state.author_lang ~target:Stlc
                ~label:"stlc" ~inject;
            ];
        ];
      Vdom.Node.textarea
        ~attrs:
          [
            Vdom.Attr.class_ "editor-textarea";
            Vdom.Attr.create "spellcheck" "false";
            Vdom.Attr.create "autocapitalize" "off";
            Vdom.Attr.create "autocomplete" "off";
            Vdom.Attr.create "autofocus" "true";
            Vdom.Attr.placeholder
              (match state.author_lang with
               | Lc -> "e.g.  \\x. x"
               | Stlc -> "e.g.  \\x:Bool. if x then false else true");
            Vdom.Attr.value state.author_buffer;
            Vdom.Attr.on_keydown (fun ev ->
                (* Cmd+Enter / Ctrl+Enter dispatches Bind_current when
                   there is a valid feedback and a name to bind. *)
                let is_enter = ev##.keyCode = 13 in
                let meta =
                  Js_of_ocaml.Js.to_bool ev##.metaKey
                  || Js_of_ocaml.Js.to_bool ev##.ctrlKey
                in
                if is_enter && meta then (
                  Js_of_ocaml.Dom.preventDefault ev;
                  inject State.Bind_current)
                else Vdom.Effect.Ignore);
            Vdom.Attr.on_input (fun _ s ->
                let fb =
                  Feedback.compute ~s:Substrate.global
                    ~lang:state.author_lang ~buffer:s
                in
                let base =
                  [
                    inject (State.Set_author_buffer s);
                    inject (State.Feedback_updated fb);
                  ]
                in
                let extras =
                  match fb with
                  | State.Ok_ { hash; _ } ->
                      let tc =
                        if state.auto_typecheck
                           && State.equal_author_lang state.author_lang
                                State.Stlc
                        then [ inject (State.Typecheck hash) ]
                        else []
                      in
                      let ev =
                        if state.auto_eval then
                          [ inject (State.Evaluate hash) ]
                        else []
                      in
                      tc @ ev
                  | _ -> []
                in
                Vdom.Effect.Many (base @ extras));
          ]
        [];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "auto-toggles" ]
        [
          Vdom.Node.label
            ~attrs:[ Vdom.Attr.class_ "auto-toggle" ]
            [
              Vdom.Node.input
                ~attrs:
                  [
                    Vdom.Attr.type_ "checkbox";
                    (if state.auto_eval then Vdom.Attr.checked
                     else Vdom.Attr.empty);
                    Vdom.Attr.on_click (fun _ ->
                        inject (State.Set_auto_eval (not state.auto_eval)));
                  ]
                ();
              Vdom.Node.text " auto-evaluate";
            ];
          (match state.author_lang with
           | Stlc ->
               Vdom.Node.label
                 ~attrs:[ Vdom.Attr.class_ "auto-toggle" ]
                 [
                   Vdom.Node.input
                     ~attrs:
                       [
                         Vdom.Attr.type_ "checkbox";
                         (if state.auto_typecheck then Vdom.Attr.checked
                          else Vdom.Attr.empty);
                         Vdom.Attr.on_click (fun _ ->
                             inject
                               (State.Set_auto_typecheck
                                  (not state.auto_typecheck)));
                       ]
                     ();
                   Vdom.Node.text " auto-typecheck";
                 ]
           | Lc -> Vdom.Node.none);
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-pane" ]
        [ render_feedback ~inject ~lang:state.author_lang state.feedback ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "bind-row" ]
        [
          Vdom.Node.input
            ~attrs:
              [
                Vdom.Attr.type_ "text";
                Vdom.Attr.placeholder "bind as name…";
                Vdom.Attr.value state.author_bind_as;
                Vdom.Attr.on_input (fun _ s ->
                    inject (State.Set_bind_as s));
                Vdom.Attr.on_keydown (fun ev ->
                    if ev##.keyCode = 13 then inject State.Bind_current
                    else Vdom.Effect.Ignore);
              ]
            ();
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.classes [ "btn-primary" ];
                (if bind_disabled then Vdom.Attr.disabled
                 else Vdom.Attr.empty);
                Vdom.Attr.on_click (fun _ -> inject State.Bind_current);
              ]
            [ Vdom.Node.text "bind" ];
        ];
    ]
