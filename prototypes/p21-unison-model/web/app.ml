(* Three panes: codebase, scratch buffer, component detail. *)
open! Core
open! Bonsai_web
open Bonsai.Let_syntax

module Action = struct
  type t = State.action [@@deriving sexp, equal]
end

let apply_action ~inject:_ ~schedule_event:_ (m : State.t) (a : State.action) : State.t =
  let s = Substrate.global in
  let bump m = { m with State.version = m.State.version + 1 } in
  match a with
  (* Typing IS saving. In Unison, writing scratch.u typechecks the file, hashes
     it, evaluates its watches and populates the cache — with no commit. So a
     keystroke here does the same, and `commit` remains a separate gesture that
     stores and names. *)
  | State.Set_buffer b ->
    Substrate.save b;
    bump { m with buffer = b }
  | State.Set_ns_filter f -> { m with ns_filter = f }
  | State.Select sel -> { m with selected = sel }
  | State.Unbind n ->
    P21_substrate.Names.remove_name s.ns n;
    bump { m with message = "unbound " ^ n ^ ". The definition is still in the store — a name ceasing to resolve is the only kind of deletion there is."; message_kind = "" }
  | State.Edit_into_buffer n ->
    let b = Ops.edit_into_buffer ~s ~buffer:m.buffer ~name:n in
    Substrate.save b;
    bump { m with buffer = b }
  | State.Commit ->
    let r = Ops.commit ~s ~buffer:m.buffer in
    let m =
      match r.new_buffer with
      | Some b ->
        Substrate.save b;
        { m with State.buffer = b }
      | None -> m
    in
    Substrate.save m.State.buffer;
    bump { m with message = r.message; message_kind = r.kind }

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
