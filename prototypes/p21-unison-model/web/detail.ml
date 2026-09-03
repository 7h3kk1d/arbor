(* The selected component. Right pane.
   Shows what is actually stored: the members in their storage order, each
   member's position, its names, its type, its cached value, and what depends
   on it. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P21_substrate

let kv k v =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "kv" ]
    [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "k" ] [ Vdom.Node.text k ]
    ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "v" ] [ Vdom.Node.text v ]
    ]

let member_block ~(s : Substrate.t) ~(hash : string) i =
  let r = Reference.Derived (hash, i) in
  let ppe = Ppe.make s.ns in
  let names = Names.names_of s.ns (Reference.Ref r) in
  let ty =
    match Store.type_of s.store r with
    | Some t -> Ty.to_string t
    | None -> "?"
  in
  let src =
    match Store.lookup s.store r with
    | Some tm -> Pretty.term ppe tm
    | None -> "(missing)"
  in
  let deps = Store.deps s.store r in
  let dependents = Store.dependents s.store r in
  let show_refs rs =
    if List.is_empty rs
    then "(none)"
    else
      String.concat
        ~sep:", "
        (List.map rs ~f:(fun r ->
           match Names.names_of s.ns (Reference.Ref r) with
           | n :: _ -> n
           | [] -> Reference.to_string r))
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ (if i = 0 then "member-block pos0" else "member-block") ]
    [ Vdom.Node.h3
        [ Vdom.Node.text
            (Printf.sprintf
               "position %d%s"
               i
               (if i = 0 then "  — referenced transparently" else "  — referenced opaquely"))
        ]
    ; kv "reference" (Reference.to_string r)
    ; kv "names" (if List.is_empty names then "(unnamed)" else String.concat ~sep:", " names)
    ; kv "type" ty
    ; kv "stored form" src
    ; kv "depends on" (show_refs deps)
    ; kv "depended on by" (show_refs dependents)
    ; kv "cached value" (Ops.value_of ~s r)
    ]

(* --- the watch cache --------------------------------------------------------

   The cache is the one piece of state in this prototype that is worth watching
   directly rather than in summary, because its two properties are only visible
   as *contents*: the key is a reference that need not denote anything stored
   (prop:watchdom), and nothing is ever removed (cor:wnoinval). Both show up as
   rows that outlive the buffer that produced them. *)

let cache_view ~(s : Substrate.t) ~(sc : Scratch.t) =
  let entries = Watch.entries s.watch in
  (* Which entries the buffer currently points at. Everything else is an entry
     for an expression the buffer no longer contains — still valid, still
     returning the right answer, and never invalidated. *)
  let live =
    List.filter_map sc.watches ~f:(fun (w : Scratch.watch) -> w.reference)
  in
  let is_live r = List.exists live ~f:(fun r' -> Reference.equal r r') in
  let ordered =
    let l, o = List.partition_tf entries ~f:(fun (r, _) -> is_live r) in
    List.sort l ~compare:(fun (a, _) (b, _) -> Reference.compare a b)
    @ List.sort o ~compare:(fun (a, _) (b, _) -> Reference.compare a b)
  in
  let ppe = Ppe.make s.ns in
  let row (r, v) =
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ (if is_live r then "centry live" else "centry") ]
      [ Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "ckey" ]
          [ Vdom.Node.text
              (match r with
               | Reference.Derived (h, i) ->
                 Hash.short ~len:20 h ^ if i = 0 then "" else "." ^ Int.to_string i
               | Reference.Builtin b -> "##" ^ b)
          ]
      ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "carrow" ] [ Vdom.Node.text "→" ]
      ; Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "cval" ]
          [ Vdom.Node.text (Pretty.term ppe v) ]
      ; (if is_live r
         then Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "cmark" ] [ Vdom.Node.text "in buffer" ]
         else Vdom.Node.none)
      ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "member-block cache" ]
    [ Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "pane-h2" ] [ Vdom.Node.text "watch cache" ]
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "cmeta" ]
        [ Vdom.Node.span
            [ Vdom.Node.text
                (Printf.sprintf
                   "%d entr%s"
                   (List.length entries)
                   (if List.length entries = 1 then "y" else "ies"))
            ]
        ; Vdom.Node.span
            [ Vdom.Node.text
                (match sc.evaluated with
                 | 0 -> "nothing evaluated this save"
                 | 1 -> "1 evaluated this save"
                 | n -> Printf.sprintf "%d evaluated this save" n)
            ]
        ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "cstage" ]
            [ Vdom.Node.text (Scratch.stage_label sc.stage) ]
        ]
    ; (if List.is_empty ordered
       then
         Vdom.Node.p
           ~attrs:[ Vdom.Attr.class_ "hint" ]
           [ Vdom.Node.text "Empty. Add a watch line (`> expr`) to the buffer." ]
       else Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "centries" ] (List.map ordered ~f:row))
    ; Vdom.Node.p
        ~attrs:[ Vdom.Attr.class_ "hint" ]
        [ Vdom.Node.text
            "Keyed by reference, holding the result term inline. Rows without a \
             marker are expressions the buffer no longer contains — nothing \
             removed them, and nothing can: the key is the expression, so a \
             stale entry is unreachable rather than wrong."
        ]
    ]

let view ~(state : State.t Bonsai.Value.t) ~inject:_ =
  let%arr state = state in
  ignore state.version;
  let s = Substrate.global in
  (* the value the last save produced — NOT a fresh analysis, which would report
     no hashes, no evaluations and an empty cache no matter what happened *)
  let sc = !Substrate.scratch in
  let selected =
    match state.selected with
    | State.Nothing_selected ->
      [ Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "pane-h2" ] [ Vdom.Node.text "component" ]
      ; Vdom.Node.p
          ~attrs:[ Vdom.Attr.class_ "hint" ]
          [ Vdom.Node.text "Select a component in the codebase pane." ]
      ]
    | State.Definition hash ->
      let n = Store.component_size s.store hash in
      Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "pane-h2" ] [ Vdom.Node.text "component" ]
      :: (if n = 0
          then [ Vdom.Node.p ~attrs:[ Vdom.Attr.class_ "hint" ] [ Vdom.Node.text "(gone)" ] ]
          else
            kv "component" (Hash.to_string hash)
            :: kv
                 "members"
                 (match n with
                  | 1 -> "1 — a singleton component"
                  | n -> Printf.sprintf "%d — a cycle, hashed as a unit" n)
            :: List.init n ~f:(member_block ~s ~hash))
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "detail-pane" ]
    (cache_view ~s ~sc :: selected)
