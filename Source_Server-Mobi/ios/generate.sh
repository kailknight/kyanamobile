#!/bin/bash
# Generates ios/build/WorldOfKiraIOS.xcodeproj and opens it in Xcode.
# Usage: ./generate.sh [TEAM_ID]
#   TEAM_ID: Xcode > Settings > Accounts > your Apple ID > Team (10 characters).
#            Remembered after the first run.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CMAKE="$(command -v cmake || echo /opt/homebrew/bin/cmake)"

# Homebrew's installer switches the active developer directory to the
# standalone Command Line Tools, which have no iOS SDK. Use Xcode's regardless.
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

TEAM_ARG=()
if [ $# -ge 1 ]; then
    TEAM_ARG=(-DMU_IOS_TEAM_ID="$1")
fi

# Shared sokol (5.Main/ThirdParty/sokol) when present, else the stock stand-in
# fetch_deps.sh downloads. Passed every run so the choice follows the files.
SOKOL="$HERE/../5.Main/ThirdParty/sokol"
if [ ! -f "$SOKOL/sokol_app.h" ]; then
    SOKOL="$HERE/third_party/sokol-stock"
    echo "[generate] shared sokol missing - building with stock sokol from $SOKOL"
fi

"$CMAKE" -S "$HERE" -B "$HERE/build" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DSOKOL_ROOT="$SOKOL" \
    "${TEAM_ARG[@]+"${TEAM_ARG[@]}"}"

open "$HERE/build/WorldOfKiraIOS.xcodeproj"
