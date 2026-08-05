#!/bin/sh
# status.sh — what is proved, and what is only stated.
#
#   usage: scripts/status.sh [Meta.agda]
#   default: agda/Arbor/Core/Meta.agda
#
# `agda --safe` passing does NOT mean the paper is proved: Meta.agda states
# every result as a named type, and an unproved one is simply a type with no
# inhabitant. This script reads the status markers in Meta.agda's comments:
#
#   [proved]           an inhabitant is given
#   [by-construction]  discharged by a datatype or module declaration
#   [spec, Mn]         discharged from a specification record; the obligation
#                      transfers to milestone n's construction of an inhabitant
#   [open, Mn]         stated only; scheduled for milestone n
#   [deferred, Mn]     not yet statable; needs a construction from milestone n

set -eu

here=$(dirname "$0")
meta=${1:-"$here/../agda/Arbor/Core/Meta.agda"}

if [ ! -f "$meta" ]; then echo "no such file: $meta" >&2; exit 2; fi

# Only lines in the canonical form "-- (label) [marker] — description" count;
# the legend at the top of Meta.agda and prose mentions of a marker do not.
canonical='\([a-z]*:[A-Za-z0-9-]*\) \[[a-z-]*[^]]*\] — '

count() { grep -cE "\($1:[A-Za-z0-9-]+\) \[$2[^]]*\] — " "$meta" || true; }

show() { # marker, heading
  echo "$2 ($(count '[a-z]*' "$1"))"
  grep -oE "\([a-z]+:[A-Za-z0-9-]+\) \[$1[^]]*\] — .*" "$meta" | sed 's/^/  /' || true
  echo
}

echo "arbor-core mechanization status"
echo "  source: $meta"
echo

show 'proved'          'PROVED'
show 'by-construction' 'BY CONSTRUCTION'
show 'spec'            'FROM SPEC — obligation transferred to a later milestone'
show 'open'            'OPEN — stated, not proved'
show 'deferred'        'DEFERRED — not yet statable'

total=$(grep -cE "\([a-z]+:[A-Za-z0-9-]+\) \[[a-z-]+[^]]*\] — " "$meta" || true)
done_n=$(( $(count '[a-z]*' proved) + $(count '[a-z]*' by-construction) ))
echo "$done_n of $total statements discharged."
