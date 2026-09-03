(* The substrate, as a module-level mutable singleton.
   Bonsai's model holds only UI state; a version counter forces re-render after
   any mutation. Same shape as p7/p9/p15-p17. *)
open! Core
open P21_substrate

type t =
  { store : Store.t
  ; ns : Names.t
  ; watch : Watch.t
  }

let create () = { store = Store.create (); ns = Names.create (); watch = Watch.create () }
let global : t = create ()

(* The saved scratch state, held beside the substrate rather than recomputed in
   the view — because saving is an EVENT (a keystroke), not a render. It mutates
   the watch cache, so it must happen exactly once per edit. *)
let scratch : Scratch.t ref = ref Scratch.empty

let save (text : string) : unit =
  scratch := Scratch.save ~st:global.store ~ns:global.ns ~w:global.watch text

(* The bootstrap buffer: the paper's own worked examples, ready to commit.
   Everything below the fold is inert scrollback, which is what the fold is. *)
let seed_buffer =
  String.concat
    ~sep:"\n"
    [ "-- The scratch buffer. Several definitions, their signatures, and watch"
    ; "-- expressions. Nothing is stored until the whole buffer typechecks:"
    ; "-- identity is conferred by typechecking (def:uingest)."
    ; ""
    ; "inc : Nat -> Nat"
    ; "inc = \\n. Nat.+ n 1"
    ; ""
    ; "scale : Nat -> Nat"
    ; "scale = \\n. Nat.* n 2"
    ; ""
    ; "double : Nat -> Nat"
    ; "double = \\n. scale n"
    ; ""
    ; "-- A watch expression needs no name and enters no namespace, but it is"
    ; "-- hashed and cached on exactly the same footing (rem:watchkey)."
    ; "> Nat.+ 2 3"
    ; ""
    ; "---"
    ; "Everything below the `---` fold is inert. Unison's working-set boundary"
    ; "is a string in a text file, and so is this one."
    ; ""
    ; "Try, in order:"
    ; "  1. Commit. inc, scale and double are hashed and bound."
    ; "  2. Replace the buffer with a new scale of the SAME type:"
    ; "         scale : Nat -> Nat"
    ; "         scale = \\n. Nat.* n 3"
    ; "     Commit. double is rebuilt -- its source text is unchanged and its"
    ; "     hash changes anyway, because the printed NAME resolved against the"
    ; "     new namespace (def:update)."
    ; "  3. Now replace it with one of a DIFFERENT type:"
    ; "         scale : Text -> Nat"
    ; "         scale = \\s. Text.size s"
    ; "     Commit. It fails -- and both scale and double leave the namespace,"
    ; "     with their source handed back into this buffer (rem:step5)."
    ; "  4. Paste in two definitions that call each other and are otherwise"
    ; "     identical, and watch ingest refuse the ambiguous ordering"
    ; "     (cor:detect):"
    ; "         ping : Nat -> Nat"
    ; "         ping = \\n. pong n"
    ; "         pong : Nat -> Nat"
    ; "         pong = \\n. ping n"
    ]
