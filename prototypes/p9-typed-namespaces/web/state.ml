(* Bonsai-visible model + action sum. Every action that mutates
   Substrate.global bumps [version]; views key their memoization off it.

   p9-specific additions vs p7:
   - Single language; no toggle.
   - Selected namespace path (left tree pane); set of expanded prefixes.
   - Recovered surface AST in feedback (always present after first
     keystroke, since parse is total). *)

open! Core
open P9_typed_namespaces_substrate

module Hash_m = struct
  type t = Hash.t

  let sexp_of_t h = Sexp.Atom (Hash.to_string h)

  let t_of_sexp = function
    | Sexp.Atom s -> s
    | _ -> failwith "hash expects atom"

  let equal = Hash.equal
end

module Surface_m = struct
  type t = Surface_ast.t

  let sexp_of_t s = Sexp.Atom (Pretty.print_surface s)

  let t_of_sexp _ = failwith "surface_m not deserialisable"

  let equal = Surface_ast.equal
end

module Surface_ty_m = struct
  type t = Surface_ty.t

  let sexp_of_t s = Sexp.Atom (Surface_ty.print s)

  let t_of_sexp _ = failwith "surface_ty_m not deserialisable"

  let equal = Surface_ty.equal
end

type view = Detail of Hash_m.t | Author [@@deriving sexp, equal]

(* Editor mode: term editor (default) or type editor. Each mode keeps
   its own buffer, bind-as box, and feedback. The toggle lives at the
   top of the editor pane. *)
type editor_mode = Term_mode | Type_mode [@@deriving sexp, equal]

type scope_filter = Named_only | All_hashes [@@deriving sexp, equal]

type filter = {
  query : string;
  scope : scope_filter;
  type_filter : string option;
      (* printed type, e.g. "Int -> Int" — set by clicking the type chip in detail *)
}
[@@deriving sexp, equal]

let initial_filter : filter =
  { query = ""; scope = Named_only; type_filter = None }

type ingest_result =
  | Empty
  | Recovered of {
      surface : Surface_m.t;
      hole_count : int;
      ingest : ingest_outcome;
    }

and ingest_outcome =
  | Ingested of {
      hash : Hash_m.t;
      was_new : bool;
      type_summary : string;
      has_holes : bool;
      resolved : (string * Hash_m.t) list;
    }
  | Resolve_unbound of string
  | Resolve_ambiguous of { name : string; candidates : string list }
  | Resolve_missing_hash of string * Hash_m.t
  | Type_error of string
[@@deriving sexp, equal]

(* Parallel feedback shape for the type editor. The recovered Surface_ty
   replaces the term-side surface, and the ingest outcome is simpler —
   types have no typecheck or has-holes aspects. *)
type ty_ingest_result =
  | Ty_empty
  | Ty_recovered of {
      surface : Surface_ty_m.t;
      hole_count : int;
      ingest : ty_ingest_outcome;
    }

and ty_ingest_outcome =
  | Ty_ingested of {
      hash : Hash_m.t;
      was_new : bool;
      ty_summary : string;
      resolved : (string * Hash_m.t) list;
    }
  | Ty_resolve_unbound of string
  | Ty_resolve_ambiguous of { name : string; candidates : string list }
  | Ty_kind_mismatch of string
[@@deriving sexp, equal]

type t = {
  version : int;
  view : view;
  nav_back : view list;
  expanded_paths : string list;
  filter : filter;
  editor_mode : editor_mode;
  author_buffer : string;
  author_bind_as : string;
  author_ty_buffer : string;
  author_ty_bind_as : string;
  auto_eval : bool;
  feedback : ingest_result;
  ty_feedback : ty_ingest_result;
  pending_rebind : (string * Hash_m.t * Hash_m.t) option;
}
[@@deriving sexp, equal]

let initial : t =
  {
    version = 0;
    view = Author;
    nav_back = [];
    expanded_paths = [ "math"; "string"; "logic"; "vector"; "draft"; "alias" ];
    filter = initial_filter;
    editor_mode = Term_mode;
    author_buffer = "";
    author_bind_as = "";
    author_ty_buffer = "";
    author_ty_bind_as = "";
    auto_eval = false;
    feedback = Empty;
    ty_feedback = Ty_empty;
    pending_rebind = None;
  }

type action =
  | Set_view of view
  | Go_back
  | Toggle_path of string
  | Set_filter of filter
  | Set_editor_mode of editor_mode
  | Set_author_buffer of string
  | Set_bind_as of string
  | Set_author_ty_buffer of string
  | Set_ty_bind_as of string
  | Set_auto_eval of bool
  | Feedback_updated of ingest_result
  | Ty_feedback_updated of ty_ingest_result
  | Bind_current
  | Bind_current_ty
  | Bind_existing of { name : string; hash : Hash_m.t }
  | Unbind of string
  | Request_rebind of { name : string; new_hash : Hash_m.t }
  | Confirm_rebind
  | Cancel_rebind
  | Evaluate of Hash_m.t
[@@deriving sexp]
