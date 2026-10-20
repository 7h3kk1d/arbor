(* The codebase. Left pane.

   Four lists, deliberately separate, because they are four populations rather
   than one. A definition here is a PAIR -- an open term and a link -- so the
   store shows both halves, and the counts alongside are the sharing claim in
   numbers: fewer open terms than definitions means shapes are being reused. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P22_substrate

let pill cls text = Vdom.Node.span ~attrs:[ Vdom.Attr.class_ cls ] [ Vdom.Node.text text ]

let view ~(state : State.t Bonsai.Value.t) ~inject =
  let%arr state = state
  and inject = inject in
  ignore state.version;
  let s = Substrate.global in
  let matches name =
    String.is_empty state.ns_filter
    || String.is_substring name ~substring:state.ns_filter
  in
  let name_rows =
    List.filter (Namespace.all s.Session.ns) ~f:(fun (n, _) -> matches n)
    |> List.map ~f:(fun (n, h) ->
      let selected =
        match state.selected with
        | State.Named m -> String.equal m n
        | State.Nothing_selected -> false
      in
      let ty =
        match Store.type_of s.Session.store h with
        | Some t -> Ty.to_string t
        | None -> "?"
      in
      let open_h =
        match Store.get_def s.Session.store h with
        | Some d ->
          (match Store.open_hash_of s.Session.store d with
           | Some o -> Hash.short ~len:8 o
           | None -> "")
        | None -> ""
      in
      Vdom.Node.div
        ~attrs:
          [ Vdom.Attr.class_ (if selected then "nrow sel" else "nrow")
          ; Vdom.Attr.on_click (fun _ -> inject (State.Select (State.Named n)))
          ]
        [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "nm" ] [ Vdom.Node.text n ]
        ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "ty" ] [ Vdom.Node.text (": " ^ ty) ]
        ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "oh" ] [ Vdom.Node.text open_h ]
        ])
  in
  let counts =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "counts" ]
      [ pill "count" (Printf.sprintf "%d defs" (Store.count_defs s.Session.store))
      ; pill "count" (Printf.sprintf "%d open" (Store.count_opens s.Session.store))
      ; pill "count" (Printf.sprintf "%d links" (Store.count_links s.Session.store))
      ; pill
          "count"
          (Printf.sprintf "%d bundles" (Store.count_bundles s.Session.store))
      ]
  in
  let sharing =
    let d = Store.count_defs s.Session.store
    and o = Store.count_opens s.Session.store in
    if d > o
    then
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "note" ]
        [ Vdom.Node.text
            (Printf.sprintf
               "%d definitions over %d open terms: %d share a shape with \
                something else."
               d
               o
               (d - o))
        ]
    else Vdom.Node.none
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "pane browser" ]
    [ Vdom.Node.h2 [ Vdom.Node.text "codebase" ]
    ; counts
    ; sharing
    ; Vdom.Node.input
        ~attrs:
          [ Vdom.Attr.class_ "filter"
          ; Vdom.Attr.placeholder "filter names"
          ; Vdom.Attr.value_prop state.ns_filter
          ; Vdom.Attr.on_input (fun _ v -> inject (State.Set_ns_filter v))
          ]
        ()
    ; Vdom.Node.h3 [ Vdom.Node.text "namespace" ]
    ; Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "names" ] name_rows
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "note" ]
        [ Vdom.Node.text
            "Single-valued: a name denotes one definition. Several names may \
             denote one definition -- that is an alias, and it cannot change \
             what propagation does, because propagation rewrites links."
        ]
    ]
