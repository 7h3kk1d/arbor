(* Audio playback for terms typed as Music.types.Note or
   Music.types.Song. Hooks into the browser's Web Audio API via
   js_of_ocaml unsafe-calls; no OCaml-level wrapper library required.

   "Plays" a Note as a single sine-wave tone at the MIDI pitch,
   sustained for `duration` units (one unit = 0.1s here, so a quarter
   note at duration=4 lasts 0.4s). A Song plays its notes
   sequentially with a tiny gap so consecutive same-pitch notes are
   audibly separated.

   The type discrimination is structural: compare the cached
   `Type_of` aspect's underlying Ty.t hash to the substructure body
   hashes of `Music.types.Note` and `Music.types.Song`. Pure
   substrate operations — the player has no knowledge of language
   internals beyond labels and Int literals. *)

open! Core
open P11_mint_threads_substrate
open Js_of_ocaml

type kind = Note | Song

let note_type_hash ~(store : Store.t) ~(ns : Namespace.t) : Hash.t option =
  match Namespace.resolve_query ns "Music.types.Note" with
  | Ok wrapper -> Some (Store.unwrap_named store wrapper)
  | Error _ -> None

let song_type_hash ~(store : Store.t) ~(ns : Namespace.t) : Hash.t option =
  match Namespace.resolve_query ns "Music.types.Song" with
  | Ok wrapper -> Some (Store.unwrap_named store wrapper)
  | Error _ -> None

(* Get the cached type hash for a term, if any. The Typecheck aspect
   stores the type as a Hash.t pointing at a Definition.Type — that's
   what we compare against. *)
let cached_type_hash ~(store : Store.t) ~(att : Attachment.t) (h : Hash.t)
    : Hash.t option =
  let target = Store.unwrap_named store h in
  match
    Attachment.peek att ~target ~aspect:Typecheck.aspect_id
      ~procedure:Typecheck.procedure_id
  with
  | Some (Attachment.Type_of ty_h) -> Some ty_h
  | Some (Attachment.Type_with_holes ty_h) -> Some ty_h
  | _ -> None

let kind_of_hash ~(store : Store.t) ~(ns : Namespace.t) ~(att : Attachment.t)
    (h : Hash.t) : kind option =
  match cached_type_hash ~store ~att h with
  | None -> None
  | Some ty_h ->
      let note_h = note_type_hash ~store ~ns in
      let song_h = song_type_hash ~store ~ns in
      if Option.equal String.equal (Some ty_h) note_h then Some Note
      else if Option.equal String.equal (Some ty_h) song_h then Some Song
      else None

(* ===== Note extraction ===== *)

let pitch_label_hash ~(ns : Namespace.t) : Hash.t option =
  match Namespace.resolve_query ns "pitch" with
  | Ok h -> Some h
  | Error _ -> None

let duration_label_hash ~(ns : Namespace.t) : Hash.t option =
  match Namespace.resolve_query ns "duration" with
  | Ok h -> Some h
  | Error _ -> None

let int_of_hash ~(store : Store.t) (h : Hash.t) : int option =
  match Store.lookup_term store h with
  | Some (Node.Int_lit n) -> Some n
  | _ -> None

let extract_note ~(store : Store.t) ~pitch_h ~duration_h (value_h : Hash.t)
    : (int * int) option =
  let value_h = Store.unwrap_named store value_h in
  match Store.lookup_term store value_h with
  | Some (Node.Record_lit fields) ->
      let lookup target_lh =
        List.find_map fields ~f:(fun (lh, vh) ->
            if String.equal lh target_lh then int_of_hash ~store vh else None)
      in
      (match lookup pitch_h, lookup duration_h with
       | Some p, Some d -> Some (p, d)
       | _ -> None)
  | _ -> None

let extract_song ~(store : Store.t) ~pitch_h ~duration_h (value_h : Hash.t)
    : (int * int) list option =
  let value_h = Store.unwrap_named store value_h in
  match Store.lookup_term store value_h with
  | Some (Node.List_lit items) ->
      let rec walk = function
        | [] -> Some []
        | h :: rest ->
            (match extract_note ~store ~pitch_h ~duration_h h with
             | None -> None
             | Some n ->
                 (match walk rest with
                  | None -> None
                  | Some ns -> Some (n :: ns)))
      in
      walk items
  | _ -> None

(* ===== Web Audio playback ===== *)

let audio_context : Js.Unsafe.any option ref = ref None

let get_ctx () : Js.Unsafe.any =
  match !audio_context with
  | Some c -> c
  | None ->
      let ctor = Js.Unsafe.global##._AudioContext in
      let c = Js.Unsafe.new_obj ctor [||] in
      audio_context := Some c;
      c

