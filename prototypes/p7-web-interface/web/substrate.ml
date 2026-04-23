(* Module-level singleton — the one mutable substrate the entire app shares.
   Bonsai holds a [version : int] counter that bumps on every mutation so
   views re-render; the actual Store/Attachment/Namespace live here, not in
   Bonsai state. See decisions.md §State pattern. *)

open P7_web_interface_substrate

type t = {
  store : Store.t;
  att : Attachment.t;
  ns : Namespace.t;
  mutable step_limit : int;
}

let create () =
  let store = Store.create () in
  let att = Attachment.create () in
  Attachment.register_descriptor att Lc_eval.descriptor;
  Attachment.register_descriptor att Stlc_eval.descriptor;
  Attachment.register_descriptor att Stlc_typecheck.descriptor;
  Attachment.register_descriptor att Stlc_to_lc_erase_church.descriptor;
  Attachment.register_descriptor att Lc_to_stlc_check.descriptor;
  let ns = Namespace.create () in
  Bootstrap.seed ~store ~att ~ns;
  { store; att; ns; step_limit = 10000 }

let global : t = create ()
