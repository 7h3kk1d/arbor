/* has-holes:v1 — pure syntactic analysis cached as a derived aspect.

   Given a stored hash, returns true iff the term reachable from it
   (via child hashes in the DAG) contains any Hole node.

   Because storage is content-addressed, sub-DAGs have stable has-holes
   status; the aspect store can cache the result at every node visited
   without correctness risk. The cache uses Has_holes(bool) as the
   aspect value. */

let aspect_id: Attachment.aspect_id = "has-holes";
let procedure_id: Attachment.procedure_id = "has-holes:v1";

let descriptor: Attachment.descriptor = {
  id: aspect_id,
  disposition: Attachment.Derived,
  languages: ["p10"],
};

let peek_cache_raw = (att: Attachment.t, h: Hash.t): option(bool) =>
  switch (
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(Attachment.Has_holes(b)) => Some(b)
  | _ => None
  };

/* Public peek_cache follows Named wrappers — the aspect is cached on
   the substructure body, not on the minted wrapper. */
let peek_cache = (~store: Store.t, att: Attachment.t, h: Hash.t): option(bool) => {
  let h = Store.unwrap_named(store, h);
  peek_cache_raw(att, h);
};

let rec compute = (~store: Store.t, ~att: Attachment.t, h: Hash.t): bool =>
  switch (peek_cache_raw(att, h)) {
  | Some(b) => b
  | None =>
    let result =
      switch (Store.lookup_term(store, h)) {
      | None => false /* missing or a type definition (no holes today) */
      | Some(Node.Hole) => true
      | Some(node) =>
        List.exists(c => compute(~store, ~att, c), Node.children(node))
      };
    Attachment.attach(
      att,
      ~target=h,
      ~aspect=aspect_id,
      ~procedure=procedure_id,
      Attachment.Has_holes(result),
    );
    result;
  };
