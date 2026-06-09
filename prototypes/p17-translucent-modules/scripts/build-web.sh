#!/bin/bash
# Build the jsoo bundle and copy it into public/ alongside index.html.
# Run from prototypes/p17-translucent-modules/:
#   eval $(opam env --switch=. --set-switch) && scripts/build-web.sh
set -eu
cd "$(dirname "$0")/.."
dune build webmain/main.bc.js
rm -f public/p17.js
cp _build/default/webmain/main.bc.js public/p17.js
chmod u+w public/p17.js
echo "built public/p17.js ($(wc -c <public/p17.js | tr -d ' ') bytes)"
echo "open public/index.html in a browser."
