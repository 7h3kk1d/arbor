(* Bonsai-visible model + action sum. Hashes are plain strings (Hash.t = string).
   [version] bumps on every substrate-mutating action so views re-render and
   re-read the Namespace / Store. [open_set] is the editing context — the set of
   abstract-type hashes currently transparent for authoring. *)

open! Core

type kind =
  | Normal
  | Sealed
[@@deriving sexp, equal]

(* Which definition sorts the browser shows. [`All] is the default; the others
   keep only bindings whose definition is of that sort. *)
type sort_filter =
  [ `All
  | `Terms
  | `Types
  | `Labels
  ]
[@@deriving sexp, equal]

(* One component of a module's sig, for the open form. Every component is
   labeled in p17 (the sig's own labels), so opening needs NO user-supplied
   names — the form just previews what `N.<cname>` will bind. *)
type open_comp = {
  cname : string;
  cty : string;  (* "type" / "type = Int" for type components; the member type otherwise *)
}
[@@deriving sexp, equal]

(* The shape of a module (a Sig-typed value), for driving the open form. *)
type open_shape = {
  type_comps : open_comp list;
  fields : open_comp list;
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
  sort_filter : sort_filter;  (* definition-sort filter (terms / types / labels) *)
  ty_name : string;
  ty_body : string;
  ty_abstract : bool;
  (* the merged work area: one expression buffer, evaluated live, that can be
     bound (with an optional annotation) or opened as a module *)
  work_expr : string;  (* the live expression buffer *)
  work_fb : feedback;  (* live typecheck + eval of [work_expr]; no binding *)
  work_name : string;  (* name to bind / module name to open as *)
  work_ty : string;  (* optional annotation; synthesized if blank *)
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
    sort_filter = `All;
    ty_name = "";
    ty_body = "Int";
    ty_abstract = true;
    work_expr = "";
    work_fb = Empty;
    work_name = "";
    work_ty = "";
    sel_open_name = "";
    feedback = Empty;
  }

type action =
  | Toggle_open of string
  | Toggle_collapse of string
  | Set_ns_filter of string
  | Set_sort_filter of sort_filter
  | Unbind of string  (* delete a name; the definition stays in the store *)
  | Select of string
  | Set_ty_name of string
  | Set_ty_body of string
  | Toggle_ty_abstract
  | Set_work_expr of string
  | Set_work_fb of feedback
  | Set_work_name of string
  | Set_work_ty of string
  | Toggle_test of string
  | Set_sel_open_name of string
  | Open_module  (* open the work-area expression as a module *)
  | Open_selected  (* open the selected detail term as a module *)
  | Define
  | Create_type
[@@deriving sexp]
