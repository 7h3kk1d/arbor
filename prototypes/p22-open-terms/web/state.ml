(* UI state. The substrate itself is not in here -- see substrate.ml. *)
open! Core

type selection =
  | Nothing_selected
  | Named of string (* a name in the namespace *)
[@@deriving sexp, equal]

type strategy =
  | Pin
  | Follow
  | Explicit
[@@deriving sexp, equal]

type t =
  { buffer : string
  ; version : int (* bumped on every substrate mutation, to force re-render *)
  ; message : string
  ; message_kind : string (* "ok" | "err" | "" *)
  ; selected : selection
  ; ns_filter : string
  ; strategy : strategy
  }
[@@deriving sexp, equal]

type action =
  | Set_buffer of string
  | Set_strategy of strategy
  | Commit
  | Select of selection
  | Set_ns_filter of string
  | Alias of string
  | Edit_into_buffer of string
[@@deriving sexp, equal]

let initial =
  { buffer = Substrate.seed_buffer
  ; version = 0
  ; message =
      "Press commit to store these three. Then change `base` to 2 and commit \
       again: scale and double pick up new links while their open hashes hold."
  ; message_kind = ""
  ; selected = Nothing_selected
  ; ns_filter = ""
  ; strategy = Follow
  }
