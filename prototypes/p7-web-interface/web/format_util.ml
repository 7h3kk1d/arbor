(* Rendering helpers shared across views. *)

open! Core
open P7_web_interface_substrate

(* 8-char display hash with a leading '#'. Matches Hash.short default. *)
let short_hash (h : Hash.t) : string = Hash.short h

(* Full hash with leading '#'. *)
let long_hash (h : Hash.t) : string = Hash.to_string h

(* "[lc]" / "[stlc]" — fixed-width 7 char string incl. trailing spaces,
   mirrors bin/main.re's language tag padding. *)
let language_tag_of_hash ~(store : Store.t) (h : Hash.t) : string =
  Pretty.language_tag_of_hash store h
