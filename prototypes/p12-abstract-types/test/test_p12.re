/* p12 substrate tests. Starts with the primitive layer (Hash, Mint); grows as
   Tnode / Node / Typecheck / editing-context land. */

let test_hash_determinism = () => {
  let h1 = Hash.digest_string("hello");
  let h2 = Hash.digest_string("hello");
  let h3 = Hash.digest_string("world");
  Alcotest.(check(bool))("same bytes hash equal", true, Hash.equal(h1, h2));
  Alcotest.(check(bool))("different bytes differ", false, Hash.equal(h1, h3));
};

let test_mint_distinct = () => {
  let src = Mint.make_source();
  let m1 = Mint.fresh(src);
  let m2 = Mint.fresh(src);
  Alcotest.(check(bool))("fresh mints differ", false, Mint.equal(m1, m2));
};

let test_mint_reproducible = () => {
  /* Two independent sources draw the same sequence — re-ingest reproducibility. */
  let a = Mint.make_source();
  let b = Mint.make_source();
  let a0 = Mint.fresh(a);
  let _ = Mint.fresh(a);
  let b0 = Mint.fresh(b);
  Alcotest.(check(bool))("first draw matches across sources", true, Mint.equal(a0, b0));
};

let () =
  Alcotest.run(
    "p12",
    [
      (
        "primitives",
        [
          Alcotest.test_case("hash determinism", `Quick, test_hash_determinism),
          Alcotest.test_case("mint distinctness", `Quick, test_mint_distinct),
          Alcotest.test_case("mint reproducible", `Quick, test_mint_reproducible),
        ],
      ),
    ],
  );
