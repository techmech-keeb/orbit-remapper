#!/usr/bin/env bash
# Builds the Orbit firmware and names the merged image by the firmware
# naming standard (ai-agent-playbook domains/keyboard/firmware-naming.md):
#   orbit_hid-remapper_v<version>_<YYYYMMDD>-<sha>.bin
# The same script runs in CI (build.yml), in the release workflow
# (release.yml) and by hand, so the three never build differently.
#
# Usage: dev/tools/build/build-firmware.sh [build-dir]
#   ORBIT_RELEASE=1  release build: the image must report v<version>
# Prints FW_BASE=<path without .bin> on the last line.
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
project="$root/firmware/orbit"
build="${1:-$project/build}"

if ! command -v idf.py > /dev/null; then
    . "${IDF_PATH:?IDF_PATH is not set}/export.sh" > /dev/null
fi

version="$(head -n 1 "$project/version.txt")"
sha="$(git -C "$root" rev-parse --short=7 HEAD)"
date="$(date -u +%Y%m%d)"

idf.py -C "$project" -B "$build" build merge-bin

# The version esp_app_get_description() reports: the 32 bytes after the
# magic word, secure_version and two reserved words of esp_app_desc_t.
reported="$(python3 - "$build/orbit.bin" <<'PY'
import sys
image = open(sys.argv[1], "rb").read()
at = image.find(bytes.fromhex("3254cdab"))
print(image[at + 16:at + 48].split(b"\0")[0].decode() if at >= 0 else "")
PY
)"
if [ "${ORBIT_RELEASE:-}" = "1" ] && [ "$reported" != "v$version" ]; then
    printf 'release build reports "%s", expected "v%s" (stale build directory?)\n' "$reported" "$version" >&2
    exit 1
fi

base="$build/orbit_hid-remapper_v${version}_${date}-${sha}"
cp "$build/merged-binary.bin" "$base.bin"
printf 'version=%s reported=%s\n' "$version" "$reported"
printf 'FW_BASE=%s\n' "$base"
