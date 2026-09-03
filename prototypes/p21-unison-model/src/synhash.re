/* The syntactic hash, and the naming function it runs under.
 * (def:namer, def:synhash, rem:syn-dual, lem:prop-silent)
 *
 * This is Abt.hash with exactly one clause reversed. The content hash erases
 * NAMES and keeps HASHES: a binder becomes a de Bruijn index, a reference
 * contributes its referent's hash. The syntactic hash erases HASHES and keeps
 * NAMES: a binder still becomes a de Bruijn index, and a reference contributes
 * the NAME the merge gives it, as text. Same recursion, same treatment of
 * binders, opposite substitution (rem:syn-dual).
 *
 * The content hash is a definition's identity and is stored. This one is
 * computed during a merge, used for one decision, and thrown away: nothing
 * persists it and no operation retrieves a definition by it.
 *
 * Two consequences fall straight out of the definition:
 *
 *   - It is a function of the NAMESPACE as well as the term. The same stored
 *     definition has different syntactic hashes under different namers.
 *   - Equality of syntactic hashes is equality of RENDERED FORM. Two
 *     definitions share one exactly when a reader shown both under the same
 *     namer would see the same code. Everything in merge.re is built on that
 *     reading. */

/* nu : Referent -> Name, partial. (def:namer)
 *
 * One fully qualified name per referent, taken from Alice if Alice names it at
 * all, else Bob, else the LCA. The bias is by REFERENT, not by name: if Alice
 * has any name for r, none of Bob's names for r are considered. Names are
 * atomic here, so there is no suffixification and no lib canonicalization --
 * see rem:libcanon for the one place the real system repairs a rename by hand,
 * which this model does not need because it has no lib.
 *
 * Among several names for one referent the real PPE takes the fewest-segment
 * one; here names are atomic, so it takes the least alphabetically, which is
 * what Names.names_of already sorts by. Deterministic either way, and
 * rem:onlyrenames records that an alias is enough to change the choice. */
type t = Names.t;

let make = (~lca: Names.t, ~alice: Names.t, ~bob: Names.t): t =>
  Names.preferring(Names.preferring(alice, bob), lca);

let name_of = (nu: t, r: Reference.referent): option(string) =>
  switch (Names.names_of(nu, r)) {
  | [] => None
  | [n, ..._] => Some(n)
  };

/* Tags. Deliberately a separate space from Abt's: this hash is never stored and
   never compared against a content hash, so nothing requires them to agree, and
   keeping them apart makes it impossible to confuse the two by accident. */
let layer_tag = 2;
let s_var = 0
and s_lam = 1
and s_app = 2
and s_nat = 3
and s_text = 4
and s_ref_named = 5
and s_ref_anon = 6
and s_ann = 7;

/* syn_nu(t). (def:synhash)
 *
 * The Ref case is the whole difference from Abt.hash. A reference contributes
 * the name nu gives it; a referent nu does not name contributes its hash, which
 * is the degenerate case where the syntactic hash falls back to the content
 * hash because there is no name to substitute. */
let rec hash = (nu: t, env: list(string), tm: Term.t): Hash.t =>
  switch (tm) {
  | Term.Var(x) =>
    let rec idx = (j, e) =>
      switch (e) {
      | [] => raise(Abt.Free_variable(x))
      | [y, ...rest] => String.equal(x, y) ? j : idx(j + 1, rest)
      };
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(s_var),
      Hash.Nat(idx(0, env)),
    ]);
  | Term.Lam(x, b) =>
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(s_lam),
      Hash.Hashed(hash(nu, [x, ...env], b)),
    ])
  | Term.App(f, a) =>
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(s_app),
      Hash.Hashed(hash(nu, env, f)),
      Hash.Hashed(hash(nu, env, a)),
    ])
  | Term.Nat(n) =>
    Hash.acc([Hash.Tag(layer_tag), Hash.Tag(s_nat), Hash.Nat(n)])
  | Term.Text(s) =>
    Hash.acc([Hash.Tag(layer_tag), Hash.Tag(s_text), Hash.Text(s)])
  /* The reference clause, and the whole difference from Abt.hash.
     PROPAGATION IS INVISIBLE HERE (lem:prop-silent): if a term is rebuilt with
     each reference r replaced by an r' that nu gives the SAME name, this clause
     emits the same token both times and the two syntactic hashes agree. That is
     why update can rebuild a thousand dependents and merge can tell that no
     human touched any of them -- and it holds only while names are stable,
     which is the hypothesis every guarantee in merge.re inherits. */
  | Term.Ref(r) =>
    switch (name_of(nu, Reference.Ref(r))) {
    | Some(n) =>
      Hash.acc([
        Hash.Tag(layer_tag),
        Hash.Tag(s_ref_named),
        Hash.Text(n),
      ])
    | None =>
      let h =
        switch (r) {
        | Reference.Builtin(b) => Hash.acc([Hash.Text(b)])
        | Reference.Derived(h, i) =>
          Hash.acc([Hash.Hashed(h), Hash.Nat(i)])
        };
      Hash.acc([Hash.Tag(layer_tag), Hash.Tag(s_ref_anon), Hash.Hashed(h)]);
    }
  | Term.Ann(e, ty) =>
    Hash.acc([
      Hash.Tag(layer_tag),
      Hash.Tag(s_ann),
      Hash.Hashed(hash(nu, env, e)),
      Hash.Hashed(Ty.hash(ty)),
    ])
  };

let of_term = (nu: t, tm: Term.t): Hash.t => hash(nu, [], tm);

/* The syntactic hash of whatever a referent denotes. A referent the store does
   not know is hashed as itself -- the case cannot arise for the branches merge
   is given, since a namespace only names things that were ingested. */
let of_referent =
    (st: Store.t, nu: t, r: Reference.referent): option(Hash.t) =>
  switch (r) {
  | Reference.Ref(rr) =>
    switch (Store.lookup(st, rr)) {
    | Some(tm) => Some(of_term(nu, tm))
    | None => None
    }
  | Reference.Con(_, _) => None
  };
