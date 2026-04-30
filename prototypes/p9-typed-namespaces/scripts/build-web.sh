#!/bin/bash
# Build the jsoo bundle and copy it into public/ alongside index.html.
# Run from prototypes/p9-typed-namespaces/:
#   eval $(opam env --switch=. --set-switch) && scripts/build-web.sh
set -eu
cd "$(dirname "$0")/.."
dune build bin/p9_main.bc.js
rm -f public/p9.js
cp _build/default/bin/p9_main.bc.js public/p9.js
chmod u+w public/p9.js
echo "built public/p9.js ($(wc -c <public/p9.js | tr -d ' ') bytes)"
echo "open public/index.html in a browser."
