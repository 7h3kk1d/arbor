#!/bin/bash
# Build the jsoo bundle and copy it into public/ alongside index.html.
# Run from prototypes/p11-mint-threads/:
#   eval $(opam env --switch=. --set-switch) && scripts/build-web.sh
set -eu
cd "$(dirname "$0")/.."
dune build bin/p11_main.bc.js
rm -f public/p11.js
cp _build/default/bin/p11_main.bc.js public/p11.js
chmod u+w public/p11.js
echo "built public/p11.js ($(wc -c <public/p11.js | tr -d ' ') bytes)"
echo "open public/index.html in a browser."
