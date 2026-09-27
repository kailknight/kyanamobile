#!/bin/bash
# Fetches the third-party sources the iOS build compiles from scratch.
# Android links these as prebuilt .so files (android/app/src/main/jniLibs);
# iOS builds them as static libraries through ios/CMakeLists.txt instead.
#
# Versions are pinned to match what Android ships: SDL2 2.28.5 is the version
# of the vendored headers in android/app/src/main/cpp/SDL2/include. Android's
# SDL_ttf.h is a hand-written stub, so SDL2_ttf takes the last 2.x release.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="$HERE/third_party"
mkdir -p "$DEST"

fetch() {
    local name="$1" url="$2" tag="$3"
    if [ -d "$DEST/$name/.git" ]; then
        echo "[fetch_deps] $name already present, skipping"
        return
    fi
    echo "[fetch_deps] cloning $name @ $tag"
    git clone --depth 1 --branch "$tag" --recurse-submodules --shallow-submodules "$url" "$DEST/$name"
}

fetch SDL2     https://github.com/libsdl-org/SDL.git     release-2.28.5
fetch SDL2_ttf https://github.com/libsdl-org/SDL_ttf.git release-2.22.0

# The shared sokol lives in 5.Main/ThirdParty/sokol and is copied by hand from
# the Windows PC (D:/firstproject/sokol-master): the Android build uses a
# locally patched copy and both platforms must share it. Until it is there, a
# stock upstream checkout here stands in for the iOS build only - generate.sh
# picks whichever exists, preferring the shared one.
SOKOL="$HERE/../5.Main/ThirdParty/sokol"
if [ ! -f "$SOKOL/sokol_app.h" ]; then
    echo "[fetch_deps] shared sokol not found at $SOKOL"
    echo "[fetch_deps] using stock upstream sokol for iOS until it is copied there"
    if [ ! -d "$DEST/sokol-stock/.git" ]; then
        git clone --depth 1 https://github.com/floooh/sokol.git "$DEST/sokol-stock"
    fi
fi

echo "[fetch_deps] done"
