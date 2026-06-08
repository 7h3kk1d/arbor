(* Bonsai-visible model + action sum. Hashes are plain strings (Hash.t = string).
   [version] bumps on every substrate-mutating action so views re-render and
   re-read the Namespace / Store. [open_set] is the editing context — the set of
   abstract-type hashes currently transparent for authoring. *)

open! Core

type kind =
  | Normal
  | Sealed
[@@deriving sexp, equal]

(* One operation field of an existential's interface, for the open form: its
   pretty-printed type, and — when the interface is a record — the field's label
   name, used to pre-fill (and default) the field's binding name. *)
type open_field = {
  flabel : string option;
  fty : string;
}
[@@deriving sexp, equal]

(* The shape of an existential package, for driving the open form: how many
   abstract types it hides, and its operation fields — so the editor can render
   one input per type and per field. *)
type open_shape = {
  type_arity : int;
  fields : open_field list;
}
[@@deriving sexp, equal]

type feedback =
  | Empty
  | Info of string
  | Err of string
  | Bound of { name : string; kind : kind; ty : string; hash : string }
  | Typed of { ty : string; value : string; open_shape : open_shape option }
[@@deriving sexp, equal]

type t = {
  version : int;
  open_set : string list;  (* abstract-type hashes currently open *)
  selected : string option;  (* hash shown in the detail pane *)
  collapsed : string list;  (* namespace section path-prefixes collapsed in the browser *)
  ns_filter : string;  (* substring filter over the whole namespace *)
  ty_name : string;
  ty_body : string;
  ty_abstract : bool;
  (* the merged work area: one expression buffer, evaluated live, that can be
     bound (with an optional annotation) or opened as a module *)
  work_expr : string;  (* the live expression buffer *)
  work_fb : feedback;  (* live typecheck + eval of [work_expr]; no binding *)
  work_name : string;  (* name to bind / module name to open as *)
  work_ty : string;  (* optional annotation; synthesized if blank *)
  open_type_names : string list;  (* per-abstract-type names, positional, for open *)
  open_field_names : string list;  (* per-operation field names, positional, for open *)
  sel_open_name : string;  (* module name for opening the selected detail term *)
  feedback : feedback;  (* result of the last bind / create-type / open *)
}
[@@deriving sexp, equal]

let initial : t =
  {
    version = 0;
    open_set = [];
    selected = None;
    collapsed = [];
    ns_filter = "";
    ty_name = "";
    ty_body = "Int";
    ty_abstract = true;
    work_expr = "";
    work_fb = Empty;
    work_name = "";
    work_ty = "";
    open_type_names = [];
    open_field_names = [];
    sel_open_name = "";
    feedback = Empty;
  }

type action =
  | Toggle_open of string
  | Toggle_collapse of string
  | Set_ns_filter of string
  | Select of string
  | Set_ty_name of string
  | Set_ty_body of string
  | Toggle_ty_abstract
  | Set_work_expr of string
  | Set_work_fb of feedback
  | Set_work_name of string
  | Set_work_ty of string
  | Set_open_type_name of int * string
  | Set_open_field_name of int * string
  | Toggle_test of string
  | Set_sel_open_name of string
  | Open_existential  (* open the work-area expression as a module *)
  | Open_selected  (* open the selected detail term as a module *)
  | Define
  | Create_type
[@@deriving sexp]
