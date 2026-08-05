#!/bin/sh
# check-labels.sh — report paper statements with no counterpart in the Agda
# development, and Agda references to labels the paper no longer has.
#
#   usage: scripts/check-labels.sh [paper.tex] [agda-dir]
#   default: paper/arbor-core.tex agda/Arbor
#
# The convention this enforces: every \label{def:x} / \label{thm:x} / ... in the
# paper is mentioned somewhere in the Agda sources as "(def:x)" / "(thm:x)".
# That is what keeps the two artifacts from drifting silently — the failure mode
# that produced this script (agda/README.md sat unchanged across the whole
# arbor-stlc landing, and both it and decisions.md still indexed theorems as
# T1-T8, a numbering the papers had stopped using).
#
# Remarks and examples are prose, not statements: not required, accepted when
# present.
#
# Exit status: 0 if every paper statement is mirrored, 1 otherwise.

set -eu

here=$(dirname "$0")
tex=${1:-"$here/../paper/arbor-core.tex"}
src=${2:-"$here/../agda/Arbor"}

[ -f "$tex" ] || { echo "no such paper: $tex" >&2; exit 2; }
[ -d "$src" ] || { echo "no such directory: $src" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

kinds='def|lem|prop|thm|cor|rem|ex'

grep -oE "\\\\label\{($kinds):[A-Za-z0-9-]+\}" "$tex" \
  | sed 's/.*{//; s/}//' | sort -u \
  | grep -v '^rem:' | grep -v '^ex:' > "$tmp/paper" || true

grep -rhoE "\(($kinds):[A-Za-z0-9-]+\)" "$src" \
  | sed 's/^(//; s/)$//' | sort -u > "$tmp/agda" || true

comm -23 "$tmp/paper" "$tmp/agda" > "$tmp/missing"
comm -13 "$tmp/paper" "$tmp/agda" | grep -v '^rem:' | grep -v '^ex:' > "$tmp/stale" || true

n_paper=$(wc -l < "$tmp/paper" | tr -d ' ')
n_missing=$(wc -l < "$tmp/missing" | tr -d ' ')

echo "paper:  $tex"
echo "agda:   $src"
echo "labels: $n_paper statements in the paper, $n_missing not mirrored"

if [ -s "$tmp/missing" ]; then
  echo
  echo "NOT MIRRORED (no \"(label)\" mention in any .agda file):"
  sed 's/^/  /' "$tmp/missing"
fi

if [ -s "$tmp/stale" ]; then
  echo
  echo "STALE (mentioned in Agda, absent from this paper — check the other rung):"
  sed 's/^/  /' "$tmp/stale"
fi

if [ -s "$tmp/missing" ]; then exit 1; fi
echo
echo "OK — every statement in the paper is mirrored."
