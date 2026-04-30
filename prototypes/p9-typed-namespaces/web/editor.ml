(* Editor pane: textarea, recovered-AST panel below, then feedback +
   bind row. On every keystroke, calls Feedback.compute synchronously
   to ingest into the substrate. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P9_typed_namespaces_substrate

let render_resolved (items : (string * Hash.t) list) : Vdom.Node.t list =
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
    ~(filter : State.filter)
    (fb : State.ingest_result) : Vdom.Node.t =
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
                "every parse succeeds (errors become holes). \
                 Well-typed terms (or terms whose holes don't break \
                 type discipline) ingest into the Store immediately. \
                 Bind a name with the box below.";
            ];
        ]
  | Recovered { ingest; _ } -> (
      match ingest with
      | Ingested { hash; was_new; type_summary; has_holes; resolved } ->
          let status_label =
            if was_new then "new" else "already in store"
          in
          let status_class =
            if was_new then "feedback-new" else "feedback-cached"
          in
          let names = Namespace.names_of Substrate.global.ns hash in
          let name_chips =
            List.map names ~f:(fun n ->
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "chip chip-name" ]
                  [ Vdom.Node.text n ])
          in
          let aspect_rows =
            Aspects_view.render_aspects ~inject
              ~att:Substrate.global.att ~store:Substrate.global.store
              ~ns:Substrate.global.ns ~filter hash
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
                   Vdom.Node.span
                     ~attrs:[ Vdom.Attr.class_ "feedback-type" ]
                     [ Vdom.Node.text (": " ^ type_summary) ];
                 ]
                @ name_chips
                @ if has_holes then
                    [
                      Vdom.Node.span
                        ~attrs:[ Vdom.Attr.class_ "feedback-holes-tag" ]
                        [ Vdom.Node.text "◌ holes" ];
                    ]
                  else []);
              Vdom.Node.div
                ~attrs:[ Vdom.Attr.class_ "feedback-line" ]
                [
                  Vdom.Node.span
                    ~attrs:[ Vdom.Attr.class_ "label" ]
                    [ Vdom.Node.text "resolved" ];
                  Vdom.Node.div
                    ~attrs:[ Vdom.Attr.class_ "resolved-list" ]
                    (render_resolved resolved);
                ];
              Vdom.Node.div
                ~attrs:[ Vdom.Attr.class_ "feedback-aspects" ]
                aspect_rows;
            ]
      | Resolve_unbound name ->
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
            [ Vdom.Node.text ("unbound name: " ^ name) ]
      | Resolve_ambiguous { name; candidates } ->
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
            [
              Vdom.Node.text
                (Printf.sprintf "ambiguous name '%s' — could be: %s"
                   name
                   (String.concat ~sep:", " candidates));
            ]
      | Resolve_missing_hash (n, h) ->
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
            [
              Vdom.Node.text
                (Printf.sprintf "name '%s' resolves to missing hash %s"
                   n (Hash.short h));
            ]
      | Type_error msg ->
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
            [ Vdom.Node.text ("type error: " ^ msg) ])

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%sub recovered = Recovered_view.view ~state ~inject in
  let%arr state = state
  and inject = inject
  and recovered = recovered in
  let bind_disabled =
    match state.feedback with
    | State.Recovered { ingest = Ingested _; _ } -> false
    | _ -> true
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "editor-pane" ]
    [
      Vdom.Node.h2
        ~attrs:[ Vdom.Attr.class_ "panel-title" ]
        [ Vdom.Node.text "editor" ];
      Vdom.Node.textarea
        ~attrs:
          [
            Vdom.Attr.class_ "editor-textarea";
            Vdom.Attr.create "spellcheck" "false";
            Vdom.Attr.create "autocapitalize" "off";
            Vdom.Attr.create "autocomplete" "off";
            Vdom.Attr.create "autofocus" "true";
            Vdom.Attr.placeholder
              "e.g.  let inc = \\x: Int. x + 1 in inc 41";
            Vdom.Attr.value state.author_buffer;
            Vdom.Attr.on_keydown (fun ev ->
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
                let fb = Feedback.compute ~s:Substrate.global ~buffer:s in
                let base =
                  [
                    inject (State.Set_author_buffer s);
                    inject (State.Feedback_updated fb);
                  ]
                in
                let extras =
                  match fb with
                  | State.Recovered { ingest = Ingested { hash; _ }; _ }
                    when state.auto_eval ->
                      [ inject (State.Evaluate hash) ]
                  | _ -> []
                in
                Vdom.Effect.Many (base @ extras));
          ]
        [];
      recovered;
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "feedback-pane" ]
        [ render_feedback ~inject ~filter:state.filter state.feedback ];
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
                        inject
                          (State.Set_auto_eval (not state.auto_eval)));
                  ]
                ();
              Vdom.Node.text " auto-evaluate";
            ];
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "bind-row" ]
        [
          Vdom.Node.input
            ~attrs:
              [
                Vdom.Attr.type_ "text";
                Vdom.Attr.placeholder "bind as name (e.g. math.foo)";
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
