(* Shared rendering for a hash's aspects panel and action buttons.
   Used by both Detail (full view) and Editor (inline in feedback pane).

   Aspects shown: type (stlc), eval cache, translations.
   Actions shown: Evaluate, Typecheck (stlc only), Translate / → check. *)

open! Core
open! Bonsai_web
open P7_web_interface_substrate

let lang_of_hash store h : [ `Lc | `Stlc ] =
  match Pretty.language_tag_of_hash store h with
  | tag when String.is_substring tag ~substring:"stlc" -> `Stlc
  | _ -> `Lc

let linkify_hash ~(inject : State.action -> unit Vdom.Effect.t) (h : Hash.t) :
    Vdom.Node.t =
  Vdom.Node.span
    ~attrs:
      [
        Vdom.Attr.classes [ "chip chip-hash"; "clickable" ];
        Vdom.Attr.on_click (fun _ -> inject (State.Set_view (Detail h)));
      ]
    [ Vdom.Node.text (Hash.short h) ]

(* Preferred label for a hash: first bound name, else one-line pretty
   body truncated to ~48 chars. *)
let preferred_label ~(store : Store.t) ~(ns : Namespace.t) (h : Hash.t) :
    string =
  match Namespace.names_of ns h with
  | n :: _ -> n
  | [] -> (
      try
        let s = Pretty.print_named ~namespace:ns store h in
        let s =
          String.map s ~f:(fun c ->
              match c with '\n' | '\r' | '\t' -> ' ' | _ -> c)
        in
        if String.length s > 48 then String.prefix s 48 ^ "…" else s
      with _ -> "?")

(* A clickable body chip that combines a label (name or pretty expr) with
   the short hash. Used for eval results and translation targets so the
   user sees *what* the result is at a glance, not just its hash. *)
let body_chip_for_hash
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    (h : Hash.t) : Vdom.Node.t =
  let label = preferred_label ~store ~ns h in
  let is_named = match Namespace.names_of ns h with [] -> false | _ -> true in
  Vdom.Node.span
    ~attrs:
      [
        Vdom.Attr.classes [ "body-chip"; "clickable" ];
        Vdom.Attr.title "open in detail";
        Vdom.Attr.on_click (fun _ -> inject (State.Set_view (Detail h)));
      ]
    [
      Vdom.Node.span
        ~attrs:
          [
            Vdom.Attr.classes
              [ (if is_named then "chip-name-inline" else "mono body-chip-body") ];
          ]
        [ Vdom.Node.text label ];
      Vdom.Node.text " ";
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "mono hash body-chip-hash" ]
        [ Vdom.Node.text (Hash.short h) ];
    ]

let aspect_value_label (v : Attachment.aspect_value) : string =
  match v with
  | Eval_value h -> Printf.sprintf "Value(%s)" (Hash.short h)
  | Eval_stuck h -> Printf.sprintf "Stuck(%s)" (Hash.short h)
  | Eval_step_limit h -> Printf.sprintf "StepLimit(%s)" (Hash.short h)
  | Translation_target h -> Printf.sprintf "→ %s" (Hash.short h)
  | Translation_untypable msg -> Printf.sprintf "untypable: %s" msg
  | Type_of ty -> Ty.print ty

(* A single row in the aspects panel. *)
let row ~label ~body : Vdom.Node.t =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
    ([
       Vdom.Node.span
         ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
         [ Vdom.Node.text label ];
     ]
     @ body)

