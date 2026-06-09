/* The Attachment layer: associated data outside the content hash (design/06).
   p12's first aspect is the asserted `test` marker — a hash tagged `test` is a
   boolean expression the interface runs and reports pass/fail (the pass/fail
   itself is derived live by evaluation, not stored). Keyed by aspect id (a
   string) so more aspects can be added without changing the shape. */

type t = {marks: Hashtbl.t(string, Hashtbl.t(Hash.t, unit))};

let create = (): t => {marks: Hashtbl.create(16)};

let set_for = (t: t, aspect: string): Hashtbl.t(Hash.t, unit) =>
  switch (Hashtbl.find_opt(t.marks, aspect)) {
  | Some(s) => s
  | None =>
    let s = Hashtbl.create(64);
    Hashtbl.replace(t.marks, aspect, s);
    s;
  };

let mark = (t: t, ~aspect: string, h: Hash.t): unit =>
  Hashtbl.replace(set_for(t, aspect), h, ());

let unmark = (t: t, ~aspect: string, h: Hash.t): unit =>
  Hashtbl.remove(set_for(t, aspect), h);

let has = (t: t, ~aspect: string, h: Hash.t): bool =>
  Hashtbl.mem(set_for(t, aspect), h);

let marked = (t: t, ~aspect: string): list(Hash.t) =>
  Hashtbl.fold((h, _, acc) => [h, ...acc], set_for(t, aspect), []);
