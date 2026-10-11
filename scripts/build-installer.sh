#!/usr/bin/env bash
# Rebuilds the Windows installer (EXE) from an already built PRYSM.vst3 bundle.
# Usage: scripts/build-installer.sh <path/to/PRYSM.vst3> [output-dir] [version]
# Requires: makensis (NSIS 3.x).  The bundle MUST be the Windows (x86_64-win) build.
set -euo pipefail
VST3_DIR="$(cd "${1:?usage: build-installer.sh <PRYSM.vst3 dir> [out dir] [version]}" && pwd)"
OUT_DIR="$(mkdir -p "${2:-dist}" && cd "${2:-dist}" && pwd)"
VERSION="${3:-1.1.0}"
cd "$(dirname "$0")/.."
command -v makensis >/dev/null || { echo "makensis not found (install NSIS 3)"; exit 1; }
[ -f "$VST3_DIR/Contents/x86_64-win/PRYSM.vst3" ] || { echo "Not a Windows bundle: $VST3_DIR/Contents/x86_64-win/PRYSM.vst3 is missing"; exit 1; }
makensis -V2 -DVERSION="$VERSION" -DVST3_DIR="$VST3_DIR" -DOUT_DIR="$OUT_DIR" installer/prysm.nsi
ls -l "$OUT_DIR"/PRYSM-"$VERSION"-Windows-x64-Setup.exe
