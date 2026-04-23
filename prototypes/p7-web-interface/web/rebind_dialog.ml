(* Modal shown when state.pending_rebind is Some. Lists direct callers
   of the old hash so the user sees which references remain pinned. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P7_web_interface_substrate

let direct_callers (store : Store.t) (target : Hash.t) : Hash.t list =
  let children_of (def : Definition.t) : Hash.t list =
    match def with
    | Lc (Var _) -> []
    | Lc (Lam h) -> [ h ]
    | Lc (App (f, a)) -> [ f; a ]
    | Stlc (Var _) | Stlc True | Stlc False -> []
    | Stlc (Lam (_, h)) -> [ h ]
    | Stlc (App (f, a)) -> [ f; a ]
    | Stlc (If (c, t, e)) -> [ c; t; e ]
  in
  Store.entries store
  |> List.filter_map ~f:(fun (h, def) ->
         let kids = children_of def in
         if List.mem kids target ~equal:Hash.equal then Some h else None)

let render_caller
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(store : Store.t)
    ~(ns : Namespace.t)
    (h : Hash.t) : Vdom.Node.t =
  let short = Hash.short h in
  let names = Namespace.names_of ns h in
  let names_str =
    if List.is_empty names then "" else " (" ^ String.concat ~sep:", " names ^ ")"
  in
  let body =
    try
      let s = Pretty.print_named ~namespace:ns store h in
      if String.length s > 60 then String.prefix s 60 ^ "…" else s
    with _ -> "(cannot render)"
  in
  Vdom.Node.li
    ~attrs:
      [
        Vdom.Attr.class_ "caller-row";
        Vdom.Attr.on_click (fun _ -> inject (State.Set_view (Detail h)));
      ]
    [
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "mono hash" ]
        [ Vdom.Node.text short ];
      Vdom.Node.span [ Vdom.Node.text names_str ];
      Vdom.Node.span
        ~attrs:[ Vdom.Attr.class_ "mono caller-body" ]
        [ Vdom.Node.text body ];
    ]

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%arr state = state
  and inject = inject in
  match state.State.pending_rebind with
  | None -> Vdom.Node.none
  | Some (name, old_hash, new_hash) ->
      let store = Substrate.global.store in
      let ns = Substrate.global.ns in
      let callers = direct_callers store old_hash in
      let old_short = Hash.short old_hash in
      let new_short = Hash.short new_hash in
      Vdom.Node.div
        ~attrs:
          [
            Vdom.Attr.class_ "rebind-overlay";
            Vdom.Attr.on_click (fun _ -> inject State.Cancel_rebind);
            Vdom.Attr.on_keydown (fun ev ->
                if ev##.keyCode = 27 then inject State.Cancel_rebind
                else Vdom.Effect.Ignore);
            Vdom.Attr.create "tabindex" "-1";
          ]
        [
          Vdom.Node.div
            ~attrs:
              [
                Vdom.Attr.class_ "rebind-modal";
                (* Stop click-propagation so clicking inside the modal
                   doesn't trip the overlay's dismiss handler. *)
                Vdom.Attr.on_click (fun ev ->
                    ignore
                      ((Js_of_ocaml.Js.Unsafe.coerce ev)##stopPropagation ());
                    Vdom.Effect.Ignore);
              ]
            [
              Vdom.Node.h3
                [
                  Vdom.Node.text
                    (Printf.sprintf "Rebind \"%s\" from %s to %s?" name
                       old_short new_short);
                ];
              Vdom.Node.p
                [
                  Vdom.Node.text
                    "Existing callers remain pinned to the old hash. \
                     Rebinding only moves the name; the old definition \
                     stays in the Store. This is the ";
                  Vdom.Node.em [ Vdom.Node.text "no silent breakage" ];
                  Vdom.Node.text " invariant.";
                ];
              (if List.is_empty callers then
                 Vdom.Node.p
                   ~attrs:[ Vdom.Attr.class_ "rebind-empty" ]
                   [
                     Vdom.Node.text
                       "No direct callers. The old hash becomes nameless \
                        but is unreferenced.";
                   ]
               else
                 Vdom.Node.div
                   [
                     Vdom.Node.p
                       [
                         Vdom.Node.text
                           (Printf.sprintf
                              "Direct callers that remain pinned to %s:"
                              old_short);
                       ];
                     Vdom.Node.ul
                       (List.map callers
                          ~f:(render_caller ~inject ~store ~ns));
                   ]);
              Vdom.Node.div
                ~attrs:[ Vdom.Attr.class_ "rebind-buttons" ]
                [
                  Vdom.Node.button
                    ~attrs:
                      [
                        Vdom.Attr.class_ "btn-secondary";
                        Vdom.Attr.on_click (fun _ ->
                            inject State.Cancel_rebind);
                      ]
                    [ Vdom.Node.text "cancel" ];
                  Vdom.Node.button
                    ~attrs:
                      [
                        Vdom.Attr.class_ "btn-primary";
                        Vdom.Attr.on_click (fun _ ->
                            inject State.Confirm_rebind);
                      ]
                    [ Vdom.Node.text "rebind" ];
                ];
            ];
        ]
