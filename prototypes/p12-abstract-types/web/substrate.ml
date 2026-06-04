(* Module-level singleton: the one Store + Namespace the whole app shares, plus
   the mint source for new abstract types. The editing context's open set is UI
   state and lives in the Bonsai model, not here. Bootstrap seeds the Counter
   worked example so the app is demonstrable on load. *)

open P12_substrate

type t = {
  store : Store.t;
  ns : Namespace.t;
  mint_src : Mint.source;
}

(* Build the design/12 Counter: an opaque type over Int, the sealed ops authored
   in an opening context, and two ordinary consumers in the default context. *)
let seed ~store ~ns ~mint_src =
  let int_h = Store.int_type store in
  let arrow a b = Store.ingest_type store (Tnode.Arrow (a, b)) in
  let m = Mint.fresh mint_src in
  let counter = Store.ingest_type store (Tnode.Opaque { mint = m; witness = int_h }) in
  (try Namespace.rebind ns ~name:"Counter.t" counter with _ -> ());
  let ctx = Editing_context.make () in
  Editing_context.open_type ctx counter;
  let bind_sealed name term ann =
    match Editing_context.commit store ctx ~term ~ann with
    | Ok (h, _) -> (try Namespace.rebind ns ~name h with _ -> ())
    | Error _ -> ()
  in
  bind_sealed "Counter.empty" (Node.Lit 0) counter;
  bind_sealed "Counter.incr"
    (Node.Lam (counter, Node.Prim (Node.Add, [ Node.Var 0; Node.Lit 1 ])))
    (arrow counter counter);
  bind_sealed "Counter.get" (Node.Lam (counter, Node.Var 0)) (arrow counter int_h);
  bind_sealed "Counter.decr"
    (Node.Lam (counter, Node.Prim (Node.Sub, [ Node.Var 0; Node.Lit 1 ])))
    (arrow counter counter);
  (* consumers, authored with nothing open *)
  let ctx0 = Editing_context.make () in
  let bind_normal name term ann =
    match Editing_context.commit store ctx0 ~term ~ann with
    | Ok (h, _) -> (try Namespace.rebind ns ~name h with _ -> ())
    | Error _ -> ()
  in
  (match Namespace.resolve ns "Counter.incr", Namespace.resolve ns "Counter.get" with
   | Some incr, Some get ->
       bind_normal "bump2"
         (Node.Lam (counter, Node.App (Node.Ref incr, Node.App (Node.Ref incr, Node.Var 0))))
         (arrow counter counter);
       bind_normal "readout"
         (Node.Lam (counter, Node.App (Node.Ref get, Node.App (Node.Ref incr, Node.Var 0))))
         (arrow counter int_h)
   | _ -> ())

let create () =
  let store = Store.create () in
  let ns = Namespace.create () in
  let mint_src = Mint.make_source () in
  seed ~store ~ns ~mint_src;
  { store; ns; mint_src }

let global : t = create ()
