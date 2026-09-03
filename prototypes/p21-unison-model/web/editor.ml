(* The scratch buffer, and the pipeline it drives.

   The buffer is the centre pane; below it, every stage of the pipeline is shown
   as its own section, because the interesting part of this model is not the
   result but the route. In particular three of resolution's five outcomes
   produce the *same* output node, so nothing downstream can tell them apart —
   only the decision trace can. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P21_substrate

(* --- the stage rail ------------------------------------------------------ *)

let stages = [ Scratch.Parsed; Scratch.Resolved; Scratch.Typechecked; Scratch.Hashed ]

let stage_rail (sc : Scratch.t) =
  let reached s = Scratch.stage_index sc.stage >= Scratch.stage_index s in
    let cls s =
    if not (reached s)
    then "step"
    else if Scratch.stage_index s = Scratch.stage_index Scratch.Hashed
    then "step hash"
    else "step done"
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "rail" ]
    (Vdom.Node.span
       ~attrs:[ Vdom.Attr.class_ "step done" ]
       [ Vdom.Node.text "text" ]
     :: List.concat_map stages ~f:(fun s ->
       [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "arrow" ] [ Vdom.Node.text "→" ]
       ; Vdom.Node.span
           ~attrs:[ Vdom.Attr.class_ (cls s) ]
           [ Vdom.Node.text (Scratch.stage_label s) ]
       ])
     @ [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "arrow" ] [ Vdom.Node.text "⇢" ]
       ; Vdom.Node.span
           ~attrs:[ Vdom.Attr.class_ "step commit" ]
           [ Vdom.Node.text "stored + named" ]
       ])

(* --- resolution decisions ------------------------------------------------ *)

let outcome_class (o : Resolve.outcome) =
  match o with
  | Resolve.Local -> "o-local"
  | Resolve.Group -> "o-group"
  | Resolve.Builtin _ -> "o-builtin"
  | Resolve.Resolved _ -> "o-resolved"
  | Resolve.Unresolved -> "o-unresolved"
  | Resolve.Ambiguous _ -> "o-ambiguous"

let outcome_detail (o : Resolve.outcome) =
  match o with
  | Resolve.Resolved r -> Reference.to_string r
  | Resolve.Builtin b -> "##" ^ b
  | Resolve.Group -> "stays a variable — no hash yet"
  | Resolve.Local -> "λ-bound"
  | Resolve.Unresolved -> "stays free — becomes a type error"
  | Resolve.Ambiguous _ -> "stays free — left for the typechecker"

let decision_chip ((d : Resolve.decision), n) =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ ("chip " ^ outcome_class d.outcome) ]
    ([ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "cname" ] [ Vdom.Node.text d.name ]
     ; Vdom.Node.span
         ~attrs:[ Vdom.Attr.class_ "ckind" ]
         [ Vdom.Node.text (Resolve.outcome_label d.outcome) ]
     ]
     @ (if n > 1
        then
          [ Vdom.Node.span
              ~attrs:[ Vdom.Attr.class_ "ccount" ]
              [ Vdom.Node.text ("×" ^ Int.to_string n) ]
          ]
        else [])
     @ [ Vdom.Node.span
           ~attrs:[ Vdom.Attr.class_ "cdetail" ]
           [ Vdom.Node.text (outcome_detail d.outcome) ]
       ])

(* --- per-binding ---------------------------------------------------------- *)

let binding_block ~ppe (b : Scratch.binding) =
  let ok = Option.is_none b.error in
  let verdict =
    match b.error with
    | Some m -> "type error: " ^ m
    | None ->
      (match b.status with
       | Slurp.New -> "new"
       | Slurp.Redefines r -> "redefines " ^ Reference.to_string r
       | Slurp.Duplicate _ -> "unchanged")
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ (if ok then "bind" else "bind err") ]
    [ Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "bhead" ]
        [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "nm" ] [ Vdom.Node.text b.name ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "ty" ]
            [ Vdom.Node.text (": " ^ Ty.to_string b.ty) ]
        ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "vd" ] [ Vdom.Node.text verdict ]
        ]
    ; (match b.reference with
       | None -> Vdom.Node.none
       | Some r ->
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "brow" ]
           [ Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "k" ]
               [ Vdom.Node.text "hash at save" ]
           ; Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "v mono hashv" ]
               [ Vdom.Node.text (Reference.to_string r) ]
           ])
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "brow" ]
        [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "k" ] [ Vdom.Node.text "resolved" ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "v mono" ]
            [ Vdom.Node.text (Pretty.term ppe b.resolved) ]
        ]
    ; (if List.is_empty b.free
       then Vdom.Node.none
       else
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "brow" ]
           [ Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "k" ]
               [ Vdom.Node.text "still free" ]
           ; Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "v mono" ]
               [ Vdom.Node.text (String.concat ~sep:", " b.free) ]
           ])
    ; (if List.is_empty b.decisions
       then Vdom.Node.none
       else
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "chips" ]
           (List.map b.decisions ~f:(fun d -> decision_chip (d, 1))))
    ]

