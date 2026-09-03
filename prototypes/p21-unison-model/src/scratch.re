/* The scratch buffer's own state, kept deliberately separate from the Store.
 *
 * This is the counterpart of Unison's editing context, which is not an object in
 * the codebase at all -- it is two fields of an in-memory record:
 *
 *     latestFile            :: Maybe (FilePath, Bool)
 *     latestTypecheckedFile :: Maybe (Either UnisonFile TypecheckedUnisonFile)
 *
 * Three properties of that shape are worth preserving rather than improving on,
 * because they are what the model actually says:
 *
 *   - It is a `Maybe`, not a set. There is exactly one buffer; it cannot be
 *     named, shared, or held open alongside another.
 *   - It holds NO HASHES. A definition here is name-keyed syntax; identity is
 *     conferred by typechecking (def:uingest), so before that there is nothing
 *     to key anything by.
 *   - It dies with the process. Nothing here is persisted, and nothing here is
 *     content-addressed.
 *
 * So this module is a cache in the ordinary sense -- recomputable from the text,
 * discardable at any moment -- and it is kept out of `Store` on purpose. Mixing
 * them would suggest a buffer definition is a stored one, which is the single
 * thing p21 exists to say it is not.
 *
 * What it buys, beyond fidelity, is that the pipeline becomes inspectable: each
 * stage's output is a value someone can look at, rather than an intermediate
 * inside one function. */

/* Unison's `Either UnisonFile TypecheckedUnisonFile` distinguishes "parsed but
   did not typecheck" from "parsed and typechecked". The stages below are that
   distinction, opened out -- a buffer stops at the first one that fails. */
type stage =
  | Unparsed /* not even a stanza list */
  | Parsed /* stanzas, but the group is malformed */
  | Resolved /* names decided; may still not typecheck */
  | Typechecked /* the group is well-typed */
  | Hashed; /* hashed, and any watches evaluated and cached */

let stage_label = (s: stage): string =>
  switch (s) {
  | Unparsed => "unparsed"
  | Parsed => "parsed"
  | Resolved => "resolved"
  | Typechecked => "typechecked"
  | Hashed => "hashed"
  };

let stage_index = (s: stage): int =>
  switch (s) {
  | Unparsed => 0
  | Parsed => 1
  | Resolved => 2
  | Typechecked => 3
  | Hashed => 4
  };

/* One binding, carried through the pipeline with everything each stage learned
   about it. `hash` stays `None` for the whole life of this record -- it is only
   ever populated by a commit, and a commit replaces the record. */
type binding = {
  name: string,
  ty: Ty.t,
  source: Term.Source.t,
  resolved: Term.t,
  decisions: list(Resolve.decision),
  /* free variables still in the resolved term, i.e. the ones resolution
     deliberately did not discharge (rem:staging) */
  free: list(string),
  status: Slurp.status,
  /* type error, if this binding is why the buffer did not typecheck */
  error: option(string),
  /* The reference this definition received AT SAVE. Present as soon as the
     buffer typechecks, and NOT dependent on committing -- storing and naming
     are a separate gesture. Unison's TypecheckedUnisonFile carries the same
     thing in `hashTermsId`. */
  reference: option(Reference.t),
};

type watch = {
  index: int,
  source: Term.Source.t,
  resolved: Term.t,
  decisions: list(Resolve.decision),
  ty: option(Ty.t),
  error: option(string),
  /* as for a binding: conferred at save */
  reference: option(Reference.t),
  /* the cached or freshly computed result, and whether the cache had it */
  value: option(Term.t),
  cache_hit: bool,
  eval_error: option(string),
};

/* The strongly connected components the group decomposes into, in dependency
   order -- computed here rather than at ingest so the user can see the grouping
   BEFORE committing, which is when it is still actionable. */
type component = {
  members: list(string),
  recursive: bool,
};

type t = {
  /* latestFile */
  text: string,
  below_fold: string,
  /* latestTypecheckedFile, opened out */
  stage,
  parse_errors: list(Parse.located_error),
  problems: list(Slurp.problem),
  bindings: list(binding),
  watches: list(watch),
  components: list(component),
  /* how many times the evaluator actually ran during this save */
  evaluated: int,
  /* entries in the watch cache afterwards */
  cache_size: int,
};

let empty: t = {
  text: "",
  below_fold: "",
  stage: Unparsed,
  parse_errors: [],
  problems: [],
  bindings: [],
  watches: [],
  components: [],
  evaluated: 0,
  cache_size: 0,
};

let free_vars_of = (tm: Term.t): list(string) => Term.free_vars(tm);

/* Run the whole read-only pipeline over a buffer. Mutates nothing: no store, no
   namespace, no cache. This is what a keystroke costs.
 *
 * Note what it CANNOT do, and why that is the model rather than a shortcut: it
 * cannot report a hash, because nothing here has one; and it cannot evaluate a
 * binding, because there is no stored body to evaluate. Both become available
 * only at commit. */
