(* Left-column definition browser: filters + row list. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P7_web_interface_substrate

let truncate_body s =
  let s = String.strip s in
  if String.length s > 72 then String.prefix s 72 ^ "…" else s

let one_line s =
  String.map s ~f:(fun c ->
      match c with '\n' | '\r' | '\t' -> ' ' | _ -> c)

let lang_of_hash store h : [ `Lc | `Stlc ] =
  match Pretty.language_tag_of_hash store h with
  | tag when String.is_substring tag ~substring:"stlc" -> `Stlc
  | _ -> `Lc

let hash_matches_lang store h (lf : State.lang_filter) : bool =
  match lf, lang_of_hash store h with
  | All, _ -> true
  | Only_lc, `Lc -> true
  | Only_stlc, `Stlc -> true
  | _ -> false

let hash_matches_query ~store ~ns h (q : string) : bool =
  let q = String.strip q in
  if String.is_empty q then true
  else
    let short = Hash.short h in
    let q_no_hash = String.chop_prefix_if_exists q ~prefix:"#" in
    if String.is_substring short ~substring:q_no_hash then true
    else
      let _ = store in
      let names = Namespace.names_of ns h in
      List.exists names ~f:(fun n ->
          String.is_substring n ~substring:q)

let has_eval store att h =
  let lang = lang_of_hash store h in
  let asp, proc =
    match lang with
    | `Lc -> "lc:eval", "lc:eval:v1"
    | `Stlc -> "stlc:eval", "stlc:eval:v1"
  in
  Attachment.peek att ~target:h ~aspect:asp ~procedure:proc

let has_typecheck att h =
  Stlc_typecheck.peek_cache att h

let render_row
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(store : Store.t)
    ~(att : Attachment.t)
    ~(ns : Namespace.t)
    (h : Hash.t) : Vdom.Node.t =
  let lang = lang_of_hash store h in
  let lang_class = match lang with `Lc -> "tag-lc" | `Stlc -> "tag-stlc" in
  let lang_label = match lang with `Lc -> "lc" | `Stlc -> "stlc" in
  let short = Hash.short h in
  let names = Namespace.names_of ns h in
  let body =
    try Pretty.print_named ~namespace:ns store h |> one_line |> truncate_body
    with _ -> "(cannot render)"
  in
  let is_named = not (List.is_empty names) in
  let name_chips =
    List.map names ~f:(fun n ->
        Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "chip chip-name" ]
          [ Vdom.Node.text n ])
  in
  let aspect_indicators =
    let icons = ref [] in
    (match has_typecheck att h with
     | Some ty ->
         icons :=
           Vdom.Node.span
             ~attrs:
               [
                 Vdom.Attr.class_ "aspect-icon ty-icon";
                 Vdom.Attr.title (Printf.sprintf "type: %s" (Ty.print ty));
               ]
             [ Vdom.Node.text ":t" ]
           :: !icons
     | None -> ());
    (match has_eval store att h with
     | Some _ ->
         icons :=
           Vdom.Node.span
             ~attrs:
               [
                 Vdom.Attr.class_ "aspect-icon eval-icon";
                 Vdom.Attr.title "evaluated";
               ]
             [ Vdom.Node.text "⇓" ]
           :: !icons
     | None -> ());
    List.rev !icons
  in
  Vdom.Node.div
    ~attrs:
      [
        Vdom.Attr.classes
          [ "row"; (if is_named then "row-named" else "row-unnamed") ];
        Vdom.Attr.on_click (fun _ -> inject (State.Set_view (Detail h)));
      ]
    [
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.classes [ "chip"; lang_class ] ]
        [ Vdom.Node.text lang_label ];
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "mono hash" ]
        [ Vdom.Node.text short ];
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "row-aspects" ]
        aspect_indicators;
      Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "names" ] name_chips;
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "mono body" ]
        [ Vdom.Node.text body ];
    ]

let lang_radio
    ~(current : State.lang_filter)
    ~(target : State.lang_filter)
    ~(label : string)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(state : State.t) : Vdom.Node.t =
  let selected = State.equal_lang_filter current target in
  Vdom.Node.button
    ~attrs:
      [
        Vdom.Attr.classes
          [ "btn-filter"; (if selected then "btn-selected" else "") ];
        Vdom.Attr.on_click (fun _ ->
            inject
              (State.Set_filter { state.filter with lang = target }));
      ]
    [ Vdom.Node.text label ]

