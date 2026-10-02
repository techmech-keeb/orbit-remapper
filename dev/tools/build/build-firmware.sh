#!/usr/bin/env bash
# Builds the Orbit firmware and names the merged image by the firmware
# naming standard (ai-agent-playbook domains/keyboard/firmware-naming.md):
#   orbit_hid-remapper_v<version>_<YYYYMMDD>-<sha>.bin
# The same script runs in CI (build.yml), in the release workflow
# (release.yml) and by hand, so the three never build differently.
#
# Next to the image it writes BUILD-INFO.json, the provenance record that
# ai-agent-playbook common/verification-policy.md asks for: repository,
# ref, the commit asked for and the one checked out (they differ for a
# pull request, which builds the merge commit), the workflow run, the
# version and the image's SHA-256. The file name alone is not the record.
#
# Usage: dev/tools/build/build-firmware.sh [build-dir]
#   ORBIT_RELEASE=1  release build: the image must report v<version>
#   ORBIT_HEAD_SHA   the commit asked for (CI: the pull request head)
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

dirty=false
git -C "$root" diff --quiet HEAD -- || dirty=true
upstream="$(grep -Eo 'ORBIT_UPSTREAM_COMMIT="[0-9a-f]+"' "$project/components/hid_remapper_core/CMakeLists.txt" | grep -Eo '[0-9a-f]{7,}')"
python3 - "$base.bin" "$build/BUILD-INFO.json" "$version" "$reported" "$dirty" \
    "$(git -C "$root" rev-parse HEAD)" "$(idf.py --version)" "$upstream" <<'PY'
import hashlib, json, os, sys
image, out, version, reported, dirty, checkout, idf, upstream = sys.argv[1:9]
env = os.environ.get
run = f"{env('GITHUB_SERVER_URL')}/{env('GITHUB_REPOSITORY')}/actions/runs/{env('GITHUB_RUN_ID')}" if env("GITHUB_RUN_ID") else None
info = {
    "repository": env("GITHUB_REPOSITORY", "local"),
    "source_ref": env("GITHUB_REF"),
    "head_commit": env("ORBIT_HEAD_SHA") or checkout,
    "checkout_commit": checkout,
    "dirty": dirty == "true",
    "workflow_run": run,
    "version": version,
    "reported_version": reported,
    "release_build": env("ORBIT_RELEASE") == "1",
    "esp_idf": idf,
    "upstream_hid_remapper": upstream,
    "files": {os.path.basename(image): hashlib.sha256(open(image, "rb").read()).hexdigest()},
}
with open(out, "w") as f:
    json.dump(info, f, indent=2)
    f.write("\n")
PY
printf 'version=%s reported=%s\n' "$version" "$reported"
printf 'BUILD_INFO=%s\n' "$build/BUILD-INFO.json"
printf 'FW_BASE=%s\n' "$base"
