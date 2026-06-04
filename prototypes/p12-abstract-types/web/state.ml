(* Bonsai-visible model + action sum. Hashes are plain strings (Hash.t = string).
   [version] bumps on every substrate-mutating action so views re-render and
   re-read the Namespace / Store. [open_set] is the editing context — the set of
   abstract-type hashes currently transparent for authoring. *)

open! Core

type kind =
  | Normal
  | Sealed
[@@deriving sexp, equal]

type feedback =
  | Empty
  | Info of string
  | Err of string
  | Bound of { name : string; kind : kind; ty : string; hash : string }
  | Typed of { ty : string; value : string }
[@@deriving sexp, equal]

type t = {
  version : int;
  open_set : string list;  (* abstract-type hashes currently open *)
  selected : string option;  (* hash shown in the detail pane *)
  def_name : string;
  def_ty : string;
  def_expr : string;
  ty_name : string;
  ty_body : string;
  ty_abstract : bool;
  eval_expr : string;  (* the live scratch buffer *)
  scratch_fb : feedback;  (* live typecheck + eval of the scratch buffer; no binding *)
  feedback : feedback;  (* result of the last define / create-type *)
}
[@@deriving sexp, equal]

let initial : t =
  {
    version = 0;
    open_set = [];
    selected = None;
    def_name = "";
    def_ty = "";
    def_expr = "";
    ty_name = "";
    ty_body = "Int";
    ty_abstract = true;
    eval_expr = "";
    scratch_fb = Empty;
    feedback = Empty;
  }

type action =
  | Toggle_open of string
  | Select of string
  | Set_def_name of string
  | Set_def_ty of string
  | Set_def_expr of string
  | Set_ty_name of string
  | Set_ty_body of string
  | Toggle_ty_abstract
  | Set_eval_expr of string
  | Set_scratch_fb of feedback
  | Define
  | Create_type
[@@deriving sexp]
