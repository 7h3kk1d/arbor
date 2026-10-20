(* The buffer. Centre pane.

   The per-slot table is this prototype's counterpart to p21's resolution chips,
   and it exists for the opposite reason. p21 shows which of five routes a
   variable took because three of them produce an identical `Var` node and the
   partiality is otherwise invisible in the output. Here the outcome IS the
   output -- it is a link target, a stored thing -- so the table is not
   recovering hidden information. It is showing you the object you are about to
   commit, before you commit it, which is possible because abstraction is a pure
   function of the text and its hash is already the hash the store will use. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax

let chip cls text =
  Vdom.Node.span ~attrs:[ Vdom.Attr.class_ ("chip " ^ cls) ] [ Vdom.Node.text text ]

let slot_table (slots : Ops.slot_row list) =
  if List.is_empty slots
  then
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "note" ]
      [ Vdom.Node.text "closed: no free variables, so the link is empty" ]
  else
    Vdom.Node.div
      ~attrs:[ Vdom.Attr.class_ "slots" ]
      (List.map slots ~f:(fun (r : Ops.slot_row) ->
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "slot" ]
           [ Vdom.Node.span
               ~attrs:[ Vdom.Attr.class_ "sidx" ]
               [ Vdom.Node.text (Printf.sprintf "%d" r.slot) ]
           ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "swrote" ] [ Vdom.Node.text r.name ]
           ; chip r.outcome r.outcome
           ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "starget" ] [ Vdom.Node.text r.target ]
           ]))

let preview_block ~inject (p : Ops.preview) =
  ignore inject;
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ (if p.ok then "prev ok" else "prev bad") ]
    [ Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "phead" ]
        [ Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "nm" ] [ Vdom.Node.text p.name ]
        ; chip (if p.is_edit then "edit" else "new") (if p.is_edit then "edit" else "new")
        ; (if p.in_cycle then chip "group" "in a cycle: bundled" else Vdom.Node.none)
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "oh" ]
            [ Vdom.Node.text ("open " ^ p.open_hash) ]
        ; Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "ar" ]
            [ Vdom.Node.text (Printf.sprintf "arity %d" p.arity) ]
        ]
    ; slot_table p.slots
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ (if p.ok then "status ok" else "status bad") ]
        [ Vdom.Node.text p.status ]
    ]

let strategy_radio ~inject ~(current : State.strategy) =
  let one label value describe =
    Vdom.Node.label
      ~attrs:[ Vdom.Attr.class_ (if State.equal_strategy current value then "strat on" else "strat") ]
      [ Vdom.Node.input
          ~attrs:
            [ Vdom.Attr.type_ "radio"
            ; Vdom.Attr.name "strategy"
            ; Vdom.Attr.on_click (fun _ -> inject (State.Set_strategy value))
            ]
          ()
      ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "sl" ] [ Vdom.Node.text label ]
      ; Vdom.Node.span ~attrs:[ Vdom.Attr.class_ "sd" ] [ Vdom.Node.text describe ]
      ]
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "strategies" ]
    [ one "pin" State.Pin "rewrite nothing"
    ; one "follow" State.Follow "rewrite every transitive caller"
    ; one "explicit" State.Explicit "rewrite nothing automatically"
    ]

let view ~(state : State.t Bonsai.Value.t) ~inject =
  let%arr state = state
  and inject = inject in
  ignore state.version;
  let a = Ops.analyze state.buffer in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "pane editor" ]
    [ Vdom.Node.h2 [ Vdom.Node.text "buffer" ]
    ; Vdom.Node.textarea
        ~attrs:
          [ Vdom.Attr.class_ "buffer"
          ; Vdom.Attr.value_prop state.buffer
          ; Vdom.Attr.on_input (fun _ v -> inject (State.Set_buffer v))
          ; Vdom.Attr.placeholder
              "base : Nat\nbase = 2\n\nEditing a name that already exists is \
               an edit, and propagates to its dependents."
          ]
        []
    ; strategy_radio ~inject ~current:state.strategy
    ; Vdom.Node.button
        ~attrs:
          ([ Vdom.Attr.class_ "commit"
           ; Vdom.Attr.on_click (fun _ -> inject State.Commit)
           ]
           @ (if List.is_empty a.previews then [ Vdom.Attr.disabled ] else []))
        [ Vdom.Node.text "commit" ]
    ; (if String.is_empty state.message
       then Vdom.Node.none
       else
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ ("msg " ^ state.message_kind) ]
           [ Vdom.Node.text state.message ])
    ; (if List.is_empty a.errors
       then Vdom.Node.none
       else
         Vdom.Node.div
           ~attrs:[ Vdom.Attr.class_ "errs" ]
           (List.map a.errors ~f:(fun e ->
              Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "err" ] [ Vdom.Node.text e ])))
    ; Vdom.Node.h3 [ Vdom.Node.text "what a commit would store" ]
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "previews" ]
        (List.map a.previews ~f:(preview_block ~inject))
    ; Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "note" ]
        [ Vdom.Node.text
            "The open hash above is the hash the store will use. It is a \
             function of the text alone -- no namespace, no resolution -- so it \
             is already known before anything is committed, and it does not \
             move when a dependency does."
        ]
    ]