let freq_of_midi (midi : int) : float =
  let m = Float.of_int midi in
  440.0 *. Float.( ** ) 2.0 ((m -. 69.0) /. 12.0)

(* Duration unit → seconds. The bootstrap uses `duration = 4` for a
   quarter note, so 0.1s per unit gives 0.4s quarters — a brisk but
   audible tempo. *)
let dur_seconds (d : int) : float = Float.of_int d *. 0.1

let schedule_tone ~ctx ~start_offset ~freq ~duration =
  let now = Js.Unsafe.get ctx "currentTime" |> Js.float_of_number in
  let t0 = now +. start_offset in
  let t1 = t0 +. duration in
  let osc = Js.Unsafe.meth_call ctx "createOscillator" [||] in
  let gain = Js.Unsafe.meth_call ctx "createGain" [||] in
  let dest = Js.Unsafe.get ctx "destination" in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call osc "connect" [| Js.Unsafe.inject gain |]
  in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call gain "connect" [| Js.Unsafe.inject dest |]
  in
  Js.Unsafe.set osc "type" (Js.string "sine");
  let freq_param = Js.Unsafe.get osc "frequency" in
  Js.Unsafe.set freq_param "value" (Js.number_of_float freq);
  (* Short attack and release ramps to avoid pops at note edges. *)
  let gain_param = Js.Unsafe.get gain "gain" in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call gain_param "setValueAtTime"
      [|
        Js.Unsafe.inject (Js.number_of_float 0.0);
        Js.Unsafe.inject (Js.number_of_float t0);
      |]
  in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call gain_param "linearRampToValueAtTime"
      [|
        Js.Unsafe.inject (Js.number_of_float 0.2);
        Js.Unsafe.inject (Js.number_of_float (t0 +. 0.02));
      |]
  in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call gain_param "linearRampToValueAtTime"
      [|
        Js.Unsafe.inject (Js.number_of_float 0.2);
        Js.Unsafe.inject
          (Js.number_of_float (Float.max (t1 -. 0.05) (t0 +. 0.05)));
      |]
  in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call gain_param "linearRampToValueAtTime"
      [|
        Js.Unsafe.inject (Js.number_of_float 0.0);
        Js.Unsafe.inject (Js.number_of_float t1);
      |]
  in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call osc "start"
      [| Js.Unsafe.inject (Js.number_of_float t0) |]
  in
  let _ : Js.Unsafe.any =
    Js.Unsafe.meth_call osc "stop"
      [| Js.Unsafe.inject (Js.number_of_float t1) |]
  in
  ()

let play_one ~ctx ~start_offset (midi, duration) : float =
  if midi <= 0 then
    (* Treat rest / non-positive pitch as silence. *)
    start_offset +. dur_seconds duration
  else begin
    let sec = dur_seconds duration in
    schedule_tone ~ctx ~start_offset ~freq:(freq_of_midi midi) ~duration:sec;
    start_offset +. sec
  end

let play_notes (notes : (int * int) list) : unit =
  let ctx = get_ctx () in
  (* Some browsers suspend the AudioContext until a user gesture has
     resumed it. The button click is a gesture, but the ctor may have
     run earlier — resume defensively. *)
  let state = Js.Unsafe.get ctx "state" |> Js.to_string in
  if String.equal state "suspended" then
    ignore (Js.Unsafe.meth_call ctx "resume" [||] : Js.Unsafe.any);
  let _ : float =
    List.fold notes ~init:0.0 ~f:(fun off note ->
        play_one ~ctx ~start_offset:off note)
  in
  ()

(* ===== Top-level: evaluate then play ===== *)

let eval_to_value ~(store : Store.t) ~(att : Attachment.t)
    (h : Hash.t) : Hash.t option =
  match Eval.peek_cache ~store att h with
  | Some (Eval.Value v) -> Some v
  | _ ->
      (match Eval.eval ~store ~att ~step_limit:10000 h with
       | Eval.Value v -> Some v
       | _ -> None)

let play ~(store : Store.t) ~(ns : Namespace.t) ~(att : Attachment.t)
    (h : Hash.t) : unit =
  match kind_of_hash ~store ~ns ~att h with
  | None -> ()
  | Some kind ->
      (match pitch_label_hash ~ns, duration_label_hash ~ns with
       | None, _ | _, None -> ()
       | Some pitch_h, Some duration_h ->
           (match eval_to_value ~store ~att h with
            | None -> ()
            | Some value_h ->
                let notes =
                  match kind with
                  | Note ->
                      (match
                         extract_note ~store ~pitch_h:pitch_h
                           ~duration_h value_h
                       with
                       | Some n -> [ n ]
                       | None -> [])
                  | Song ->
                      (match
                         extract_song ~store ~pitch_h ~duration_h value_h
                       with
                       | Some ns -> ns
                       | None -> [])
                in
                play_notes notes))
