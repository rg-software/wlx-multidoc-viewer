#!/usr/bin/env bash
# ======================================================================
#  BuildMakeSetup.sh - Linux x64 release build + package
#
#  Produces:
#    build/linux-release/MultidocViewer.wlx64   (built plugin)
#    dist/linux-release/MultidocViewer.wlx64    (packaged)
#    dist/wlx-multidoc-viewer-Linux-YYYYMMDD.zip     (bundle)
#
#  Run from a plain shell at the project root. Uses the system Qt6 /
#  MuPDF / DjVuLibre / libchm / libarchive packages from the distro.
# ======================================================================

set -euo pipefail

cd "$(dirname "$0")"

# --- 1. Configure + build the x64 release -----------------------------
cmake --preset linux-release
cmake --build --preset linux-release

# --- 2. Package into dist/linux-release (outside the build tree) ------
OUT="dist/linux-release"
rm -rf "$OUT"
mkdir -p "$OUT"

cp "build/linux-release/MultidocViewer.wlx64" "$OUT/" || {
    echo "[ERROR] plugin .wlx64 not found" >&2
    exit 1
}

cp pluginst.inf "$OUT/"

# --- 3. Compress the package (zip goes outside the packaged dir; remove
#        any previous archive of this date stamp first) ----------------
STAMP="$(date +%Y%m%d)"
ZIP="dist/wlx-multidoc-viewer-Linux-${STAMP}.zip"
rm -f "$ZIP"
(cd "$OUT" && zip -qr "../$(basename "$ZIP")" .)

echo
echo "SUCCESS"
echo "  plugin: $OUT/MultidocViewer.wlx64"
echo "  bundle: $ZIP"
