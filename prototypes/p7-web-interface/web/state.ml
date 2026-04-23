(* Bonsai-visible model + action sum. Every action that mutates
   Substrate.global bumps [version]; views key their memoization off it. *)

open! Core
open P7_web_interface_substrate

(* Hash.t is an opaque string at the substrate API boundary; wrap it
   here for sexp/equal so Bonsai-derived types can use it directly. *)
module Hash_m = struct
  type t = Hash.t

  let sexp_of_t h = Sexp.Atom (Hash.to_string h)

  let t_of_sexp = function
    | Sexp.Atom s -> s
    | _ -> failwith "hash expects atom"

  let equal = Hash.equal
end

type view = Detail of Hash_m.t | Author [@@deriving sexp, equal]

type author_lang = Lc | Stlc [@@deriving sexp, equal]

type scope_filter = Named_only | All_hashes [@@deriving sexp, equal]

type lang_filter = All | Only_lc | Only_stlc [@@deriving sexp, equal]

type filter = {
  lang : lang_filter;
  scope : scope_filter;
  query : string;
  type_filter : string option; (* printed type, e.g. "Bool -> Bool" *)
}
[@@deriving sexp, equal]

type feedback =
  | Empty
  | Parse_error of string
  | Resolve_error of string
  | Ok_ of {
      hash : Hash_m.t;
      was_new : bool;
      typ_str : string option; (* pre-rendered Ty.print result for stlc *)
      resolved : (string * Hash_m.t) list;
    }
[@@deriving sexp, equal]

type t = {
  version : int;
  view : view;
  nav_back : view list; (* history stack: most recent at head *)
  filter : filter;
  author_lang : author_lang;
  author_buffer : string;
  author_bind_as : string;
  auto_eval : bool;
  auto_typecheck : bool; (* only meaningful in stlc *)
  feedback : feedback;
  pending_rebind : (string * Hash_m.t * Hash_m.t) option;
      (* name, old_hash, new_hash *)
}
[@@deriving sexp, equal]

let initial : t =
  {
    version = 0;
    view = Author;
    nav_back = [];
    filter = { lang = All; scope = Named_only; query = ""; type_filter = None };
    author_lang = Stlc;
    author_buffer = "";
    author_bind_as = "";
    auto_eval = false;
    auto_typecheck = false;
    feedback = Empty;
    pending_rebind = None;
  }

type action =
  | Set_view of view
  | Go_back
  | Set_filter of filter
  | Set_author_lang of author_lang
  | Set_author_buffer of string
  | Set_bind_as of string
  | Set_auto_eval of bool
  | Set_auto_typecheck of bool
  | Filter_by_type of string option
  | Feedback_updated of feedback
  | Bind_current
  | Bind_existing of { name : string; hash : Hash_m.t }
  | Unbind of string
  | Request_rebind of { name : string; new_hash : Hash_m.t }
  | Confirm_rebind
  | Cancel_rebind
  | Evaluate of Hash_m.t
  | Typecheck of Hash_m.t
  | Translate_stlc_to_lc of Hash_m.t
  | Translate_lc_to_stlc of Hash_m.t * string (* expected type source *)
[@@deriving sexp]