let render_aspects
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(att : Attachment.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    ~(lang : [ `Lc | `Stlc ])
    (h : Hash.t) : Vdom.Node.t list =
  let rows = ref [] in
  let add n = rows := n :: !rows in
  (* Type (stlc only). The type chip is clickable — clicking pushes
     "filter by this type" onto the browser list. *)
  (match lang with
   | `Stlc -> (
       match
         Attachment.peek att ~target:h ~aspect:"stlc:type-check"
           ~procedure:"stlc:type-check:v1"
       with
       | Some (Type_of ty) ->
           let ty_str = Ty.print ty in
           add
             (row ~label:"type"
                ~body:
                  [
                    Vdom.Node.span
                      ~attrs:
                        [
                          Vdom.Attr.classes [ "mono"; "type-chip"; "clickable" ];
                          Vdom.Attr.title "filter browser by this type";
                          Vdom.Attr.on_click (fun _ ->
                              inject (State.Filter_by_type (Some ty_str)));
                        ]
                      [ Vdom.Node.text ty_str ];
                  ])
       | _ -> ())
   | `Lc -> ());
  (* Eval *)
  let eval_aspect = match lang with `Lc -> "lc:eval" | `Stlc -> "stlc:eval" in
  let eval_proc = match lang with `Lc -> "lc:eval:v1" | `Stlc -> "stlc:eval:v1" in
  (match Attachment.peek att ~target:h ~aspect:eval_aspect ~procedure:eval_proc with
   | Some v ->
       let result_node =
         match v with
         | Eval_value target ->
             Vdom.Node.span
               [
                 Vdom.Node.span
                   ~attrs:[ Vdom.Attr.class_ "eval-kind" ]
                   [ Vdom.Node.text "Value " ];
                 body_chip_for_hash ~inject ~store ~ns target;
               ]
         | Eval_stuck target ->
             Vdom.Node.span
               [
                 Vdom.Node.span
                   ~attrs:[ Vdom.Attr.class_ "eval-kind" ]
                   [ Vdom.Node.text "Stuck " ];
                 body_chip_for_hash ~inject ~store ~ns target;
               ]
         | Eval_step_limit target ->
             Vdom.Node.span
               [
                 Vdom.Node.span
                   ~attrs:[ Vdom.Attr.class_ "eval-kind" ]
                   [ Vdom.Node.text "StepLimit " ];
                 body_chip_for_hash ~inject ~store ~ns target;
               ]
         | _ ->
             Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "mono" ]
               [ Vdom.Node.text (aspect_value_label v) ]
       in
       add
         (row ~label:"eval"
            ~body:
              [
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "mono aspect-proc" ]
                  [ Vdom.Node.text eval_proc ];
                Vdom.Node.text " → ";
                result_node;
              ])
   | None -> ());
  (* Translations *)
  (match lang with
   | `Stlc -> (
       match Stlc_to_lc_erase_church.peek_translation att h with
       | Some target ->
           add
             (row ~label:"→ lc"
                ~body:
                  [
                    Vdom.Node.span
                      ~attrs:[ Vdom.Attr.class_ "mono aspect-proc" ]
                      [ Vdom.Node.text "stlc-to-lc:erase-church:v1" ];
                    Vdom.Node.text " → ";
                    body_chip_for_hash ~inject ~store ~ns target;
                  ])
       | None -> ())
   | `Lc ->
       let mine =
         Lc_to_stlc_check.all_entries att
         |> List.filter ~f:(fun (src, _p, _v) -> Hash.equal src h)
       in
       List.iter mine ~f:(fun (_src, proc, v) ->
           let value_node =
             match v with
             | Translation_target target ->
                 body_chip_for_hash ~inject ~store ~ns target
             | Translation_untypable msg ->
                 Vdom.Node.span
                   ~attrs:[ Vdom.Attr.class_ "feedback-error" ]
                   [ Vdom.Node.text msg ]
             | _ ->
                 Vdom.Node.span
                   ~attrs:[ Vdom.Attr.class_ "mono" ]
                   [ Vdom.Node.text (aspect_value_label v) ]
           in
           add
             (row ~label:"→ stlc"
                ~body:
                  [
                    Vdom.Node.span
                      ~attrs:[ Vdom.Attr.class_ "mono aspect-proc" ]
                      [ Vdom.Node.text proc ];
                    Vdom.Node.text " → ";
                    value_node;
                  ])));
  List.rev !rows

(* Action buttons. For lc, a type-input is inlined because the translate
   procedure takes an expected type; DOM id is unique per hash to avoid
   collisions when the buttons are rendered in multiple places.

   Buttons for already-cached procedures (eval, typecheck) are hidden —
   re-running a cached derived aspect is a no-op, so the button would
   lie about being an action. *)
let render_actions
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(att : Attachment.t)
    ~(lang : [ `Lc | `Stlc ])
    ~(id_ns : string)
    (h : Hash.t) : Vdom.Node.t list =
  let eval_aspect, eval_proc =
    match lang with
    | `Lc -> "lc:eval", "lc:eval:v1"
    | `Stlc -> "stlc:eval", "stlc:eval:v1"
  in
  let eval_cached =
    match
      Attachment.peek att ~target:h ~aspect:eval_aspect ~procedure:eval_proc
    with
    | Some _ -> true
    | None -> false
  in
  let eval_button =
    if eval_cached then Vdom.Node.none
    else
      Vdom.Node.button
        ~attrs:
          [
            Vdom.Attr.class_ "btn-action";
            Vdom.Attr.on_click (fun _ -> inject (State.Evaluate h));
          ]
        [ Vdom.Node.text "evaluate" ]
  in
  let typecheck_button =
    match lang with
    | `Stlc ->
        let cached =
          match Stlc_typecheck.peek_cache att h with
          | Some _ -> true
          | None -> false
        in
        if cached then Vdom.Node.none
        else
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.class_ "btn-action";
                Vdom.Attr.on_click (fun _ -> inject (State.Typecheck h));
              ]
            [ Vdom.Node.text "typecheck" ]
    | `Lc -> Vdom.Node.none
  in
  let translate_controls =
    match lang with
    | `Stlc ->
        [
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.class_ "btn-action";
                Vdom.Attr.on_click (fun _ ->
                    inject (State.Translate_stlc_to_lc h));
              ]
            [ Vdom.Node.text "→ erase + Church" ];
        ]
    | `Lc ->
        let input_id = Printf.sprintf "%s-ty-%s" id_ns (Hash.short h) in
        [
          Vdom.Node.input
            ~attrs:
              [
                Vdom.Attr.type_ "text";
                Vdom.Attr.placeholder "expected type, e.g. Bool -> Bool";
                Vdom.Attr.class_ "ty-input";
                Vdom.Attr.id input_id;
              ]
            ();
          Vdom.Node.button
            ~attrs:
              [
                Vdom.Attr.class_ "btn-action";
                Vdom.Attr.on_click (fun _ ->
                    match
                      Js_of_ocaml.Dom_html.getElementById_coerce input_id
                        Js_of_ocaml.Dom_html.CoerceTo.input
                    with
                    | Some e ->
                        let v = Js_of_ocaml.Js.to_string e##.value in
                        inject (State.Translate_lc_to_stlc (h, v))
                    | None -> Vdom.Effect.Ignore);
              ]
            [ Vdom.Node.text "→ check" ];
        ]
  in
  eval_button :: typecheck_button :: translate_controls
