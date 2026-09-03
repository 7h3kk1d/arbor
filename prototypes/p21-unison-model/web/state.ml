(* UI state. The substrate itself is not in here -- see substrate.ml. *)
open! Core

type selection =
  | Nothing_selected
  | Definition of string (* a hash *)
[@@deriving sexp, equal]

type t =
  { buffer : string
  ; version : int (* bumped on every substrate mutation, to force re-render *)
  ; message : string
  ; message_kind : string (* "ok" | "err" | "" *)
  ; selected : selection
  ; ns_filter : string
  }
[@@deriving sexp, equal]

type action =
  | Set_buffer of string
  | Commit
  | Select of selection
  | Set_ns_filter of string
  | Unbind of string
  | Edit_into_buffer of string (* a name: render its definition into the buffer *)
[@@deriving sexp, equal]

(* The seed buffer is saved once at startup, so the app opens in a real working
   state rather than an unsaved one. *)
let () = Substrate.save Substrate.seed_buffer

let initial =
  { buffer = Substrate.seed_buffer
  ; version = 0
  ; message = "Nothing committed yet. The store is empty."
  ; message_kind = ""
  ; selected = Nothing_selected
  ; ns_filter = ""
  }
