#!/bin/bash
# Build the jsoo bundle and copy it into public/ alongside index.html.
# Run from prototypes/p10-minted-labels/:
#   eval $(opam env --switch=. --set-switch) && scripts/build-web.sh
set -eu
cd "$(dirname "$0")/.."
dune build bin/p10_main.bc.js
rm -f public/p10.js
cp _build/default/bin/p10_main.bc.js public/p10.js
chmod u+w public/p10.js
echo "built public/p10.js ($(wc -c <public/p10.js | tr -d ' ') bytes)"
echo "open public/index.html in a browser."
