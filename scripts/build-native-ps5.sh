#!/usr/bin/env bash
# Native UAPlay SDK integration. Run only against an already-built GL+SDL SDK.
# This deliberately builds the Switch C UI; it never packages EVO or Nuvio.
set -euo pipefail

project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
: "${PS5_NATIVE_TEMPLATE:?Set to a checkout of leolobato/ps5-homebrew-ui with native dependencies prepared}"
: "${PS5_OPENGL_PREFIX:?Set to the verified ps5-opengl SDK prefix}"
: "${PS5_SDL_PREFIX:?Set to the SDL2 GL-bridge SDK prefix}"
: "${PS5_NATIVE_OUTPUT:?Set to an absolute output directory (new or empty)}"

for path in "$PS5_NATIVE_TEMPLATE/Makefile" \
            "$PS5_NATIVE_TEMPLATE/src/runtime/app_heap.c" \
            "$PS5_OPENGL_PREFIX/manifest.sha256" \
            "$PS5_SDL_PREFIX/lib/libSDL2.a"; do
    if [[ ! -f "$path" ]]; then
        printf 'Missing native prerequisite: %s\n' "$path" >&2
        exit 2
    fi
done
if [[ $PS5_NATIVE_OUTPUT != /* || -e $PS5_NATIVE_OUTPUT ]]; then
    echo 'PS5_NATIVE_OUTPUT must be an absolute, not-yet-existing directory' >&2
    exit 2
fi

stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
app="$stage/title"
mkdir -p "$app"
rsync -a --exclude='/.git/' --exclude='/.deps/' --exclude='/build/' \
    --exclude='/dist/' "$PS5_NATIVE_TEMPLATE/" "$app/"
ln -s "$PS5_NATIVE_TEMPLATE/.deps" "$app/.deps"
mv "$app/src" "$app/src-base"
mkdir -p "$app/src/runtime" "$app/ps5" "$app/.local/sdk"
cp -a "$app/src-base/runtime/." "$app/src/runtime/"
cp "$project"/source/*.c "$project"/source/*.h "$app/src/"
cp "$project/ps5/kitchensink_mpv.c" "$app/src/kitchensink_mpv.c"
cp "$project/ps5/kitchensink_mpv.h" "$app/ps5/"
if [[ -f "$project/source/tmdb_build_key.h" ]]; then
    cp "$project/source/tmdb_build_key.h" "$app/src/tmdb_build_key.h"
fi
ln -s "$PS5_SDL_PREFIX/lib/libSDL2.a" "$app/.local/sdk/libSDL2.a"
cp "$project/ps5/icon0.png" "$app/sce_sys/icon0.png"

(
    cd "$app"
    make init TITLE_ID=PPSA99282 APP_NAME='UAPlay PS5' APP_CATEGORY=media
    # Bridge SDL2 archive must resolve SDL_GL_* before PacBrew's older SDL2.
    # PacBrew supplies FFmpeg, SDL_image/ttf, curl and SDL_kitchensink.
    PS5_OPENGL_PREFIX="$PS5_OPENGL_PREFIX" \
    APP_DEFINITIONS='__PS5__ SDL_MAIN_HANDLED' \
    APP_STATIC_ARCHIVES='.local/sdk/libSDL2.a' \
    PACBREW_PACKAGES='SDL2_ttf SDL2_image libcurl SDL2_kitchensink' \
    make app
)

test -s "$app/dist/PPSA99282/eboot.bin"
mkdir -p "$app/dist/PPSA99282/romfs"
cp -a "$project/romfs/." "$app/dist/PPSA99282/romfs/"
mkdir -p -- "$PS5_NATIVE_OUTPUT"
cp -a "$app/dist/PPSA99282" "$PS5_NATIVE_OUTPUT/"
printf 'Native UAPlay candidate staged at %s/PPSA99282\n' "$PS5_NATIVE_OUTPUT"