let analyze_only = (~st: Store.t, ~ns: Names.t, text: string): t => {
  let (file, parse_errors) = Parse.file(text);
  let (entries, problems) = Slurp.entries(file);
  let raw_watches = Slurp.watches(file);
  let base = {
    ...empty,
    text,
    below_fold: file.below_fold,
    parse_errors,
    problems,
  };
  if (parse_errors != []) {
    {...base, stage: Unparsed};
  } else {
    let classified = Slurp.classify(ns, entries);
    let group = List.map((e: Slurp.entry) => e.name, classified);
    let env = Store.build_env(st);
    let ctx = List.map((e: Slurp.entry) => (e.name, e.ty), classified);

    let bindings =
      List.map(
        (e: Slurp.entry) => {
          let (resolved, decisions) =
            Resolve.term_traced(~group, ~ns, e.source);
          let error =
            switch (Typecheck.check(env, ctx, resolved, e.ty)) {
            | () => None
            | exception (Typecheck.Type_error(m)) => Some(m)
            };
          {
            name: e.name,
            ty: e.ty,
            source: e.source,
            resolved,
            decisions,
            free: free_vars_of(resolved),
            status: e.status,
            error,
            reference: None,
          };
        },
        classified,
      );

    let watches =
      List.mapi(
        (i, src) => {
          let (resolved, decisions) = Resolve.term_traced(~group, ~ns, src);
          let (ty, error) =
            switch (Typecheck.synth(env, ctx, resolved)) {
            | t => (Some(t), None)
            | exception (Typecheck.Type_error(m)) => (None, Some(m))
            };
          {
            index: i + 1,
            source: src,
            resolved,
            decisions,
            ty,
            error,
            reference: None,
            value: None,
            cache_hit: false,
            eval_error: None,
          };
        },
        raw_watches,
      );

    /* The SCC decomposition ingest would perform. Shown pre-commit because the
       grouping is what decides which definitions share a hash, and that is the
       one structural consequence a user cannot see in their own text. */
    let graph =
      List.map(
        (b: binding) =>
          (b.name, List.filter(v => List.mem(v, group), free_vars_of(b.resolved))),
        bindings,
      );
    let components =
      List.map(
        members => {
          let recursive =
            switch (members) {
            | [only] =>
              /* a singleton is recursive iff it mentions itself */
              switch (List.find_opt((b: binding) => b.name == only, bindings)) {
              | Some(b) => List.mem(only, free_vars_of(b.resolved))
              | None => false
              }
            | _ => true
            };
          {members, recursive};
        },
        Scc.components(graph),
      );

    let ill_typed =
      List.exists((b: binding) => b.error != None, bindings)
      || List.exists((w: watch) => w.error != None, watches);
    let stage =
      if (problems != []) {
        Parsed;
      } else if (ill_typed) {
        Resolved;
      } else {
        Typechecked;
      };
    {...base, stage, bindings, watches, components};
  };
};

let is_committable = (s: t): bool => s.stage == Typechecked;

/* ---- save ----------------------------------------------------------------
 *
 * What writing the scratch file does, and it is more than analysis.
 *
 * Unison's save path (`loadUnisonFile` -> `evalUnisonFile`) typechecks, hashes
 * the whole file INCLUDING its watches, evaluates the watches against a code
 * lookup that covers the file itself, and writes each result to the watch cache
 * -- but `only when (not isHit)`, and with no `addDefsToCodebase` anywhere. So
 * a save confers identity and populates the cache; it does not store or name
 * anything.
 *
 * That distinction is the whole reason re-saving an expensive watch is free
 * while a fresh one is not, and it holds with no commit in sight. Measured on
 * ucm 1.1.1: one watch 5.79 s, the same watch saved twice 5.82 s, two distinct
 * watches 11.45 s.
 *
 * Note what this means for the earlier framing "identity is conferred at
 * commit": it is wrong, and this function is where. Identity is conferred by
 * TYPECHECKING (rem:saveident). Commit is what stores and binds. This is
 * (def:usave), and where a watch parts company from a definition is
 * (rem:watchsplit). */
let save = (~st: Store.t, ~ns: Names.t, ~w: Watch.t, text: string): t => {
  let sc = analyze_only(~st, ~ns, text);
  if (sc.stage != Typechecked) {
    {...sc, cache_size: Watch.size(w)};
  } else {
    let group =
      switch (
        Slurp.to_group(
          ~st,
          ns,
          Slurp.classify(
            ns,
            List.map((b: binding) => (b.name, b.ty, b.source), sc.bindings),
          ),
          List.map((wa: watch) => wa.source, sc.watches),
        )
      ) {
      | Ok(g) => Some(g)
      | Error(_) => None
      };
    switch (group) {
    | None => {...sc, cache_size: Watch.size(w)}
    | Some(group) =>
      /* Hash WITHOUT storing. Same hashes a commit would produce. */
      switch (Ingest.hash_components(~register=false, st, group)) {
      | Error(_) => {...sc, cache_size: Watch.size(w)}
      | Ok(o) =>
        let before = Eval.evaluations^;
        /* The file's own definitions, addressable by the references they just
           received but present in no store -- Unison's CodeLookup over the
           file. Without this a watch could not mention an uncommitted
           definition, which it plainly can. */
        let overlay = o.defs;
        let bindings =
          List.map(
            (b: binding) => {
              ...b,
              reference: List.assoc_opt(b.name, o.bindings),
            },
            sc.bindings,
          );
        /* o.watches is in the order Ingest produced; sc.watches is in source
           order. They are the same length and same order, since watches are
           appended to the group in source order and each is its own
           singleton component. */
        let watches =
          List.map2(
            (wa: watch, (r, body, _ty)) => {
              let (res, hit) = Eval.watch_at(~overlay, st, w, r, body);
              {
                ...wa,
                reference: Some(r),
                value:
                  switch (res) {
                  | Ok(v) => Some(v)
                  | Error(_) => None
                  },
                cache_hit: hit,
                eval_error:
                  switch (res) {
                  | Ok(_) => None
                  | Error(m) => Some(m)
                  },
              };
            },
            sc.watches,
            o.watches,
          );
        {
          ...sc,
          stage: Hashed,
          bindings,
          watches,
          evaluated: Eval.evaluations^ - before,
          cache_size: Watch.size(w),
        };
      }
    };
  };
};

/* Kept for callers that want analysis without touching the cache -- the tests
   use it to assert that analysis alone is inert. */
let analyze = analyze_only;
