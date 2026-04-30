(* Recovered-AST panel. Re-renders the post-recovery surface AST as the
   user types — this is the headline UX of p9: when the parser inserts
   holes to recover from malformed input, the user sees exactly where
   they landed. Holes are visually marked; the rest is the same surface
   syntax the user typed. *)

open! Core
open! Bonsai_web
open Bonsai.Let_syntax
open P9_typed_namespaces_substrate

(* Render a Surface_ast.t as a list of vdom nodes that mark Hole nodes
   distinctly. We follow the same precedence-aware printer as
   Pretty.print_surface — but interleave plain text and span(hole-marker)
   nodes so we can highlight holes inline. *)

type chunk = Text of string | Hole_chunk

let chunks_of_surface (s : Surface_ast.t) : chunk list =
  let buf = Buffer.create 64 in
  let acc = ref [] in
  let flush () =
    if Buffer.length buf > 0 then (
      acc := Text (Buffer.contents buf) :: !acc;
      Buffer.clear buf)
  in
  let push_text s =
    Buffer.add_string buf s
  in
  let push_hole () =
    flush ();
    acc := Hole_chunk :: !acc
  in
  let prec_of_op = function
    | Surface_ast.Or -> 0
    | And -> 1
    | Eq -> 2
    | Concat -> 3
    | Add | Sub -> 4
    | Mul | Div | Mod -> 5
    | Not -> 6
  in
  let rec walk ~prec s =
    let wrap this_prec body =
      if this_prec < prec then (
        push_text "(";
        body ();
        push_text ")")
      else body ()
    in
    match s with
    | Surface_ast.Var n -> push_text n
    | Int_lit n -> push_text (Int.to_string n)
    | Bool_lit true -> push_text "true"
    | Bool_lit false -> push_text "false"
    | String_lit s -> push_text ("\"" ^ String.escaped s ^ "\"")
    | Hole -> push_hole ()
    | Lam (x, ty, body) ->
        wrap 0 (fun () ->
            push_text ("\\" ^ x ^ ": " ^ Ty.print ty ^ ". ");
            walk ~prec:0 body)
    | Let (x, rhs, body) ->
        wrap 0 (fun () ->
            push_text ("let " ^ x ^ " = ");
            walk ~prec:0 rhs;
            push_text " in ";
            walk ~prec:0 body)
    | If (c, t, e) ->
        wrap 0 (fun () ->
            push_text "if ";
            walk ~prec:0 c;
            push_text " then ";
            walk ~prec:0 t;
            push_text " else ";
            walk ~prec:0 e)
    | Pair (a, b) ->
        push_text "(";
        walk ~prec:0 a;
        push_text ", ";
        walk ~prec:0 b;
        push_text ")"
    | Fst a ->
        wrap 7 (fun () ->
            push_text "fst ";
            walk ~prec:8 a)
    | Snd a ->
        wrap 7 (fun () ->
            push_text "snd ";
            walk ~prec:8 a)
    | App (f, a) ->
        wrap 7 (fun () ->
            walk ~prec:7 f;
            push_text " ";
            walk ~prec:8 a)
    | Prim (Not, [ e ]) ->
        wrap 6 (fun () ->
            push_text "not ";
            walk ~prec:7 e)
    | Prim (op, [ a; b ]) ->
        let op_prec = prec_of_op op in
        let s = Surface_ast.prim_op_to_string op in
        wrap op_prec (fun () ->
            walk ~prec:op_prec a;
            push_text (" " ^ s ^ " ");
            walk ~prec:(op_prec + 1) b)
    | Prim (op, args) ->
        wrap 7 (fun () ->
            push_text (Surface_ast.prim_op_to_string op);
            List.iter args ~f:(fun a ->
                push_text " ";
                walk ~prec:8 a))
  in
  walk ~prec:0 s;
  flush ();
  List.rev !acc

let render (surface : Surface_ast.t) : Vdom.Node.t =
  let chunks = chunks_of_surface surface in
  let nodes =
    List.map chunks ~f:(function
      | Text s -> Vdom.Node.text s
      | Hole_chunk ->
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "hole-marker" ]
            [ Vdom.Node.text "?" ])
  in
  Vdom.Node.div
    ~attrs:[ Vdom.Attr.class_ "recovered-ast" ]
    [
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "recovered-label" ]
        [ Vdom.Node.text "recovered AST" ];
      Vdom.Node.div ~attrs:[ Vdom.Attr.class_ "recovered-body" ] nodes;
    ]

let view
    ~(state : State.t Bonsai.Value.t)
    ~(inject : (State.action -> unit Vdom.Effect.t) Bonsai.Value.t) :
    Vdom.Node.t Bonsai.Computation.t =
  let _ = inject in
  let%arr state = state in
  match state.feedback with
  | State.Empty ->
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "recovered-ast empty" ]
        [
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "recovered-label" ]
            [ Vdom.Node.text "recovered AST" ];
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "recovered-empty-hint" ]
            [
              Vdom.Node.text
                "type something — every parse succeeds (errors become \
                 holes). The result of the recovery shows up here.";
            ];
        ]
  | State.Recovered { surface; hole_count; _ } ->
      let badge =
        if hole_count = 0 then Vdom.Node.none
        else
          Vdom.Node.span
            ~attrs:[ Vdom.Attr.class_ "hole-count" ]
            [
              Vdom.Node.text
                (Printf.sprintf " (%d hole%s)"
                   hole_count
                   (if hole_count = 1 then "" else "s"));
            ]
      in
      Vdom.Node.div
        ~attrs:[ Vdom.Attr.class_ "recovered-ast" ]
        [
          Vdom.Node.div
            ~attrs:[ Vdom.Attr.class_ "recovered-label" ]
            [ Vdom.Node.text "recovered AST"; badge ];
          render surface;
        ]
