(* Aspect badges + eval action button. Renders the typecheck and
   has-holes results, plus an evaluate button. The printed type is
   clickable: it dispatches Set_filter to filter the browser by this
   exact type, mirroring p7's filter-by-type chip. *)

open! Core
open! Bonsai_web
open P11_mint_threads_substrate

let type_chip
    ~(filter : State.filter)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(extra_class : string list)
    (ty_str : string) : Vdom.Node.t =
  Vdom.Node.span
    ~attrs:
      [
        Vdom.Attr.classes (("mono type-chip clickable" :: extra_class));
        Vdom.Attr.title "filter browser by this type";
        Vdom.Attr.on_click (fun _ ->
            inject
              (State.Set_filter
                 { filter with type_filter = Some ty_str }));
      ]
    [ Vdom.Node.text ty_str ]

let render_typecheck
    ~(att : Attachment.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    ~(filter : State.filter)
    ~(inject : State.action -> unit Vdom.Effect.t)
    (h : Hash.t) : Vdom.Node.t =
  match Typecheck.peek_cache ~store att h with
  | None ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "type" ];
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-empty" ]
            [ Vdom.Node.text "(not cached)" ];
        ]
  | Some (Typecheck.Well_typed ty) ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "type" ];
          type_chip ~filter ~inject ~extra_class:[]
            (Pretty.print_ty_named ~namespace:ns ty);
        ]
  | Some (Typecheck.Well_typed_with_holes ty) ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row aspect-with-holes" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "type" ];
          type_chip ~filter ~inject ~extra_class:[ "type-chip-holes" ]
            (Pretty.print_ty_named ~namespace:ns ty);
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-note" ]
            [ Vdom.Node.text "(best guess; contains holes)" ];
        ]
  | Some (Typecheck.Ill_typed _) ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "type" ];
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-error" ]
            [ Vdom.Node.text "ill-typed" ];
        ]

let render_has_holes ~(store : Store.t) ~(att : Attachment.t) (h : Hash.t)
    : Vdom.Node.t =
  match Has_holes.peek_cache ~store att h with
  | None ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "has-holes" ];
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-empty" ]
            [ Vdom.Node.text "(not cached)" ];
        ]
  | Some true ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "has-holes" ];
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-yes" ]
            [ Vdom.Node.text "◌ yes" ];
        ]
  | Some false ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
        [
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
            [ Vdom.Node.text "has-holes" ];
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "aspect-no" ]
            [ Vdom.Node.text "no" ];
        ]

let render_eval ~(att : Attachment.t) ~(store : Store.t)
    ~(ns : Namespace.t)
    ~(inject : State.action -> unit Vdom.Effect.t) (h : Hash.t) :
    Vdom.Node.t =
  let cached = Eval.peek_cache ~store att h in
  let label =
    match cached with
    | None -> "(not cached)"
    | Some (Eval.Value v) ->
        "= " ^ Pretty.print_named ~namespace:ns store v
    | Some (Eval.Stuck _) -> "stuck"
    | Some (Eval.StepLimit _) -> "step limit"
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "aspect-row" ]
    [
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "aspect-label" ]
        [ Vdom.Node.text "eval" ];
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "mono aspect-eval-result" ]
        [ Vdom.Node.text label ];
      Vdom.Node.button
        ~attrs:
          [
            Vdom.Attr.class_ "btn-mini";
            Vdom.Attr.on_click (fun _ -> inject (State.Evaluate h));
          ]
        [ Vdom.Node.text "run" ];
    ]

let render_aspects
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(att : Attachment.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    ~(filter : State.filter) (h : Hash.t) : Vdom.Node.t list =
  [
    render_typecheck ~att ~store ~ns ~filter ~inject h;
    render_has_holes ~store ~att h;
    render_eval ~att ~store ~ns ~inject h;
  ]
