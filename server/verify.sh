#!/bin/bash
# verify.sh — checks the games waiting on the server: fetches each recording, plays it again
# with encore-play --verify against the game's own files, and reports what it found. The
# GitHub Actions job runs this every few minutes; it can as well be run by hand.
#
#   ENCORE_API       the Worker, e.g. https://thebestpinball.com
#   VERIFIER_TOKEN   the Worker's secret of the same name
#   ENCORE_PLAY      the encore-play program, built from the same version as the game
#   GAME_DIR         the folder with the game's TABLE1.PRG ... TABLE4.MOD; without it they
#                    are fetched from the server, where upload-game-files.sh put them
#
# Nothing it prints contains the game's files or the token.
set -euo pipefail

: "${ENCORE_API:?}" "${VERIFIER_TOKEN:?}" "${ENCORE_PLAY:?}"
auth=(-H "Authorization: Bearer $VERIFIER_TOKEN")
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

ids=$(curl -fsS "${auth[@]}" "$ENCORE_API/v1/verifier/pending?limit=200" | jq -r '.runs[].id')
[ -z "$ids" ] && { echo "nothing waiting"; exit 0; }

if [ -z "${GAME_DIR:-}" ]; then
  GAME_DIR="$work/game"
  mkdir -p "$GAME_DIR"
  for n in 1 2 3 4; do
    for ext in PRG MOD; do
      curl -fsS "${auth[@]}" -o "$GAME_DIR/TABLE$n.$ext" "$ENCORE_API/v1/verifier/files/TABLE$n.$ext"
    done
  done
fi

for id in $ids; do
  curl -fsS "${auth[@]}" -o "$work/$id.RPL" "$ENCORE_API/v1/verifier/runs/$id/replay"
  # A recording it cannot play comes back as {"ok":false,...} and an exit code of 1; only a
  # missing table file (2) means the verifier itself is wrong, and then nothing is reported.
  set +e
  verdict=$("$ENCORE_PLAY" "$GAME_DIR" --verify "$work/$id.RPL")
  code=$?
  set -e
  if [ $code -gt 1 ] || [ -z "$verdict" ]; then
    echo "game $id: the verifier could not run ($code)" >&2
    exit 1
  fi
  result=$(curl -fsS "${auth[@]}" -H "Content-Type: application/json" --data "$verdict" \
    "$ENCORE_API/v1/verifier/runs/$id")
  echo "game $id: $(jq -r '.status + (if .reason then " (" + .reason + ")" else "" end)' <<<"$result")"
done
