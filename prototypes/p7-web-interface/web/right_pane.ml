(* Dispatches between Detail and Author based on state.view. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let%sub editor = Editor.view ~state ~inject in
  let%sub detail = Detail.view ~state ~inject in
  let%arr state = state
  and editor = editor
  and detail = detail in
  match state.view with
  | State.Author -> editor
  | State.Detail _ -> detail
