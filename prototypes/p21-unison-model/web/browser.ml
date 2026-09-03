(* The codebase. Left pane.
   Two lists, deliberately separate, because they are two different objects: the
   STORE maps a hash to a component and a definition is a (hash, position) pair;
   the NAMESPACE is a relation over names and referents, sitting beside it. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P21_substrate

let member_row ~inject ~(hash : string) (pos, names, ty) =
  let label =
    match names with
    | [] -> "(unnamed)"
    | ns -> String.concat ~sep:" = " ns
  in
  Vdom.Node.div
    ~attrs:
      [ Vdom.Attr.class_ (if pos = 0 then "member pos0" else "member")
      ; Vdom.Attr.on_click (fun _ -> inject (State.Select (State.Definition hash)))
      ]
    [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "pos" ] [ Vdom.Node.text (Int.to_string pos) ]
    ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "nm" ] [ Vdom.Node.text label ]
    ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "ty" ] [ Vdom.Node.text (": " ^ ty) ]
    ]

let view ~(state : State.t Bonsai.Value.t) ~inject =
  let%arr state = state
  and inject = inject in
  ignore state.version;
  let s = Substrate.global in
  let comps = Ops.components ~s in
  let matches name = String.is_empty state.ns_filter || String.is_substring name ~substring:state.ns_filter in
  let comp_nodes =
    List.filter_map comps ~f:(fun (c : Ops.comp_view) ->
      let visible =
        List.filter c.members ~f:(fun (_, names, _) ->
          List.is_empty names || List.exists names ~f:matches)
      in
      if List.is_empty visible
      then None
      else
        Some
          (Vdom.Node.div
             ~attrs:[ Vdom.Attr.class_ "component" ]
             (Vdom.Node.div
                ~attrs:[ Vdom.Attr.class_ "chash" ]
                [ Vdom.Node.text (Hash.short ~len:12 c.hash)
                ; Vdom.Node.span
                    ~attrs:[ Vdom.Attr.class_ "csize" ]
                    [ Vdom.Node.text
                        (match List.length c.members with
                         | 1 -> "singleton"
                         | n -> Printf.sprintf "cycle of %d" n)
                    ]
                ]
              :: List.map visible ~f:(member_row ~inject ~hash:c.hash))))
  in
  let name_rows =
    List.filter (Names.rows s.ns) ~f:(fun (n, _) -> matches n)
    |> List.map ~f:(fun (n, r) ->
      let conflicted = not (Names.unconflicted_at s.ns n) in
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ (if conflicted then "nrow conflicted" else "nrow") ]
        [ Vdom.Node.span
            ~attrs:
              [ Vdom.Attr.class_ "nm"
              ; Vdom.Attr.on_click (fun _ -> inject (State.Edit_into_buffer n))
              ]
            [ Vdom.Node.text n ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "ref" ]
            [ Vdom.Node.text (Reference.referent_to_string r) ]
        ; (if conflicted
           then Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "badge" ] [ Vdom.Node.text "conflicted" ]
           else Vdom.Node.none)
        ; Vdom.Node.span
            ~attrs:
              [ Vdom.Attr.class_ "unbind"; Vdom.Attr.on_click (fun _ -> inject (State.Unbind n)) ]
            [ Vdom.Node.text "×" ]
        ])
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "browser-pane" ]
    [ Vdom.Node.h2 [ Vdom.Node.text "codebase" ]
    ; Vdom.Node.input
        ~attrs:
          [ Vdom.Attr.class_ "filter"
          ; Vdom.Attr.placeholder "filter"
          ; Vdom.Attr.value_prop state.ns_filter
          ; Vdom.Attr.on_input (fun _ s -> inject (State.Set_ns_filter s))
          ]
        ()
    ; Vdom.Node.h3 [ Vdom.Node.text "namespace" ]
    ; Vdom.Node.p
        ~attrs:[ Vdom.Attr.class_ "hint" ]
        [ Vdom.Node.text
            "A relation, not a function: one name may denote several referents \
             (def:unames). Click a name to render it back into the buffer."
        ]
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "rows" ]
        (if List.is_empty name_rows
         then [ Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "hint" ] [ Vdom.Node.text "(empty)" ] ]
         else name_rows)
    ; Vdom.Node.h3 [ Vdom.Node.text "store" ]
    ; Vdom.Node.p
        ~attrs:[ Vdom.Attr.class_ "hint" ]
        [ Vdom.Node.text
            "Hash → component. A definition is a (hash, position) pair; \
             position 0 is the one referenced transparently \
             (prop:transparent)."
        ]
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "rows" ]
        (if List.is_empty comp_nodes
         then [ Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "hint" ] [ Vdom.Node.text "(empty)" ] ]
         else comp_nodes)
    ]
