(* Left pane: the namespace, one row per binding. Abstract types carry an
   "open for edit" / "close" toggle (browser-driven editing context); the open
   ones are highlighted. Clicking a name opens it in the detail pane. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P15_substrate

let badge cls txt =
  Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; cls ] ] [ Vdom.Node.text txt ]

let test_badge status =
  let cls, txt =
    match status with
    | `Pass -> ("test-pass", "pass")
    | `Fail -> ("test-fail", "fail")
    | `Error _ -> ("test-err", "err")
  in
  Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; cls ] ] [ Vdom.Node.text txt ]

(* the trailing controls of a bound row: a kind badge — or, for a term carrying
   the `test` aspect, its live pass/fail badge — then the short hash, and (for an
   abstract type) the editing-context toggle. Compact so the row never wraps. *)
let binding_controls ~(inject : State.action -> unit Vdom.Effect.t)
    ~(open_set : string list) (h : Hash.t) : Vdom.Node.t list =
  let s = Substrate.global in
  let is_open = List.mem open_set h ~equal:String.equal in
  let is_test =
    match Store.find s.store h with
    | Some (Definition.Term _) -> Attachment.has s.att ~aspect:"test" h
    | _ -> false
  in
  let kind_badge, toggle =
    if is_test then (test_badge (Ops.test_status s h), [])
    else
      match Store.find s.store h with
      | Some (Definition.Type (Tnode.Opaque _)) ->
          ( badge "badge-abstract" "abstract",
            [
              Vdom.Node.button
                ~attrs:
                  [
                    Vdom.Attr.classes [ "btn-edit"; (if is_open then "btn-open-on" else "") ];
                    Vdom.Attr.on_click (fun _ -> inject (State.Toggle_open h));
                  ]
                [ Vdom.Node.text (if is_open then "editing" else "edit") ];
            ] )
      | Some (Definition.Type _) -> (badge "badge-type" "type", [])
      | Some (Definition.Term (Node.Seal _)) -> (badge "badge-sealed" "sealed", [])
      | Some (Definition.Term _) -> (badge "badge-term" "term", [])
      | None -> (badge "badge-term" "?", [])
  in
  [
    kind_badge;
    Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "mono hash" ] [ Vdom.Node.text (Hash.short h) ];
  ]
  @ toggle

let row_classes ~(open_set : string list) ~(selected : string option) (h : Hash.t) =
  let sel = match selected with Some s when String.equal s h -> "row-selected" | _ -> "" in
  let opn = if List.mem open_set h ~equal:String.equal then "row-open" else "" in
  [ "browser-row"; sel; opn ]

(* A leaf binding (no children): the last path segment + its controls. *)
let leaf_row ~(inject : State.action -> unit Vdom.Effect.t) ~(state : State.t)
    ~(seg : string) (h : Hash.t) : Vdom.Node.t =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.classes (row_classes ~open_set:state.open_set ~selected:state.selected h) ]
    (Vdom.Node.span
       ~attrs:
         [
           Vdom.Attr.classes [ "browser-name"; "clickable" ];
           Vdom.Attr.on_click (fun _ -> inject (State.Select h));
         ]
       [ Vdom.Node.text seg ]
    :: binding_controls ~inject ~open_set:state.open_set h)

(* A section header: a collapse triangle + the segment. If the section path is
   itself a binding (e.g. an opened module Box that also has Box.t/Box.empty),
   the header doubles as that binding's row. *)
let section_header ~(inject : State.action -> unit Vdom.Effect.t) ~(state : State.t)
    ~(seg : string) ~(path : string) ~(collapsed : bool) ~(direct : Hash.t option) :
    Vdom.Node.t =
  let triangle =
    Vdom.Node.span
      ~attrs:
        [
          Vdom.Attr.classes [ "tree-triangle"; "clickable" ];
          Vdom.Attr.on_click (fun _ -> inject (State.Toggle_collapse path));
        ]
      [ Vdom.Node.text (if collapsed then "\xe2\x96\xb6" else "\xe2\x96\xbc") ]
  in
  let name_node, controls, hl =
    match direct with
    | Some h ->
        ( Vdom.Node.span
            ~attrs:
              [
                Vdom.Attr.classes [ "browser-name"; "clickable" ];
                Vdom.Attr.on_click (fun _ -> inject (State.Select h));
              ]
            [ Vdom.Node.text seg ],
          binding_controls ~inject ~open_set:state.open_set h,
          row_classes ~open_set:state.open_set ~selected:state.selected h )
    | None ->
        ( Vdom.Node.span
            ~attrs:
              [
                Vdom.Attr.classes [ "browser-name"; "clickable"; "tree-section" ];
                Vdom.Attr.on_click (fun _ -> inject (State.Toggle_collapse path));
              ]
            [ Vdom.Node.text seg ],
          [],
          [ "browser-row" ] )
  in
  Vdom.Node.div ~attrs:[ Vdom.Attr.classes ("tree-header" :: hl) ] (triangle :: name_node :: controls)

(* Render a level of the tree from entries given as (remaining_segments, hash).
   Entries are pre-sorted by full name so same-first-segment rows are adjacent. *)
let rec render_nodes ~(inject : State.action -> unit Vdom.Effect.t) ~(state : State.t)
    ~(prefix : string) (entries : (string list * Hash.t) list) : Vdom.Node.t list =
  List.group entries ~break:(fun (a, _) (b, _) ->
      not (String.equal (List.hd_exn a) (List.hd_exn b)))
  |> List.concat_map ~f:(fun group ->
         let seg = match group with (s :: _, _) :: _ -> s | _ -> "?" in
         let path = if String.is_empty prefix then seg else prefix ^ "." ^ seg in
         let direct =
           List.find_map group ~f:(fun (segs, h) ->
               match segs with [ _ ] -> Some h | _ -> None)
         in
         let children =
           List.filter_map group ~f:(fun (segs, h) ->
               match segs with _ :: (_ :: _ as rest) -> Some (rest, h) | _ -> None)
         in
         if List.is_empty children then
           match direct with Some h -> [ leaf_row ~inject ~state ~seg h ] | None -> []
         else
           let collapsed = List.mem state.collapsed path ~equal:String.equal in
           let header = section_header ~inject ~state ~seg ~path ~collapsed ~direct in
           if collapsed then [ Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "tree-node" ] [ header ] ]
           else
             [
               Vdom.Node.div
                 ~attrs:[ Vdom.Attr.class_ "tree-node" ]
                 [
                   header;
                   Vdom.Node.div
                     ~attrs:[ Vdom.Attr.class_ "tree-children" ]
                     (render_nodes ~inject ~state ~prefix:path children);
                 ];
             ])

(* Footer pinned to the bottom: live pass/total across every binding carrying the
   `test` aspect. Tests themselves live inline in the tree (each shows its
   pass/fail badge); this is just the running tally. *)
let tests_summary () : Vdom.Node.t =
  let s = Substrate.global in
  let results = List.map (Attachment.marked s.att ~aspect:"test") ~f:(fun h -> Ops.test_status s h) in
  let pass = List.count results ~f:(function `Pass -> true | _ -> false) in
  let total = List.length results in
  let cls = if total > 0 && pass = total then "tests-summary all-pass" else "tests-summary" in
  Vdom.Node.div ~attrs:[ Vdom.Attr.class_ cls ]
    [ Vdom.Node.text (Printf.sprintf "tests: %d / %d passing" pass total) ]

let view ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state and inject = inject in
  let s = Substrate.global in
  let _ = state.version in
  let entries =
    Namespace.entries s.ns
    |> List.sort ~compare:(fun (a, _) (b, _) -> String.compare a b)
    |> List.map ~f:(fun (name, h) -> (String.split name ~on:'.', h))
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "browser-pane" ]
    ((Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "namespace" ]
     :: render_nodes ~inject ~state ~prefix:"" entries)
    @ [ tests_summary () ])
