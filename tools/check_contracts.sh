#!/bin/sh
# Verify that contracts/ still matches the copies this repo was built against.
#
# WHY THIS EXISTS. The DBC, the CAN ID map and the CAN2 service protocol are a
# contract between two firmwares in two separate repositories. Their official
# home is VF_Main; the copies in contracts/ are exactly that - copies. Nothing
# stops somebody editing the original and forgetting this repo, and the failure
# that produces is silent: the module keeps transmitting a layout the analysis
# tools no longer expect, and every .blf recorded from that point is wrong in a
# way nobody notices for weeks.
#
# So the divergence is made loud instead. This runs in the build. If a hash
# moved, the build stops and names the file.
#
# Usage:
#   tools/check_contracts.sh            verify (exit 1 on mismatch)
#   tools/check_contracts.sh --update   accept current files as the new baseline
#
# --update is a deliberate act: it means "I have read the change and the
# firmware already handles it". Never run it to make a red build go green.

set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIR="$ROOT/contracts"
MANIFEST="$DIR/SHA256SUMS"

if [ ! -d "$DIR" ]; then
  echo "check_contracts: no contracts/ directory at $DIR" >&2
  exit 1
fi

# sha256sum on MSYS/Linux, shasum -a 256 on macOS.
if command -v sha256sum >/dev/null 2>&1; then
  SHA="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  SHA="shasum -a 256"
else
  echo "check_contracts: no sha256sum or shasum on PATH" >&2
  exit 1
fi

cd "$DIR"

# SHA256SUMS is the manifest itself. README.md belongs to THIS repo - it explains
# the rule rather than being part of the contract - so editing it must not stop a
# build. Everything else in here is a copy of a VF_Main file and is checked.
FILES=$(ls -1 | grep -v -e '^SHA256SUMS$' -e '^README\.md$' | sort)

if [ -z "$FILES" ]; then
  echo "check_contracts: contracts/ is empty" >&2
  exit 1
fi

if [ "$1" = "--update" ]; then
  # shellcheck disable=SC2086
  $SHA $FILES > "$MANIFEST"
  echo "check_contracts: baseline updated from the current files:"
  cat "$MANIFEST"
  echo ""
  echo "Commit SHA256SUMS together with whatever firmware change goes with it."
  exit 0
fi

if [ ! -f "$MANIFEST" ]; then
  echo "check_contracts: $MANIFEST is missing." >&2
  echo "  Run 'tools/check_contracts.sh --update' once to create it." >&2
  exit 1
fi

# A file added to contracts/ but absent from the manifest is also a divergence:
# it means somebody brought in a fourth contract without recording it.
# The leading "*" is sha256sum's binary-mode marker, not part of the name.
MANIFEST_FILES=$(awk '{ $1=""; sub(/^ +/, ""); sub(/^\*/, ""); print }' "$MANIFEST" | sort)

if [ "$FILES" != "$MANIFEST_FILES" ]; then
  echo "check_contracts: FAILED - the set of contract files changed." >&2
  echo "  in contracts/ :" >&2
  echo "$FILES" | sed 's/^/    /' >&2
  echo "  in SHA256SUMS:" >&2
  echo "$MANIFEST_FILES" | sed 's/^/    /' >&2
  exit 1
fi

if $SHA -c "$MANIFEST" >/dev/null 2>&1; then
  echo "check_contracts: ok, $(echo "$FILES" | wc -l | tr -d ' ') files match VF_Main."
  exit 0
fi

echo "check_contracts: FAILED - a contract file no longer matches." >&2
echo "" >&2
$SHA -c "$MANIFEST" 2>&1 | grep -v ': OK$' | sed 's/^/  /' >&2
echo "" >&2
echo "  These files belong to VF_Main. One of two things happened:" >&2
echo "" >&2
echo "  1. VF_Main changed the contract and this repo has not caught up." >&2
echo "     Re-copy from VF_Main, make the firmware match, then --update." >&2
echo "" >&2
echo "  2. Somebody edited the copy here. That copy is read-only by rule:" >&2
echo "     move the change to VF_Main first, then copy back." >&2
echo "" >&2
echo "  Do NOT run --update just to clear this. The point of the check is" >&2
echo "  that a layout change reaches the firmware, not that the build is green." >&2
exit 1