let watch_block ~ppe ~value (w : Scratch.watch) =
  Vdom.Node.div
    ~attrs:
      [ Vdom.Attr.class_ (if Option.is_none w.error then "bind watch" else "bind err") ]
    [ Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "bhead" ]
        [ Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "nm mono" ]
            [ Vdom.Node.text ("> " ^ Pretty.term ppe w.resolved) ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "ty" ]
            [ Vdom.Node.text
                (match w.ty with
                 | Some t -> ": " ^ Ty.to_string t
                 | None -> "")
            ]
        ; (match w.reference with
           | None -> Vdom.Node.none
           | Some r ->
             Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "badge cachekey" ]
               [ Vdom.Node.text (Reference.to_string r) ])
        ; (match w.reference with
           | None -> Vdom.Node.none
           | Some _ ->
             Vdom.Node.span
               ~attrs:
                 [ Vdom.Attr.class_ (if w.cache_hit then "badge hit" else "badge miss") ]
               [ Vdom.Node.text (if w.cache_hit then "cache hit" else "evaluated") ])
        ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "vd" ] [ Vdom.Node.text value ]
        ]
    ; (if List.is_empty w.decisions
       then Vdom.Node.none
       else
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "chips" ]
           (List.map w.decisions ~f:(fun d -> decision_chip (d, 1))))
    ]

(* --- SCC preview ---------------------------------------------------------- *)

let component_block (c : Scratch.component) =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ (if c.recursive then "scc rec" else "scc") ]
    [ Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "nm mono" ]
        [ Vdom.Node.text (String.concat ~sep:", " c.members) ]
    ; Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "vd" ]
        [ Vdom.Node.text
            (match c.members, c.recursive with
             | [ _ ], false -> "singleton — its own hash"
             | [ _ ], true -> "self-recursive — hashed as a cycle of 1"
             | ms, _ ->
               Printf.sprintf
                 "cycle of %d — one hash, %d positions"
                 (List.length ms)
                 (List.length ms))
        ]
    ]

(* --- the pane ------------------------------------------------------------- *)

let section title subtitle body =
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "stage" ]
    (Vdom.Node.h3 [ Vdom.Node.text title ]
     :: Vdom.Node.p ~attrs:[ Vdom.Attr.class_ "hint" ] [ Vdom.Node.text subtitle ]
     :: body)

let view ~(state : State.t Bonsai.Value.t) ~inject =
  let%arr state = state
  and inject = inject in
  let s = Substrate.global in
  (* the value the last save produced — not recomputed here *)
  let sc = !Substrate.scratch in
  let ppe = Ppe.make s.ns in
  let errors =
    List.map sc.parse_errors ~f:(fun (e : Parse.located_error) ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "row err" ]
        [ Vdom.Node.text (Printf.sprintf "line %d: %s" e.line e.message) ])
    @ List.map sc.problems ~f:(fun p ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "row err" ]
        [ Vdom.Node.text (Slurp.problem_to_string p) ])
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "editor-pane" ]
    [ Vdom.Node.h2 [ Vdom.Node.text "scratch buffer" ]
    ; Vdom.Node.p
        ~attrs:[ Vdom.Attr.class_ "hint" ]
        [ Vdom.Node.text
            "Typing is saving. Every keystroke typechecks the buffer, hashes \
             it, and evaluates its watches into the cache — exactly what \
             writing scratch.u does, and with no commit. Identity is conferred \
             by typechecking; storing and naming are the separate gesture \
             below."
        ]
    ; Vdom.Node.textarea
        ~attrs:
          [ Vdom.Attr.class_ "buffer"
          ; Vdom.Attr.value_prop state.buffer
          ; Vdom.Attr.create "spellcheck" "false"
          ; Vdom.Attr.create "autocapitalize" "off"
          ; Vdom.Attr.create "autocomplete" "off"
          ; Vdom.Attr.on_input (fun _ s -> inject (State.Set_buffer s))
          ]
        []
    ; stage_rail sc
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "actions" ]
        [ Vdom.Node.button
            ~attrs:
              ([ Vdom.Attr.on_click (fun _ -> inject State.Commit) ]
               @ if Scratch.is_committable sc then [] else [ Vdom.Attr.disabled ])
            [ Vdom.Node.text "commit" ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "hint inline" ]
            [ Vdom.Node.text
                (if Scratch.is_committable sc
                 then "already hashed — commit stores it and binds the names"
                 else "nothing is hashed until the whole buffer typechecks")
            ]
        ]
    ; (if String.is_empty state.message
       then Vdom.Node.none
       else
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ ("message " ^ state.message_kind) ]
           [ Vdom.Node.text state.message ])
    ; (if List.is_empty errors
       then Vdom.Node.none
       else section "1 · parse" "Fail-fast, one stanza at a time." errors)
    ; (if List.is_empty sc.bindings && List.is_empty sc.watches
       then Vdom.Node.none
       else
         section
           "2 · resolve, then typecheck"
           "Each variable takes one of five routes. Three of them leave a \
            variable in place and are indistinguishable in the output term — \
            which is why they are labelled here."
           (List.map sc.bindings ~f:(binding_block ~ppe)
            @ List.map sc.watches ~f:(fun w ->
              watch_block ~ppe ~value:(Ops.watch_value w) w)))
    ; (if List.is_empty sc.components
       then Vdom.Node.none
       else
         section
           "3 · components"
           "What ingest would hash, in dependency order. Members of one cycle \
            share a hash and are told apart by position."
           (List.map sc.components ~f:component_block))
    ; (if String.is_empty (String.strip sc.below_fold)
       then Vdom.Node.none
       else
         section
           "below the fold"
           "Inert. Everything after a `---` line is invisible to the parser."
           [ Vdom.Node.pre
               ~attrs:[ Vdom.Attr.class_ "fold" ]
               [ Vdom.Node.text sc.below_fold ]
           ])
    ]
