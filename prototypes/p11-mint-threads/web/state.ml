(* Bonsai-visible model + action sum. Every action that mutates
   Substrate.global bumps [version]; views key their memoization off it.

   p9-specific additions vs p7:
   - Single language; no toggle.
   - Selected namespace path (left tree pane); set of expanded prefixes.
   - Recovered surface AST in feedback (always present after first
     keystroke, since parse is total). *)

open! Core
open P11_mint_threads_substrate

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

(* p11: which update strategy the editor's bind action will use when
   committing an edit-of-X. Pin is the default and matches the pre-p11
   behavior (orphan the old hash; no cascade). *)
type strategy_kind = SK_pin | SK_follow | SK_migrate
[@@deriving sexp, equal]

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
  (* Input text for the detail-pane "bind another name" affordance.
     One field is enough — the detail pane is single-hash, so the
     same buffer serves whichever entry is open. Cleared on bind. *)
  detail_bind_buffer : string;
  (* p11: when Some(old_named_hash), the next term-bind goes through
     Update_strategy.apply_edit with the chosen strategy, preserving the
     mint mark. Set by Start_edit_of; cleared after a successful
     apply_edit. *)
  edit_of : Hash_m.t option;
  edit_strategy : strategy_kind;
  (* When edit_strategy = SK_migrate, the set of caller named-hashes
     selected by the user in the migrate dialog. Stored as a sorted
     list for stable equality. *)
  migrate_sites : Hash_m.t list;
  (* When non-empty, render the migrate dialog. The list is the set of
     caller named-hashes reachable from the current edit-of's body. *)
  migrate_candidates : Hash_m.t list;
  (* Last-attempted cascade error, if any. Surfaced near the bind
     button so the user knows why Follow aborted. *)
  last_strategy_error : string option;
}
[@@deriving sexp, equal]

let initial : t =
  {
    version = 0;
    view = Author;
    nav_back = [];
    expanded_paths =
      [ "math"; "string"; "logic"; "vector"; "draft"; "alias"; "Music" ];
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
    detail_bind_buffer = "";
    edit_of = None;
    edit_strategy = SK_pin;
    migrate_sites = [];
    migrate_candidates = [];
    last_strategy_error = None;
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
  (* Load a hash's pretty-printed source into the editor textarea.
     Switches editor_mode to match the hash's sort (Term_mode for
     terms / Named_term, Type_mode for types / Named_type), populates
     the matching buffer, and refreshes feedback. Labels can't be
     opened (no body to edit). *)
  | Open_in_editor of Hash_m.t
  | Set_detail_bind_buffer of string
  (* Bind a fresh name to an existing hash from the detail pane —
     used to give a label, term, or type an additional or new name.
     If the name is already bound elsewhere, the same rebind dialog
     as the editor's bind action opens for confirmation. *)
  | Detail_bind of Hash_m.t
  (* Switch the detail pane to view another hash without changing
     the editor state. Used by the p11 mint-thread / callers / history
     sections to navigate between related definitions. *)
  | Open_detail of Hash_m.t
  (* p11: begin an edit-of-X session. Loads the source into the term
     buffer (like Open_in_editor for terms), sets the bind-as box to
     the current name (so the user sees what they're editing), and
     records the old named-hash so the next Bind_current goes through
     Update_strategy.apply_edit. *)
  | Start_edit_of of Hash_m.t
  | Cancel_edit_of
  | Set_edit_strategy of strategy_kind
  | Toggle_migrate_site of Hash_m.t
[@@deriving sexp]
