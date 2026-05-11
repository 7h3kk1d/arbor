(* Left-pane definition browser. Hierarchy is the organizing principle —
   the namespace tree from p9 — augmented with the filter affordances
   from p7's flat browser: scope toggle (named only / all hashes),
   search query (matches name segments + #hash prefix), an active type
   filter chip set by clicking the type in the detail pane, an aspect
   indicator strip per leaf (:t typecheck, ⇓ eval, ◌ has-holes), a
   one-line body preview, a count label, and a reset button.

   When scope = All_hashes, the tree shows the named bindings as before
   and adds a flat "(anonymous)" section underneath listing the in-store
   hashes that have no name. Anonymous hashes are common — every
   keystroke ingests an unnamed intermediate. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P10_minted_labels_substrate

(* ===== Tree shape ===== *)

type tree =
  | Leaf of Hash.t
  | Node of (string * tree) list

let split_dotted (s : string) : string list = String.split s ~on:'.'

let rec insert (segs : string list) (h : Hash.t) (t : tree) : tree =
  match segs, t with
  | [], _ -> Leaf h
  | seg :: rest, Node children ->
      let found = ref false in
      let updated =
        List.map children ~f:(fun (s, sub) ->
            if String.equal s seg then begin
              found := true;
              (s, insert rest h sub)
            end
            else (s, sub))
      in
      if !found then Node updated
      else Node (updated @ [ (seg, insert rest h (Node [])) ])
  | _ :: _, Leaf _ -> t

let build (entries : (string * Hash.t) list) : tree =
  let sorted =
    List.sort entries ~compare:(fun (a, _) (b, _) -> String.compare a b)
  in
  List.fold_left sorted ~init:(Node []) ~f:(fun acc (name, h) ->
      insert (split_dotted name) h acc)

let path_join prefix seg =
  if String.is_empty prefix then seg else prefix ^ "." ^ seg

(* ===== Filter predicates ===== *)

let truncate_body s =
  let s = String.strip s in
  if String.length s > 56 then String.prefix s 56 ^ "…" else s

let one_line s =
  String.map s ~f:(fun c ->
      match c with '\n' | '\r' | '\t' -> ' ' | _ -> c)

let hash_matches_query ~ns h (q : string) : bool =
  let q = String.strip q in
  if String.is_empty q then true
  else
    let short = Hash.short h in
    let q_no_hash = String.chop_prefix_if_exists q ~prefix:"#" in
    if (not (String.is_empty q_no_hash))
       && String.is_substring short ~substring:q_no_hash
    then true
    else
      let names = Namespace.names_of ns h in
      List.exists names ~f:(fun n -> String.is_substring n ~substring:q)

let type_matches ~att ~store h (tf : string option) : bool =
  match tf with
  | None -> true
  | Some t ->
      (match Typecheck.peek_cache ~store att h with
       | Some (Well_typed ty) -> String.equal (Ty.print ty) t
       | Some (Well_typed_with_holes ty) -> String.equal (Ty.print ty) t
       | _ -> false)

(* Names that explicitly survive the query filter. Used to decide which
   tree leaves and intermediate prefixes to show. *)
let leaf_visible ~att ~store ~filter ~ns h ~name =
  let q_ok =
    let q = String.strip filter.State.query in
    if String.is_empty q then true
    else
      let q_no_hash = String.chop_prefix_if_exists q ~prefix:"#" in
      let short = Hash.short h in
      ((not (String.is_empty q_no_hash))
       && String.is_substring short ~substring:q_no_hash)
      || String.is_substring name ~substring:q
  in
  let _ = ns in
  q_ok && type_matches ~att ~store h filter.type_filter

(* ===== Aspect indicators ===== *)

let render_aspect_icons ~(att : Attachment.t) ~(store : Store.t)
    (h : Hash.t) : Vdom.Node.t list =
  let icons = ref [] in
  (match Typecheck.peek_cache ~store att h with
   | Some (Well_typed ty) ->
       icons :=
         Vdom.Node.span
           ~attrs:
             [
               Vdom.Attr.class_ "aspect-icon ty-icon";
               Vdom.Attr.title (Printf.sprintf "type: %s" (Ty.print ty));
             ]
           [ Vdom.Node.text ":t" ]
         :: !icons
   | Some (Well_typed_with_holes ty) ->
       icons :=
         Vdom.Node.span
           ~attrs:
             [
               Vdom.Attr.class_ "aspect-icon ty-icon ty-icon-holes";
               Vdom.Attr.title
                 (Printf.sprintf "type (best guess; contains holes): %s"
                    (Ty.print ty));
             ]
           [ Vdom.Node.text ":t" ]
         :: !icons
   | _ -> ());
  (match Eval.peek_cache att h with
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
  (match Has_holes.peek_cache att h with
   | Some true ->
       icons :=
         Vdom.Node.span
           ~attrs:
             [
               Vdom.Attr.class_ "aspect-icon holes-icon";
               Vdom.Attr.title "contains holes";
             ]
           [ Vdom.Node.text "◌" ]
         :: !icons
   | _ -> ());
  List.rev !icons

let render_leaf_row
    ~(att : Attachment.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    ~(state : State.t)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~depth
    ~label
    (h : Hash.t) : Vdom.Node.t =
  let indent_style =
    Vdom.Attr.style (Css_gen.padding_left (`Px (8 + (depth * 14))))
  in
  let names = Namespace.names_of ns h in
  let kind = Store.kind_of store h in
  let body =
    try
      (match kind with
       | Some Definition.Type_kind ->
           Pretty.print_named_ty ~namespace:ns store h
       | _ -> Pretty.print_named ~namespace:ns store h)
      |> one_line |> truncate_body
    with _ -> "(cannot render)"
  in
  let is_named = not (List.is_empty names) in
  let selected =
    match state.view with
    | State.Detail h' when Hash.equal h h' -> true
    | _ -> false
  in
  let indicators = render_aspect_icons ~att ~store h in
  let kind_badge =
    match kind with
    | Some Definition.Type_kind ->
        Vdom.Node.span
          ~attrs:
            [
              Vdom.Attr.classes [ "kind-badge"; "kind-badge-type" ];
              Vdom.Attr.title "type definition";
            ]
          [ Vdom.Node.text "T" ]
    | _ -> Vdom.Node.none
  in
  let alias_chips =
    (* If the leaf has multiple names, show the aliases inline after the
       primary label. The label itself is what the tree row is keyed on. *)
    if List.length names <= 1 then []
    else
      List.filter names ~f:(fun n -> not (String.equal n label))
      |> List.map ~f:(fun n ->
             Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "chip chip-name chip-alias" ]
               [ Vdom.Node.text n ])
  in
  Vdom.Node.div
    ~attrs:
      [
        indent_style;
        Vdom.Attr.classes
          ([
             "browser-row";
             (if is_named then "browser-row-named" else "browser-row-unnamed");
           ]
          @ if selected then [ "browser-row-selected" ] else []);
        Vdom.Attr.on_click (fun _ ->
            inject (State.Set_view (State.Detail h)));
      ]
    ([
       kind_badge;
       Vdom.Node.span
         ~attrs:[ Vdom.Attr.class_ "browser-leaf-label" ]
         [ Vdom.Node.text label ];
     ]
    @ alias_chips
    @ [
        Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "browser-leaf-hash mono" ]
          [ Vdom.Node.text (Hash.short ~len:6 h) ];
        Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "row-aspects" ]
          indicators;
        Vdom.Node.span
          ~attrs:[ Vdom.Attr.class_ "browser-leaf-body mono" ]
          [ Vdom.Node.text body ];
      ])

(* ===== Tree rendering with query-aware visibility ===== *)

let rec collect_visible_leaves t : Hash.t list =
  match t with
  | Leaf h -> [ h ]
  | Node children ->
      List.concat_map children ~f:(fun (_, sub) -> collect_visible_leaves sub)

let rec render_tree
    ~depth
    ~prefix
    ~(att : Attachment.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    ~(state : State.t)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~filtering
    (t : tree) : Vdom.Node.t list =
  match t with
  | Leaf _ -> []
  | Node children ->
      List.concat_map children ~f:(fun (seg, sub) ->
          let path = path_join prefix seg in
          match sub with
          | Leaf h ->
              if leaf_visible ~att ~store ~filter:state.filter ~ns h ~name:path
              then
                [
                  render_leaf_row ~att ~store ~ns ~state ~inject ~depth
                    ~label:seg h;
                ]
              else []
          | Node _ ->
              (* Recurse first; if no descendants are visible, drop the
                 prefix row entirely. When filtering is active, prefixes
                 auto-expand so the user sees every match. *)
              let descendant_leaves = collect_visible_leaves sub in
              let any_descendant_visible =
                List.exists descendant_leaves ~f:(fun h ->
                    let names = Namespace.names_of ns h in
                    List.exists names ~f:(fun name ->
                        let prefix_match =
                          String.is_prefix name ~prefix:(path ^ ".")
                          || String.equal name path
                        in
                        prefix_match
                        && leaf_visible ~att ~store ~filter:state.filter ~ns h
                             ~name))
              in
              if not any_descendant_visible then []
              else
                let is_open =
                  filtering
                  || List.exists state.expanded_paths ~f:(String.equal path)
                in
                let arrow = if is_open then "▾" else "▸" in
                let header =
                  Vdom.Node.div
                    ~attrs:
                      [
                        Vdom.Attr.style
                          (Css_gen.padding_left (`Px (8 + (depth * 14))));
                        Vdom.Attr.class_ "ns-tree-prefix";
                        Vdom.Attr.on_click (fun _ ->
                            inject (State.Toggle_path path));
                      ]
                    [
                      Vdom.Node.span
                        ~attrs:[ Vdom.Attr.class_ "ns-tree-arrow" ]
                        [ Vdom.Node.text arrow ];
                      Vdom.Node.text (seg ^ ".");
                    ]
                in
                if is_open then
                  header
                  :: render_tree ~depth:(depth + 1) ~prefix:path ~att
                       ~store ~ns ~state ~inject ~filtering sub
                else [ header ])

(* ===== Top-level filter widgets ===== *)

let scope_button
    ~(current : State.scope_filter)
    ~(target : State.scope_filter)
    ~(label : string)
    ~(state : State.t)
    ~(inject : State.action -> unit Vdom.Effect.t) : Vdom.Node.t =
  let selected = State.equal_scope_filter current target in
  Vdom.Node.button
    ~attrs:
      [
        Vdom.Attr.classes
          [ "btn-filter"; (if selected then "btn-selected" else "") ];
        Vdom.Attr.on_click (fun _ ->
            inject (State.Set_filter { state.filter with scope = target }));
      ]
    [ Vdom.Node.text label ]

(* ===== View ===== *)

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state
  and inject = inject in
  let ns = Substrate.global.ns in
  let store = Substrate.global.store in
  let att = Substrate.global.att in
  let filter = state.filter in
  let entries = Namespace.entries ns in
  let tree = build entries in
  let filtering =
    not (String.is_empty (String.strip filter.query))
    || Option.is_some filter.type_filter
  in
  let tree_rows =
    render_tree ~depth:0 ~prefix:"" ~att ~store ~ns ~state ~inject
      ~filtering tree
  in
  let named_hashes =
    entries |> List.map ~f:snd
    |> List.dedup_and_sort ~compare:String.compare
  in
  let all_hashes = Store.hashes store in
  let anon_hashes =
    match filter.scope with
    | Named_only -> []
    | All_hashes ->
        List.filter all_hashes ~f:(fun h ->
            not (List.exists named_hashes ~f:(Hash.equal h)))
        |> List.filter ~f:(fun h ->
               hash_matches_query ~ns h filter.query
               && type_matches ~att ~store h filter.type_filter)
        |> List.sort ~compare:(fun a b ->
               String.compare (Hash.short a) (Hash.short b))
  in
  let anonymous_section =
    if List.is_empty anon_hashes then Vdom.Node.none
    else
      let rows =
        List.map anon_hashes ~f:(fun h ->
            let kind = Store.kind_of store h in
            let body =
              try
                (match kind with
                 | Some Definition.Type_kind ->
                     Pretty.print_named_ty ~namespace:ns store h
                 | _ -> Pretty.print_named ~namespace:ns store h)
                |> one_line |> truncate_body
              with _ -> "(cannot render)"
            in
            let selected =
              match state.view with
              | State.Detail h' when Hash.equal h h' -> true
              | _ -> false
            in
            let kind_badge =
              match kind with
              | Some Definition.Type_kind ->
                  Vdom.Node.span
                    ~attrs:
                      [
                        Vdom.Attr.classes [ "kind-badge"; "kind-badge-type" ];
                        Vdom.Attr.title "type definition";
                      ]
                    [ Vdom.Node.text "T" ]
              | _ -> Vdom.Node.none
            in
            Vdom.Node.div
              ~attrs:
                [
                  Vdom.Attr.classes
                    ([ "browser-row"; "browser-row-unnamed" ]
                    @ if selected then [ "browser-row-selected" ] else []);
                  Vdom.Attr.style (Css_gen.padding_left (`Px 8));
                  Vdom.Attr.on_click (fun _ ->
                      inject (State.Set_view (State.Detail h)));
                ]
              [
                kind_badge;
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "browser-leaf-hash mono" ]
                  [ Vdom.Node.text (Hash.short ~len:8 h) ];
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "row-aspects" ]
                  (render_aspect_icons ~att ~store h);
                Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "browser-leaf-body mono" ]
                  [ Vdom.Node.text body ];
              ])
      in
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "browser-section" ]
        (Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "browser-section-label" ]
           [
             Vdom.Node.text
               (Printf.sprintf "(anonymous · %d)" (List.length anon_hashes));
           ]
        :: rows)
  in
  let stats = Attachment.stats att in
  let visible_named_count =
    List.length entries
    |> fun _ ->
    List.count entries ~f:(fun (name, h) ->
        leaf_visible ~att ~store ~filter ~ns h ~name)
  in
  let count_label =
    Printf.sprintf "%d shown · store %d · named %d · aspects %d"
      visible_named_count (List.length all_hashes)
      (Namespace.size ns) stats.entries
  in
  let reset_button =
    Vdom.Node.button
      ~attrs:
        [
          Vdom.Attr.class_ "btn-mini";
          Vdom.Attr.title
            "reload the page to reset the store and re-seed the bootstrap";
          Vdom.Attr.on_click (fun _ ->
              Js_of_ocaml.Dom_html.window##.location##reload;
              Vdom.Effect.Ignore);
        ]
      [ Vdom.Node.text "reset" ]
  in
  let clear_query_visible =
    not (String.is_empty (String.strip filter.query))
  in
  let active_type_filter =
    match filter.type_filter with
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
          ]
  in
  let body =
    if List.is_empty tree_rows && List.is_empty anon_hashes then
      [
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "ns-tree-empty" ]
          [
            Vdom.Node.text
              (if filtering then "no matches" else "no bindings yet");
          ];
      ]
    else tree_rows @ [ anonymous_section ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "browser-pane" ]
    [
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "browser-header" ]
        [
          Vdom.Node.h2
            ~attrs:[ Vdom.Attr.class_ "panel-title" ]
            [ Vdom.Node.text "browser" ];
          reset_button;
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "filter-row" ]
        [
          scope_button ~current:filter.scope ~target:Named_only
            ~label:"named only" ~state ~inject;
          scope_button ~current:filter.scope ~target:All_hashes
            ~label:"all hashes" ~state ~inject;
        ];
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "filter-row" ]
        [
          Vdom.Node.input
            ~attrs:
              [
                Vdom.Attr.type_ "text";
                Vdom.Attr.placeholder "search name or #hash…";
                Vdom.Attr.value filter.query;
                Vdom.Attr.on_input (fun _ s ->
                    inject
                      (State.Set_filter { state.filter with query = s }));
              ]
            ();
          (if clear_query_visible then
             Vdom.Node.button
               ~attrs:
                 [
                   Vdom.Attr.class_ "btn-mini";
                   Vdom.Attr.on_click (fun _ ->
                       inject
                         (State.Set_filter { state.filter with query = "" }));
                 ]
               [ Vdom.Node.text "×" ]
           else Vdom.Node.none);
        ];
      active_type_filter;
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "count-label" ]
        [ Vdom.Node.text count_label ];
      Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "browser-rows" ] body;
    ]
