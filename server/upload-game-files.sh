#!/bin/bash
# upload-game-files.sh — puts the original game's table files where the verifier can fetch
# them: on the server, readable only with the verifier's token. Run it once, from a machine
# that has the game.
#
#   ENCORE_API       the Worker, e.g. https://thebestpinball.com
#   VERIFIER_TOKEN   the Worker's secret of the same name
#   upload-game-files.sh <the game's folder>
set -euo pipefail

: "${ENCORE_API:?}" "${VERIFIER_TOKEN:?}"
dir="${1:?give the folder with TABLE1.PRG ... TABLE4.MOD}"
for n in 1 2 3 4; do
  for ext in PRG MOD; do
    f="$dir/TABLE$n.$ext"
    [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
    curl -fsS -X PUT -H "Authorization: Bearer $VERIFIER_TOKEN" --data-binary @"$f" \
      "$ENCORE_API/v1/verifier/files/TABLE$n.$ext" | jq -r '"\(.name)  \(.size) bytes  \(.sha256)"'
  done
done
