# p13 — Open questions

Running list. Findings that generalize bubble up to `docs/design/12-type-abstraction.md`.

## System-F core

- **`∀` instantiation depth (predicative vs impredicative).** The driving program
  only instantiates type variables at ground/opaque types. Whether to allow
  instantiating at a `∀` type (impredicative) is untested; predicative is assumed.
- **Where the type-var context lives in the checker.** `synth` wraps `TyLam`
  bodies in `Forall` without an explicit depth counter; confirm this stays correct
  once `TyApp`'s `subst_ty` and nested `/\` interact.
- **Opaque witnesses under type variables.** Witnesses are assumed closed (no
  `TVar`); `shift_ty`/`subst_ty` therefore treat `Opaque` as closed. Polymorphic
  sealing (`/\t. struct … :> …`) would break that assumption — deferred.

## Surface / ergonomics

- **Type-lambda syntax.** `/\t. e` chosen (token `/\`); `e [T]` for application;
  `forall t. T` for the type. Revisit if the `/\` / `[ ]` lexing collides with
  anything once more surface lands.
- **Multi-parameter type abstraction.** Only single-binder `/\t.` / `forall t.`;
  `/\(a b).` sugar is deferred.
- **Pretty-printing `∀`.** Type-var binder names are regenerated (no stored
  names), like term binders — confirm the rendering reads well for nested `∀`.

## Toward the deferred `∃`

- **First-class modules / existentials.** The original `(C : { type t; … }) -> C.t`
  needs `∃` (pack/unpack) — factories, heterogeneous collections, stored modules.
  What is the minimal native `pack`/`unpack` on this substrate, and does it reuse
  the `Forall` machinery (Church-encoded `∃` needs higher-rank `∀`)? For a later
  prototype; tracked in `12`.
