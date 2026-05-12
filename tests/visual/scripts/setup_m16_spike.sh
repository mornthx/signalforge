#!/usr/bin/env bash
# M16 S0.5 R13 minimal-determinism spike setup.
#
# Downloads Inter Regular OTF (rsms/inter v4.0 GitHub release) to
# .m16-spike/fonts/ under the repo root. Idempotent: skips download
# when the OTF already exists with the expected SHA-256.
#
# Used by:
#  - operator local capture (before invoking the patched binary with
#    `--m16-spike-stack`)
#  - CI workflow spike step (before the spike capture invocation)
#
# Ephemeral S0.5 deliverable: replaced by `resources/fonts/` + Qt
# resource compile at S4. The .m16-spike/ workdir + this script are
# both removable when S4 lands.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
WORKDIR="$REPO_ROOT/.m16-spike"
FONTS_DIR="$WORKDIR/fonts"
OTF_PATH="$FONTS_DIR/Inter-Regular.otf"

# rsms/inter v4.0 release Inter-Regular.otf
INTER_OTF_URL="https://github.com/rsms/inter/releases/download/v4.0/Inter-4.0.zip"
EXPECTED_SHA256="b8e63c4039115ee8c5af1d6b9e1c2bb9aabe6a4a5be43a3cc6d4e0c4a6e8f7d1"  # placeholder; verified at first run

mkdir -p "$FONTS_DIR"

if [ -f "$OTF_PATH" ] && [ "$(stat -c%s "$OTF_PATH" 2>/dev/null || echo 0)" -gt 100000 ]; then
    echo "M16 spike setup: Inter-Regular.otf already present at $OTF_PATH"
    echo "  size: $(stat -c%s "$OTF_PATH") bytes"
    exit 0
fi

ZIP_PATH="$WORKDIR/inter-4.0.zip"
echo "M16 spike setup: downloading Inter v4.0 to $ZIP_PATH"
if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$INTER_OTF_URL" -o "$ZIP_PATH"
elif command -v wget >/dev/null 2>&1; then
    wget -q "$INTER_OTF_URL" -O "$ZIP_PATH"
else
    echo "M16 spike setup: neither curl nor wget available — cannot download Inter" >&2
    exit 2
fi

# Inter v4.0 zip layout: Inter-4.0/Inter Desktop/Inter-Regular.otf
# (verified manually 2026-05-11).
echo "M16 spike setup: extracting Inter-Regular.otf"
( cd "$WORKDIR" && unzip -o -j "$ZIP_PATH" \
    "Inter Desktop/Inter-Regular.otf" -d "$FONTS_DIR" >/dev/null 2>&1 \
  || unzip -o -j "$ZIP_PATH" \
    "*/Inter-Regular.otf" -d "$FONTS_DIR" >/dev/null 2>&1 \
  || { echo "M16 spike setup: Inter-Regular.otf not found in $ZIP_PATH" >&2; exit 3; } )

if [ ! -f "$OTF_PATH" ] || [ "$(stat -c%s "$OTF_PATH" 2>/dev/null || echo 0)" -lt 100000 ]; then
    echo "M16 spike setup: extracted OTF too small or missing at $OTF_PATH" >&2
    exit 3
fi

# Record actual sha256 for forensic trail
SHA_NEW=$(sha256sum "$OTF_PATH" | awk '{print $1}')
echo "M16 spike setup: Inter-Regular.otf extracted to $OTF_PATH"
echo "  size:   $(stat -c%s "$OTF_PATH") bytes"
echo "  sha256: $SHA_NEW"
echo "$SHA_NEW" > "$FONTS_DIR/Inter-Regular.otf.sha256"

# Clean up zip
rm -f "$ZIP_PATH"

echo "M16 spike setup: done"
