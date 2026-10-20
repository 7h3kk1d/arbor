(* Three panes: codebase, buffer, detail. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P22_substrate

module Action = struct
  type t = State.action [@@deriving sexp, equal]
end

let strategy_of (s : State.strategy) : Propagate.strategy =
  match s with
  | State.Pin -> Propagate.Pin
  | State.Follow -> Propagate.Follow
  | State.Explicit -> Propagate.Explicit []

let apply_action ~inject:_ ~schedule_event:_ (m : State.t) (a : State.action) : State.t =
  let s = Substrate.global in
  let bump m = { m with State.version = m.State.version + 1 } in
  match a with
  | State.Set_buffer b -> { m with buffer = b }
  | State.Set_strategy st -> { m with strategy = st }
  | State.Set_ns_filter f -> { m with ns_filter = f }
  | State.Select sel -> { m with selected = sel }
  | State.Edit_into_buffer n ->
    { m with buffer = Ops.edit_into_buffer ~buffer:m.State.buffer ~name:n }
  | State.Alias n ->
    (match Session.alias s ~existing:n ~new_name:(n ^ "_alias") with
     | Ok () ->
       bump
         { m with
           message =
             n
             ^ " now has a second name. Nothing about propagation changes: an \
                alias is a namespace fact, and propagation rewrites links."
         ; message_kind = "ok"
         }
     | Error e -> bump { m with message = e; message_kind = "err" })
  | State.Commit ->
    let message, kind = Ops.commit ~strategy:(strategy_of m.State.strategy) m.State.buffer in
    bump { m with message; message_kind = kind }

let component : Vdom.Node.t Bonsai.Computation.t =
  let%sub state_and_inject =
    Bonsai.state_machine0
      (module struct
        type t = State.t [@@deriving sexp, equal]
      end)
      (module Action)
      ~default_model:State.initial
      ~apply_action
  in
  let%sub state =
    let%arr s, _ = state_and_inject in
    s
  in
  let%sub inject =
    let%arr _, f = state_and_inject in
    f
  in
  let%sub browser = Browser.view ~state ~inject in
  let%sub editor = Editor.view ~state ~inject in
  let%sub detail = Detail.view ~state ~inject in
  let%arr browser = browser
  and editor = editor
  and detail = detail in
  Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "app" ] [ browser; editor; detail ]
