/* Strongly connected components of a binding group, in dependency order.
 *
 * Tarjan. The unit that can be given an identity is the COMPONENT, not the
 * definition: a member of a cycle cannot be hashed before its neighbours, since
 * it refers to them and they refer back (rem:mutrec). With single-definition
 * ingest every component would be a singleton and def:comphash would be
 * invisible.
 *
 * Members within a component come back in NAME order, because that is what
 * def:comphash is defined over. */

/* graph: an association list from name to the names it mentions. Edges to
   names outside the graph are ignored. */
let components = (graph: list((string, list(string)))): list(list(string)) => {
  let nodes = List.map(fst, graph);
  let succs = n =>
    switch (List.assoc_opt(n, graph)) {
    | None => []
    | Some(ds) => List.filter(d => List.mem(d, nodes), ds)
    };

  let index = Hashtbl.create(16);
  let low = Hashtbl.create(16);
  let on_stack = Hashtbl.create(16);
  let stack = ref([]);
  let counter = ref(0);
  let out = ref([]);

  let rec strongconnect = v => {
    Hashtbl.replace(index, v, counter^);
    Hashtbl.replace(low, v, counter^);
    incr(counter);
    stack := [v, ...stack^];
    Hashtbl.replace(on_stack, v, true);

    List.iter(
      w =>
        if (!Hashtbl.mem(index, w)) {
          strongconnect(w);
          Hashtbl.replace(low, v, min(Hashtbl.find(low, v), Hashtbl.find(low, w)));
        } else if (Hashtbl.mem(on_stack, w)) {
          Hashtbl.replace(low, v, min(Hashtbl.find(low, v), Hashtbl.find(index, w)));
        },
      succs(v),
    );

    if (Hashtbl.find(low, v) == Hashtbl.find(index, v)) {
      let comp = ref([]);
      let continue_ = ref(true);
      while (continue_^) {
        switch (stack^) {
        | [] => continue_ := false
        | [w, ...rest] =>
          stack := rest;
          Hashtbl.remove(on_stack, w);
          comp := [w, ...comp^];
          if (String.equal(w, v)) {
            continue_ := false;
          };
        };
      };
      /* Tarjan emits components in reverse topological order, i.e.
         dependencies first, which is exactly the order ingest needs. */
      out := [List.sort(String.compare, comp^), ...out^];
    };
  };

  List.iter(v => if (!Hashtbl.mem(index, v)) {strongconnect(v)}, nodes);
  List.rev(out^);
};
