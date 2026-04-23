#!/bin/bash
# Build the jsoo bundle and copy it into public/ alongside index.html.
# Run from prototypes/p7-web-interface/:
#   eval $(opam env --switch=. --set-switch) && scripts/build-web.sh
set -eu
cd "$(dirname "$0")/.."
dune build bin/p7_main.bc.js
rm -f public/p7.js
cp _build/default/bin/p7_main.bc.js public/p7.js
chmod u+w public/p7.js
echo "built public/p7.js ($(wc -c <public/p7.js | tr -d ' ') bytes)"
echo "open public/index.html in a browser."
