(* Left pane: the namespace, one row per binding. Abstract types carry an
   "open for edit" / "close" toggle (browser-driven editing context); the open
   ones are highlighted. Clicking a name opens it in the detail pane. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P14_substrate

let badge cls txt =
  Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; cls ] ] [ Vdom.Node.text txt ]

let row ~(inject : State.action -> unit Vdom.Effect.t) ~(open_set : string list)
    ~(selected : string option) (name : string) (h : Hash.t) : Vdom.Node.t =
  let s = Substrate.global in
  let is_open = List.mem open_set h ~equal:String.equal in
  let kind_badge, toggle =
    match Store.find s.store h with
    | Some (Definition.Type (Tnode.Opaque _)) ->
        ( badge "badge-abstract" "abstract",
          [
            Vdom.Node.button
              ~attrs:
                [
                  Vdom.Attr.classes
                    [ "btn-mini"; (if is_open then "btn-open-on" else "") ];
                  Vdom.Attr.on_click (fun _ -> inject (State.Toggle_open h));
                ]
              [ Vdom.Node.text (if is_open then "close" else "open for edit") ];
          ] )
    | Some (Definition.Type _) -> (badge "badge-type" "type", [])
    | Some (Definition.Term (Node.Seal _)) -> (badge "badge-sealed" "sealed", [])
    | Some (Definition.Term _) -> (badge "badge-term" "term", [])
    | None -> (badge "badge-term" "?", [])
  in
  let selected_cls =
    match selected with Some sel when String.equal sel h -> "row-selected" | _ -> ""
  in
  Vdom.Node.div
    ~attrs:
      [
        Vdom.Attr.classes
          [ "browser-row"; selected_cls; (if is_open then "row-open" else "") ];
      ]
    ([
       Vdom.Node.span
         ~attrs:
           [
             Vdom.Attr.classes [ "browser-name"; "clickable" ];
             Vdom.Attr.on_click (fun _ -> inject (State.Select h));
           ]
         [ Vdom.Node.text name ];
       kind_badge;
       Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "mono hash" ] [ Vdom.Node.text (Hash.short h) ];
     ]
    @ toggle)

let test_badge status =
  let cls, txt =
    match status with
    | `Pass -> ("test-pass", "PASS")
    | `Fail -> ("test-fail", "FAIL")
    | `Error _ -> ("test-err", "ERR")
  in
  Vdom.Node.span ~attrs:[ Vdom.Attr.classes [ "badge"; cls ] ] [ Vdom.Node.text txt ]

(* The tests panel: every hash carrying the `test` aspect, with its live
   pass/fail, a namespace filter, and a summary. *)
let tests_section ~(inject : State.action -> unit Vdom.Effect.t) ~(state : State.t) :
    Vdom.Node.t =
  let s = Substrate.global in
  let name_of h = match Namespace.name_of s.ns h with Some n -> n | None -> Hash.short h in
  let flt = String.strip state.test_filter in
  let matches h =
    String.is_empty flt
    || List.exists (Namespace.names_of s.ns h) ~f:(fun n ->
           String.is_substring n ~substring:flt)
  in
  let tests =
    Attachment.marked s.att ~aspect:"test"
    |> List.filter ~f:matches
    |> List.sort ~compare:(fun a b -> String.compare (name_of a) (name_of b))
  in
  let results = List.map tests ~f:(fun h -> (h, Ops.test_status s h)) in
  let pass = List.count results ~f:(fun (_, r) -> match r with `Pass -> true | _ -> false) in
  let total = List.length results in
  let summary_cls = if total > 0 && pass = total then "tests-summary all-pass" else "tests-summary" in
  let rows =
    if total = 0 then
      [ Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "feedback-empty" ] [ Vdom.Node.text "(no tests match)" ] ]
    else
      List.map results ~f:(fun (h, status) ->
          Vdom.Node.div
            ~attrs:
              [
                Vdom.Attr.classes [ "test-row"; "clickable" ];
                Vdom.Attr.on_click (fun _ -> inject (State.Select h));
              ]
            [
              test_badge status;
              Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "browser-name" ] [ Vdom.Node.text (name_of h) ];
              (match status with
               | `Error m -> Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "test-errmsg" ] [ Vdom.Node.text m ]
               | _ -> Vdom.Node.none);
            ])
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "tests-section" ]
    ([
       Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "tests" ];
       Vdom.Node.input
         ~attrs:
           [
             Vdom.Attr.type_ "text";
             Vdom.Attr.class_ "test-filter";
             Vdom.Attr.placeholder "filter by namespace (e.g. Counter.)";
             Vdom.Attr.value_prop state.test_filter;
             Vdom.Attr.create "autocomplete" "off";
             Vdom.Attr.on_input (fun _ v -> inject (State.Set_test_filter v));
           ]
         ();
       Vdom.Node.div ~attrs:[ Vdom.Attr.class_ summary_cls ] [ Vdom.Node.text (Printf.sprintf "%d / %d passing" pass total) ];
     ]
    @ rows)

let view ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state and inject = inject in
  let s = Substrate.global in
  let _ = state.version in
  let entries = Namespace.entries s.ns in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "browser-pane" ]
    (tests_section ~inject ~state
    :: Vdom.Node.h2 ~attrs:[ Vdom.Attr.class_ "panel-title" ] [ Vdom.Node.text "namespace" ]
    :: List.map entries ~f:(fun (name, h) ->
           row ~inject ~open_set:state.open_set ~selected:state.selected name h))
