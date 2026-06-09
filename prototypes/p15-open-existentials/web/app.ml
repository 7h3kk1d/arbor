(* Top-level Bonsai computation. Three-pane layout: namespace browser (left),
   editor + editing-context (center), detail (right). Substrate-mutating actions
   run the Ops pipeline and bump [version] so the views re-read fresh state. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P15_substrate

module Action = struct
  type t = State.action [@@deriving sexp_of]
end

(* set the i-th element of a positional name list, padding with "" as needed *)
let list_set (lst : string list) (i : int) (v : string) : string list =
  let n = List.length lst in
  if i < n then List.mapi lst ~f:(fun j x -> if j = i then v else x)
  else lst @ List.init (i - n) ~f:(fun _ -> "") @ [ v ]

let apply_action ~inject:_ ~schedule_event:_ (m : State.t) (a : State.action) : State.t =
  let bump (m : State.t) = { m with version = m.version + 1 } in
  let s = Substrate.global in
  match a with
  | State.Toggle_open h ->
      let open_set =
        if List.mem m.open_set h ~equal:String.equal then
          List.filter m.open_set ~f:(fun x -> not (String.equal x h))
        else h :: m.open_set
      in
      { m with open_set }
  | State.Toggle_collapse path ->
      let collapsed =
        if List.mem m.collapsed path ~equal:String.equal then
          List.filter m.collapsed ~f:(fun x -> not (String.equal x path))
        else path :: m.collapsed
      in
      { m with collapsed }
  | State.Set_ns_filter v -> { m with ns_filter = v }
  | State.Select h -> { m with selected = Some h }
  | State.Set_ty_name v -> { m with ty_name = v }
  | State.Set_ty_body v -> { m with ty_body = v }
  | State.Toggle_ty_abstract -> { m with ty_abstract = not m.ty_abstract }
  | State.Set_work_expr v -> { m with work_expr = v }
  | State.Set_work_fb fb -> { m with work_fb = fb }
  | State.Set_work_name v -> { m with work_name = v }
  | State.Set_work_ty v -> { m with work_ty = v }
  | State.Set_open_type_name (i, v) ->
      { m with open_type_names = list_set m.open_type_names i v }
  | State.Set_open_field_name (i, v) ->
      { m with open_field_names = list_set m.open_field_names i v }
  | State.Set_sel_open_name v -> { m with sel_open_name = v }
  | State.Open_existential ->
      let fb =
        Ops.open_existential ~s ~name:m.work_name ~type_names:m.open_type_names
          ~field_names:m.open_field_names ~expr:m.work_expr
      in
      bump { m with feedback = fb }
  | State.Open_selected ->
      let fb =
        match m.selected with
        | None -> State.Err "no definition selected"
        | Some h ->
            Ops.open_existential_node ~s ~name:m.sel_open_name ~type_names:[]
              ~field_names:[] ~node:(Node.Ref h)
      in
      bump { m with feedback = fb }
  | State.Toggle_test h ->
      let att = Substrate.global.att in
      if Attachment.has att ~aspect:"test" h then Attachment.unmark att ~aspect:"test" h
      else Attachment.mark att ~aspect:"test" h;
      bump m
  | State.Define ->
      let fb =
        Ops.define ~s ~open_set:m.open_set ~name:m.work_name ~ty:m.work_ty ~expr:m.work_expr
      in
      bump { m with feedback = fb }
  | State.Create_type ->
      let fb, opened = Ops.create_type ~s ~name:m.ty_name ~body:m.ty_body ~abstract:m.ty_abstract in
      let open_set =
        match opened with
        | Some h when not (List.mem m.open_set h ~equal:String.equal) -> h :: m.open_set
        | _ -> m.open_set
      in
      bump { m with feedback = fb; open_set }

let component : Vdom.Node.t Bonsai.Computation.t =
  let%sub state_and_inject =
    Bonsai.state_machine0
      (module struct
        type t = State.t [@@deriving sexp, equal]
      end)
      (module Action)
      ~default_model:State.initial ~apply_action
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
  let%arr browser = browser and editor = editor and detail = detail in
  Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "app" ] [ browser; editor; detail ]
