(* Seed the Store + Namespace with example bindings that exercise the
   prototype's headline features: dot-paths, suffix resolution
   ambiguity, holes, has-holes badge, named-type aliases, and
   primitive functions registered via the Primitive_registry. *)

open P10_minted_labels_substrate

let ingest ~store ~att ~ns src =
  let surface = Parse_recover.parse src in
  match Resolver.ingest ~namespace:ns ~store ~att surface with
  | Ok r -> Some r.hash
  | Error _ -> None

let ingest_ty ~store ~ns src =
  let surface_ty = Parse_recover.parse_ty src in
  match Resolver.ingest_ty ~namespace:ns ~store surface_ty with
  | Ok r -> Some r.hash
  | Error _ -> None

let seed ~store ~att ~ns =
  (* Every bootstrap binding is minted: wrap the substructure body
     hash with a fresh Named_term / Named_type and bind to that. *)
  let bind_term name h =
    let bound = Store.register_named_term store h in
    try Namespace.bind ns ~name bound with _ -> ()
  in
  let bind_type name h =
    let bound = Store.register_named_type store h in
    try Namespace.bind ns ~name bound with _ -> ()
  in
  let opt_bind_term name = function
    | Some h -> bind_term name h
    | None -> ()
  in
  let opt_bind_ty name = function
    | Some h -> bind_type name h
    | None -> ()
  in
  let opt_bind = opt_bind_term in
  (* Built-in primitives registered + name-bound through the substrate.
     install does its own mint-wrapping inside primitives.re. *)
  Primitives.install ~store ~att ~ns;
  (* alias.* — bind before any term that uses them as type annotations.
     IntEndo collides on the annotation in math.apply below, demonstrating
     that named-alias and structural form produce identical hashes. *)
  opt_bind_ty "alias.Predicate" (ingest_ty ~store ~ns "Int -> Bool");
  opt_bind_ty "alias.StringOp"  (ingest_ty ~store ~ns "String -> String");
  opt_bind_ty "alias.Compare"   (ingest_ty ~store ~ns "Int -> Int -> Bool");
  opt_bind_ty "alias.IntPair"   (ingest_ty ~store ~ns "Int * Int");
  opt_bind_ty "alias.IntEndo"   (ingest_ty ~store ~ns "Int -> Int");
  opt_bind_ty "alias.BinOp"     (ingest_ty ~store ~ns "Int -> Int -> Int");
  (* int.* surface terms — comparisons use int.lt / int.min / int.max primitives *)
  opt_bind "int.signum"
    (ingest ~store ~att ~ns
       "\\x: Int => if int.lt x 0 then (0 - 1) else if x == 0 then 0 else 1");
  opt_bind "int.clamp"
    (ingest ~store ~att ~ns
       "\\lo: Int => \\hi: Int => \\x: Int => int.max lo (int.min hi x)");
  (* bool.* surface terms *)
  opt_bind "bool.to_int"
    (ingest ~store ~att ~ns "\\b: Bool => if b then 1 else 0");
  opt_bind "bool.of_int"
    (ingest ~store ~att ~ns "\\n: Int => not (n == 0)");
  (* string.* surface terms — is_empty must precede non_empty *)
  opt_bind "string.is_empty"
    (ingest ~store ~att ~ns "\\s: String => s == \"\"");
  opt_bind "string.non_empty"
    (ingest ~store ~att ~ns "\\s: String => not (string.is_empty s)");
  opt_bind "string.greet"
    (ingest ~store ~att ~ns "\\name: String => \"hello, \" ++ name");
  opt_bind "string.greet_loud"
    (ingest ~store ~att ~ns "\\name: String => \"HELLO, \" ++ string.to_upper name");
  (* math.* — full-path lookups, existing + new *)
  opt_bind "math.add"
    (ingest ~store ~att ~ns "\\x: Int => \\y: Int => x + y");
  opt_bind "math.sub"
    (ingest ~store ~att ~ns "\\x: Int => \\y: Int => x - y");
  opt_bind "math.mul"
    (ingest ~store ~att ~ns "\\x: Int => \\y: Int => x mul y");
  opt_bind "math.inc"    (ingest ~store ~att ~ns "\\x: Int => x + 1");
  opt_bind "math.dec"    (ingest ~store ~att ~ns "\\x: Int => x - 1");
  opt_bind "math.square" (ingest ~store ~att ~ns "\\x: Int => x mul x");
  (* math.double: x + x. Structurally != x mul 2; same behavior, different hash.
     Demonstrates that content addressing is structural, not semantic. *)
  opt_bind "math.double" (ingest ~store ~att ~ns "\\x: Int => x + x");
  (* math.apply uses IntEndo by name; structurally identical to
     `\f: Int -> Int. \x: Int. f x`. Demonstrates aliasing identity. *)
  opt_bind "math.apply"
    (ingest ~store ~att ~ns "\\f: IntEndo => \\x: Int => f x");
  (* math.compose: higher-order, uses IntEndo alias in two argument positions *)
  opt_bind "math.compose"
    (ingest ~store ~att ~ns "\\f: IntEndo => \\g: IntEndo => \\x: Int => f (g x)");
  (* logic.* — full boolean algebra set *)
  opt_bind "logic.implies"
    (ingest ~store ~att ~ns "\\p: Bool => \\q: Bool => not p || q");
  opt_bind "logic.xor"
    (ingest ~store ~att ~ns "\\p: Bool => \\q: Bool => (p || q) && not (p && q)");
  opt_bind "logic.iff"
    (ingest ~store ~att ~ns "\\p: Bool => \\q: Bool => (p && q) || (not p && not q)");
  opt_bind "logic.nand"
    (ingest ~store ~att ~ns "\\p: Bool => \\q: Bool => not (p && q)");
  opt_bind "logic.nor"
    (ingest ~store ~att ~ns "\\p: Bool => \\q: Bool => not (p || q)");
  (* pair.* — new category, monomorphic Int * Int.
     min/max suffix is intentionally ambiguous with int.min/int.max. *)
  opt_bind "pair.swap"
    (ingest ~store ~att ~ns "\\p: Int * Int => (snd p, fst p)");
  opt_bind "pair.sum"
    (ingest ~store ~att ~ns "\\p: Int * Int => fst p + snd p");
  opt_bind "pair.min"
    (ingest ~store ~att ~ns "\\p: Int * Int => int.min (fst p) (snd p)");
  opt_bind "pair.max"
    (ingest ~store ~att ~ns "\\p: Int * Int => int.max (fst p) (snd p)");
  (* vector.add — collides on suffix `add` to demo Ambiguous_name *)
  opt_bind "vector.add"
    (ingest ~store ~att ~ns "\\x: Int => \\y: Int => x + y mul 2");
  (* Geom.* — record types demonstrating intentional label sharing.
     Geom.Point declares labels x, y for the first time (mints them).
     Geom.Vector reuses the same x, y because the resolver finds the
     existing namespace bindings — label hashes are shared, and
     functions like translate work on either with the same label
     references in the body. *)
  opt_bind_ty "Geom.Point"  (ingest_ty ~store ~ns "{ x : Int, y : Int }");
  opt_bind_ty "Geom.Vector" (ingest_ty ~store ~ns "{ x : Int, y : Int }");
  opt_bind "Geom.origin"
    (ingest ~store ~att ~ns "{ x = 0, y = 0 }");
  opt_bind "Geom.translate"
    (ingest ~store ~att ~ns
       "\\dx: Int => \\dy: Int => \\p: Geom.Point => { p with x = p.x + dx, y = p.y + dy }");
  opt_bind "Geom.magnitude_sq"
    (ingest ~store ~att ~ns
       "\\p: Geom.Point => (p.x mul p.x) + (p.y mul p.y)");
  (* Record types with different field shapes — labels here are
     distinct from x/y. *)
  opt_bind_ty "Music.Note"
    (ingest_ty ~store ~ns "{ pitch : Int, duration : Int }");
  opt_bind "Music.middle_c"
    (ingest ~store ~att ~ns "{ pitch = 60, duration = 4 }");
  (* List.* — wrappers over the list.* primitive registry entries.
     Demonstrates list literals + list primitives end-to-end. *)
  opt_bind "List.factorial"
    (ingest ~store ~att ~ns
       "\\n: Int => list.product (list.tail_int (list.range (n + 1)))");
  opt_bind "List.sum_to"
    (ingest ~store ~att ~ns "\\n: Int => list.sum (list.range (n + 1))");
  opt_bind "List.first_or_zero"
    (ingest ~store ~att ~ns "\\xs: List Int => list.head_int xs");
  (* Tuple example — using Geom.Point projection. *)
  opt_bind "pair.from_point"
    (ingest ~store ~att ~ns "\\p: Geom.Point => (p.x, p.y)");
  (* draft.* stubs — intentionally holey, all show the has-holes badge *)
  opt_bind "draft.todo"        (ingest ~store ~att ~ns "\\x: Int => ?");
  opt_bind "draft.fixed_point" (ingest ~store ~att ~ns "\\f: IntEndo => ?");
  opt_bind "draft.iterate"     (ingest ~store ~att ~ns "\\n: Int => \\f: IntEndo => ?")
