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
  languages: ["p9"],
};

let peek_cache = (att: Attachment.t, h: Hash.t): option(bool) =>
  switch (
    Attachment.peek(att, ~target=h, ~aspect=aspect_id, ~procedure=procedure_id)
  ) {
  | Some(Attachment.Has_holes(b)) => Some(b)
  | _ => None
  };

let rec compute = (~store: Store.t, ~att: Attachment.t, h: Hash.t): bool =>
  switch (peek_cache(att, h)) {
  | Some(b) => b
  | None =>
    let result =
      switch (Store.lookup(store, h)) {
      | None => false
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
