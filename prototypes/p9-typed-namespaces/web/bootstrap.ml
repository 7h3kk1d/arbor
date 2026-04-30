(* Seed the Store + Namespace with example bindings that exercise the
   prototype's headline features: dot-paths, suffix resolution
   ambiguity, holes, and the has-holes badge. *)

open P9_typed_namespaces_substrate

let ingest ~store ~att ~ns src =
  let surface = Parse_recover.parse src in
  match Resolver.ingest ~namespace:ns ~store ~att surface with
  | Ok r -> Some r.hash
  | Error _ -> None

let seed ~store ~att ~ns =
  let bind name h = try Namespace.bind ns ~name h with _ -> () in
  let opt_bind name = function Some h -> bind name h | None -> () in
  (* math.* — exercise full-path lookups *)
  opt_bind "math.add"
    (ingest ~store ~att ~ns "\\x: Int. \\y: Int. x + y");
  opt_bind "math.sub"
    (ingest ~store ~att ~ns "\\x: Int. \\y: Int. x - y");
  opt_bind "math.mul"
    (ingest ~store ~att ~ns "\\x: Int. \\y: Int. x mul y");
  opt_bind "math.inc" (ingest ~store ~att ~ns "\\x: Int. x + 1");
  (* vector.add — collides on suffix `add` to demo Ambiguous_name *)
  opt_bind "vector.add"
    (ingest ~store ~att ~ns "\\x: Int. \\y: Int. x + y mul 2");
  (* string utils *)
  opt_bind "string.greet"
    (ingest ~store ~att ~ns "\\name: String. \"hello, \" ++ name");
  (* boolean utils *)
  opt_bind "logic.implies"
    (ingest ~store ~att ~ns "\\p: Bool. \\q: Bool. not p || q");
  (* an intentionally holey definition to demo the has-holes badge *)
  opt_bind "draft.todo" (ingest ~store ~att ~ns "\\x: Int. ?")
