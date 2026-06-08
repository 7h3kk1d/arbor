#!/bin/bash
# Build the jsoo bundle and copy it into public/ alongside index.html.
# Run from prototypes/p16-records/:
#   eval $(opam env --switch=. --set-switch) && scripts/build-web.sh
set -eu
cd "$(dirname "$0")/.."
dune build webmain/main.bc.js
rm -f public/p16.js
cp _build/default/webmain/main.bc.js public/p16.js
chmod u+w public/p16.js
echo "built public/p16.js ($(wc -c <public/p16.js | tr -d ' ') bytes)"
echo "open public/index.html in a browser."
