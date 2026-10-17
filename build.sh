#!/usr/bin/env bash
# Build wrapper for M5Tab-Poco (Git Bash / POSIX version of build.cmd).
# Works around esp32:esp32 3.3.8 missing BOARD_SDMMC_POWER_CHANNEL in the
# m5stack_tab5 variant header.
# IMPORTANT: inject via compiler.*.extra_flags (appended) instead of
# build.extra_flags (replaced) so board defaults like -DBOARD_HAS_PSRAM
# and -DARDUINO_USB_MODE=1 are preserved.
#   ./build.sh                  -> compile, then list the COM ports (comports.ps1)
#   ./build.sh --upload -p COM4 -> compile + upload
set -e
SKETCH_DIR="$(cd "$(dirname "$0")" && pwd)"
# arduino-cli.exe on Windows wants a native path, not /d/...
if command -v cygpath >/dev/null 2>&1; then
  SKETCH_DIR="$(cygpath -m "$SKETCH_DIR")"
fi
EXTRA="-DBOARD_SDMMC_POWER_CHANNEL=4"
COMPILE=(arduino-cli compile
  --fqbn esp32:esp32:m5stack_tab5
  --build-property "compiler.c.extra_flags=$EXTRA"
  --build-property "compiler.cpp.extra_flags=$EXTRA"
  --build-property "compiler.S.extra_flags=$EXTRA")
if [ $# -gt 0 ]; then
  exec "${COMPILE[@]}" "$@" "$SKETCH_DIR"
fi
rc=0
"${COMPILE[@]}" "$SKETCH_DIR" || rc=$?
if command -v powershell.exe >/dev/null 2>&1; then
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$SKETCH_DIR/comports.ps1" || true
  echo "  upload: ./build.sh --upload -p COMx"
fi
exit $rc
