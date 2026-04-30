(* Module-level singleton — the one mutable substrate the entire app
   shares. Bonsai holds a [version] counter that bumps on every
   mutation so views re-render; the actual Store/Attachment/Namespace
   live here. *)

open P9_typed_namespaces_substrate

type t = {
  store : Store.t;
  att : Attachment.t;
  ns : Namespace.t;
  mutable step_limit : int;
}

let create () =
  let store = Store.create () in
  let att = Attachment.create () in
  Attachment.register_descriptor att Typecheck.descriptor;
  Attachment.register_descriptor att Has_holes.descriptor;
  Attachment.register_descriptor att Eval.descriptor;
  let ns = Namespace.create () in
  Bootstrap.seed ~store ~att ~ns;
  { store; att; ns; step_limit = 10000 }

let global : t = create ()
