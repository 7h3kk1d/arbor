/* Primitives, named by TEXT rather than by hash. (rem:builtin)
 *
 * An identifier is intrinsic when it can be recomputed from the thing it names
 * and extrinsic when it is an assertion by some authority. A hash is the
 * former; a builtin name is the latter. So the system is intrinsically
 * identified everywhere except at its primitives, and the boundary is visible
 * in the type of a reference rather than hidden in a convention.
 *
 * A hash chain must bottom out at things whose content the system does not
 * model, so this is not avoidable; it is worth noting only because the sum type
 * makes the seam unusually legible. */

let all: list((string, Ty.t)) = [
  ("Nat.+", Ty.Arrow(Ty.Nat, Ty.Arrow(Ty.Nat, Ty.Nat))),
  ("Nat.*", Ty.Arrow(Ty.Nat, Ty.Arrow(Ty.Nat, Ty.Nat))),
  ("Nat.drop", Ty.Arrow(Ty.Nat, Ty.Arrow(Ty.Nat, Ty.Nat))),
  ("Text.size", Ty.Arrow(Ty.Text, Ty.Nat)),
  ("Text.++", Ty.Arrow(Ty.Text, Ty.Arrow(Ty.Text, Ty.Text))),
  ("Text.repeat", Ty.Arrow(Ty.Nat, Ty.Arrow(Ty.Text, Ty.Text))),
];

let type_of = (name: string): option(Ty.t) => List.assoc_opt(name, all);

let names: list(string) = List.map(fst, all);

let exists = (name: string): bool => List.mem_assoc(name, all);
