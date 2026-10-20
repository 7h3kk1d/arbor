(* The selected definition, and its three histories. Right pane.

   This view is the argument. Every other pane could exist in p21 in some form;
   this one could not, because it shows two things a single commit log cannot
   separate: how many times the AUTHOR changed this definition, and how many
   times it changed because something underneath it did.

   Unison answers that question with synhash -- rehash the definition with names
   substituted for references, quotienting out the dependencies' identities, and
   compare -- which is a reconstruction, and whose own source says it "cannot
   handle renames very well" because the quotient is by name. p11's mint threads
   answer it with an asserted mark that has to survive edits. Here it is two
   hash comparisons over a log, and it survives renames because no name appears
   in either hash. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P22_substrate

let row cls cells =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ cls ]
    (List.map cells ~f:(fun (c, txt) ->
       Vdom.Node.span ~attrs:[ Vdom.Attr.class_ c ] [ Vdom.Node.text txt ]))

(* One timeline. `mark` says which versions belong to this history, so all three
   can be read against the same column of versions. *)
let timeline ~title ~explain ~(all : Oplog.version list) ~(mark : Oplog.version list) =
  let is_marked (v : Oplog.version) =
    List.exists mark ~f:(fun (m : Oplog.version) -> m.Oplog.seq = v.Oplog.seq)
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "timeline" ]
    [ Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "thead" ]
        [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "tt" ] [ Vdom.Node.text title ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "tc" ]
            [ Vdom.Node.text (Printf.sprintf "%d" (List.length mark)) ]
        ]
    ; Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "texplain" ] [ Vdom.Node.text explain ]
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "trows" ]
        (List.map all ~f:(fun (v : Oplog.version) ->
           row
             (if is_marked v then "trow on" else "trow off")
             [ "dot", (if is_marked v then "\xe2\x97\x8f" else "\xc2\xb7")
             ; "seq", Printf.sprintf "%d" v.Oplog.seq
             ; "lbl", v.Oplog.label
             ; "oh", Hash.short ~len:7 v.Oplog.open_hash
             ; "lh", Hash.short ~len:7 v.Oplog.link_hash
             ]))
    ]

let view ~(state : State.t Bonsai.Value.t) ~inject =
  let%arr state = state
  and inject = inject in
  ignore state.version;
  let s = Substrate.global in
  match state.selected with
  | State.Nothing_selected ->
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "pane detail" ]
      [ Vdom.Node.h2 [ Vdom.Node.text "detail" ]
      ; Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "note" ]
          [ Vdom.Node.text "Select a name on the left." ]
      ]
  | State.Named name ->
    let h = Namespace.resolve s.Session.ns name in
    let hist = Session.histories_of s name in
    let versions = hist.Session.versions in
    let header =
      match h with
      | None ->
        Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "note" ] [ Vdom.Node.text "unbound" ]
      | Some h ->
        let d = Store.get_def s.Session.store h in
        let open_h, link_h =
          match d with
          | Some (Def.Simple (o, l)) -> Hash.short ~len:10 o, Hash.short ~len:10 l
          | Some (Def.Member (b, i)) ->
            Printf.sprintf "member %d of %s" i (Hash.short ~len:8 b), "(in bundle)"
          | None -> "", ""
        in
        let ty =
          match Store.type_of s.Session.store h with
          | Some t -> Ty.to_string t
          | None -> "?"
        in
        let value =
          match Session.eval_name s name with
          | Ok v -> Eval.to_string v
          | Error m -> m
        in
        let aliases =
          List.filter (Namespace.names_of s.Session.ns h) ~f:(fun n ->
            not (String.equal n name))
        in
        let deps =
          match d with
          | Some d ->
            (match Store.link_of s.Session.store d with
             | Some l ->
               List.mapi (Link.dependencies l) ~f:(fun i dep ->
                 let nm =
                   match Namespace.names_of s.Session.ns dep with
                   | n :: _ -> n
                   | [] -> Hash.short ~len:8 dep
                 in
                 ignore i;
                 nm)
             | None -> [])
          | None -> []
        in
        Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "dhead" ]
          [ row "drow" [ "k", "definition"; "v", Hash.short ~len:10 h ]
          ; row "drow" [ "k", "open term"; "v", open_h ]
          ; row "drow" [ "k", "link"; "v", link_h ]
          ; row "drow" [ "k", "type"; "v", ty ]
          ; row "drow" [ "k", "value"; "v", value ]
          ; row
              "drow"
              [ "k", "depends on"
              ; ( "v"
                , if List.is_empty deps then "\xe2\x80\x94" else String.concat ~sep:", " deps )
              ]
          ; row
              "drow"
              [ "k", "also named"
              ; ( "v"
                , if List.is_empty aliases
                  then "\xe2\x80\x94"
                  else String.concat ~sep:", " aliases )
              ]
          ]
    in
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "pane detail" ]
      [ Vdom.Node.h2 [ Vdom.Node.text name ]
      ; header
      ; Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "dbuttons" ]
          [ Vdom.Node.button
              ~attrs:
                [ Vdom.Attr.on_click (fun _ -> inject (State.Edit_into_buffer name)) ]
              [ Vdom.Node.text "edit into buffer" ]
          ; Vdom.Node.button
              ~attrs:[ Vdom.Attr.on_click (fun _ -> inject (State.Alias name)) ]
              [ Vdom.Node.text "alias as <name>_alias" ]
          ]
      ; Vdom.Node.h3 [ Vdom.Node.text "three histories" ]
      ; Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "note" ]
          [ Vdom.Node.text
              (Printf.sprintf
                 "%d versions in all. A single commit log would show that number \
                  and stop there."
                 (List.length versions))
          ]
      ; timeline
          ~title:"authored edits"
          ~explain:"versions where the OPEN term moved: someone changed the code"
          ~all:versions
          ~mark:hist.Session.authored
      ; timeline
          ~title:"link moves"
          ~explain:
            "versions where the LINK moved: something underneath it changed"
          ~all:versions
          ~mark:hist.Session.links
      ; Vdom.Node.div
          ~attrs:[ Vdom.Attr.class_ "timeline" ]
          [ Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "thead" ]
              [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "tt" ] [ Vdom.Node.text "name history" ]
              ; Vdom.Node.span
                  ~attrs:[ Vdom.Attr.class_ "tc" ]
                  [ Vdom.Node.text (Printf.sprintf "%d" (List.length hist.Session.names)) ]
              ]
          ; Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "texplain" ]
              [ Vdom.Node.text "bind, rebind, unbind: what this name has meant" ]
          ; Vdom.Node.div
              ~attrs:[ Vdom.Attr.class_ "trows" ]
              (List.map hist.Session.names ~f:(fun (e : Oplog.entry) ->
                 row
                   "trow on"
                   [ "dot", "\xe2\x97\x8f"
                   ; "seq", Printf.sprintf "%d" e.Oplog.seq
                   ; "lbl", Oplog.event_to_string e.Oplog.event
                   ]))
          ]
      ]