let scope_radio
    ~(current : State.scope_filter)
    ~(target : State.scope_filter)
    ~(label : string)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(state : State.t) : Vdom.Node.t =
  let selected = State.equal_scope_filter current target in
  Vdom.Node.button
    ~attrs:
      [
        Vdom.Attr.classes
          [ "btn-filter"; (if selected then "btn-selected" else "") ];
        Vdom.Attr.on_click (fun _ ->
            inject
              (State.Set_filter { state.filter with scope = target }));
      ]
    [ Vdom.Node.text label ]

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state
  and inject = inject in
  let store = Substrate.global.store in
  let ns = Substrate.global.ns in
  let att = Substrate.global.att in
  let all_hashes = Store.hashes store in
  let named_hashes =
    Namespace.entries ns |> List.map ~f:snd |> List.dedup_and_sort ~compare:String.compare
  in
  let scoped =
    match state.filter.scope with
    | State.Named_only -> named_hashes
    | State.All_hashes -> all_hashes
  in
  let type_matches h =
    match state.filter.type_filter with
    | None -> true
    | Some t ->
        (match Stlc_typecheck.peek_cache att h with
         | Some ty -> String.equal (Ty.print ty) t
         | None -> false)
  in
  let filtered =
    scoped
    |> List.filter ~f:(fun h ->
           hash_matches_lang store h state.filter.lang
           && hash_matches_query ~store ~ns h state.filter.query
           && type_matches h)
    |> List.sort ~compare:(fun a b ->
           String.compare (Hash.short a) (Hash.short b))
  in
  let rows = List.map filtered ~f:(render_row ~inject ~store ~att ~ns) in
  let stats = Attachment.stats att in
  let count_label =
    Printf.sprintf "%d shown · store %d · named %d · aspects %d"
      (List.length filtered) (List.length all_hashes)
      (Namespace.size ns) stats.entries
  in
  let author_button =
    Vdom.Node.button
      ~attrs:
        [
          Vdom.Attr.class_ "btn-primary";
          Vdom.Attr.on_click (fun _ -> inject (State.Set_view Author));
        ]
      [ Vdom.Node.text "+ new" ]
  in
  let reset_button =
    Vdom.Node.button
      ~attrs:
        [
          Vdom.Attr.class_ "btn-secondary btn-reset";
          Vdom.Attr.title
            "reload the page to reset the store and restore the seeded \
             demo state";
          Vdom.Attr.on_click (fun _ ->
              Js_of_ocaml.Dom_html.window##.location##reload;
              Vdom.Effect.Ignore);
        ]
      [ Vdom.Node.text "reset" ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "browser" ]
    [
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "browser-header" ]
        [
          Vdom.Node.h2 [ Vdom.Node.text "browser" ];
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "header-actions" ]
            [ reset_button; author_button ];
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "filter-row" ]
        [
          lang_radio ~current:state.filter.lang ~target:All ~label:"all"
            ~inject ~state;
          lang_radio ~current:state.filter.lang ~target:Only_lc ~label:"lc"
            ~inject ~state;
          lang_radio ~current:state.filter.lang ~target:Only_stlc
            ~label:"stlc" ~inject ~state;
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "filter-row" ]
        [
          scope_radio ~current:state.filter.scope ~target:Named_only
            ~label:"named only" ~inject ~state;
          scope_radio ~current:state.filter.scope ~target:All_hashes
            ~label:"all hashes" ~inject ~state;
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "filter-row" ]
        [
          Vdom.Node.input
            ~attrs:
              [
                Vdom.Attr.type_ "text";
                Vdom.Attr.placeholder "search name or #hash…";
                Vdom.Attr.value state.filter.query;
                Vdom.Attr.on_input (fun _ s ->
                    inject
                      (State.Set_filter { state.filter with query = s }));
              ]
            ();
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "count-label" ]
        [ Vdom.Node.text count_label ];
      (match state.filter.type_filter with
       | None -> Vdom.Node.none
       | Some t ->
           Vdom.Node.div
             ~attrs:[ Vdom.Attr.class_ "active-filter" ]
             [
               Vdom.Node.span
                 ~attrs:[ Vdom.Attr.class_ "filter-label" ]
                 [ Vdom.Node.text "type:" ];
               Vdom.Node.span
                 ~attrs:[ Vdom.Attr.class_ "mono" ]
                 [ Vdom.Node.text t ];
               Vdom.Node.span
                 ~attrs:
                   [
                     Vdom.Attr.class_ "filter-clear";
                     Vdom.Attr.on_click (fun _ ->
                         inject
                           (State.Set_filter
                              { state.filter with type_filter = None }));
                   ]
                 [ Vdom.Node.text "×" ];
             ]);
      Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "rows" ] rows;
    ]
