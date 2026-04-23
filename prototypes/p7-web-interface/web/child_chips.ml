(* Renders a definition's body as Vdom.Node.t with clickable hash chips
   for namespace-bound subterms. Mirrors Pretty's surface_of_hash + print
   precedence rules but emits spans decorated with click handlers.

   A Var(n) in the surface AST is either:
     - a bound binder (n is in in_scope) → plain text
     - a namespace name, produced because surface_of_hash_ctx collapsed a
       closed bound subterm → clickable chip linking to the name's hash
     - a stray `$k` or `<missing …>` indicator → plain text
*)

open! Core
open! Bonsai_web
open P7_web_interface_substrate

let txt s = Vdom.Node.text s

let span s = Vdom.Node.span [ txt s ]

let chip_for_name
    ~(ns : Namespace.t)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(in_scope : string list)
    (n : string) : Vdom.Node.t =
  if List.mem in_scope n ~equal:String.equal then span n
  else
    match Namespace.resolve ns n with
    | None -> span n
    | Some h ->
        Vdom.Node.span
          ~attrs:
            [
              Vdom.Attr.classes [ "chip-name-inline"; "clickable" ];
              Vdom.Attr.on_click (fun _ -> inject (State.Set_view (Detail h)));
              Vdom.Attr.title (Hash.short h);
            ]
          [ txt n ]

(* ==================== Lc ==================== *)

let rec lc_render
    ~(ns : Namespace.t)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(in_scope : string list)
    (s : Lc_surface_ast.t) : Vdom.Node.t list =
  match s with
  | Var n -> [ chip_for_name ~ns ~inject ~in_scope n ]
  | Lam (x, body) ->
      txt "\\" :: span x :: txt ". "
      :: lc_render ~ns ~inject ~in_scope:(x :: in_scope) body
  | App (f, a) ->
      lc_render_app_left ~ns ~inject ~in_scope f
      @ [ txt " " ]
      @ lc_render_atom ~ns ~inject ~in_scope a

and lc_render_app_left ~ns ~inject ~in_scope (s : Lc_surface_ast.t) :
    Vdom.Node.t list =
  match s with
  | Var _ -> lc_render ~ns ~inject ~in_scope s
  | App _ -> lc_render ~ns ~inject ~in_scope s
  | Lam _ -> [ txt "(" ] @ lc_render ~ns ~inject ~in_scope s @ [ txt ")" ]

and lc_render_atom ~ns ~inject ~in_scope (s : Lc_surface_ast.t) :
    Vdom.Node.t list =
  match s with
  | Var _ -> lc_render ~ns ~inject ~in_scope s
  | _ -> [ txt "(" ] @ lc_render ~ns ~inject ~in_scope s @ [ txt ")" ]

(* ==================== Stlc ==================== *)

let rec stlc_render
    ~(ns : Namespace.t)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(in_scope : string list)
    (s : Stlc_surface_ast.t) : Vdom.Node.t list =
  match s with
  | Var n -> [ chip_for_name ~ns ~inject ~in_scope n ]
  | True -> [ span "true" ]
  | False -> [ span "false" ]
  | Lam (x, ty, body) ->
      txt "\\" :: span x :: txt ":" :: span (Ty.print ty) :: txt ". "
      :: stlc_render ~ns ~inject ~in_scope:(x :: in_scope) body
  | App (f, a) ->
      stlc_render_app_left ~ns ~inject ~in_scope f
      @ [ txt " " ]
      @ stlc_render_atom ~ns ~inject ~in_scope a
  | If (c, t, e) ->
      (txt "if "
       :: stlc_render_atom ~ns ~inject ~in_scope c)
      @ [ txt " then " ]
      @ stlc_render_atom ~ns ~inject ~in_scope t
      @ [ txt " else " ]
      @ stlc_render_atom ~ns ~inject ~in_scope e

and stlc_render_app_left ~ns ~inject ~in_scope (s : Stlc_surface_ast.t) :
    Vdom.Node.t list =
  match s with
  | Var _ | True | False -> stlc_render ~ns ~inject ~in_scope s
  | App _ -> stlc_render ~ns ~inject ~in_scope s
  | Lam _ | If _ ->
      [ txt "(" ] @ stlc_render ~ns ~inject ~in_scope s @ [ txt ")" ]

and stlc_render_atom ~ns ~inject ~in_scope (s : Stlc_surface_ast.t) :
    Vdom.Node.t list =
  match s with
  | Var _ | True | False -> stlc_render ~ns ~inject ~in_scope s
  | _ -> [ txt "(" ] @ stlc_render ~ns ~inject ~in_scope s @ [ txt ")" ]

(* ==================== Entry point ==================== *)

let view
    ~(ns : Namespace.t)
    ~(store : Store.t)
    ~(inject : State.action -> unit Vdom.Effect.t)
    ~(lang : [ `Lc | `Stlc ])
    (h : Hash.t) : Vdom.Node.t =
  let children =
    try
      match lang with
      | `Lc ->
          let s = Lc_pretty.surface_of_hash ~namespace:ns store h in
          lc_render ~ns ~inject ~in_scope:[] s
      | `Stlc ->
          let s = Stlc_pretty.surface_of_hash ~namespace:ns store h in
          stlc_render ~ns ~inject ~in_scope:[] s
    with _ -> [ txt "(cannot render)" ]
  in
  Vdom.Node.pre ~attrs:[ Vdom.Attr.class_ "mono detail-body-text" ] children
