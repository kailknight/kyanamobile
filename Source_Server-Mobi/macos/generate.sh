#!/bin/bash
# Generates macos/build/WorldOfKiraMac.xcodeproj. Run ios/fetch_deps.sh once
# first (SDL2 and SDL2_ttf are shared with the iOS build).
# Usage: ./generate.sh   then:  ./build.sh   (or open the project in Xcode)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CMAKE="$(command -v cmake || echo /opt/homebrew/bin/cmake)"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

"$CMAKE" -S "$HERE" -B "$HERE/build" -G Xcode \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DSOKOL_ROOT="$HERE/../5.Main/ThirdParty/sokol" \
    "$@"
