#!/usr/bin/env bash
# Build UAPlay against the PS5 SDL software VideoOut backend.
# This deliberately builds the Switch C UI; it never packages EVO or Nuvio.
set -euo pipefail

project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
: "${PS5_NATIVE_TEMPLATE:?Set to a checkout of leolobato/ps5-homebrew-ui with native dependencies prepared}"
: "${PS5_NATIVE_OUTPUT:?Set to an absolute output directory (new or empty)}"

for path in "$PS5_NATIVE_TEMPLATE/Makefile" \
            "$PS5_NATIVE_TEMPLATE/src/runtime/app_heap.c"; do
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
mkdir -p "$app/src/runtime" "$app/ps5"
cp -a "$app/src-base/runtime/." "$app/src/runtime/"
cp "$project"/source/*.c "$project"/source/*.h "$app/src/"
cp "$project/ps5/kitchensink_mpv.c" "$app/src/kitchensink_mpv.c"
cp "$project/ps5/posix_compat.c" "$app/src/posix_compat.c"
cp "$project/ps5/kitchensink_mpv.h" "$project/ps5/native_display.h" "$app/ps5/"
cp "$project/ps5/native_display.c" "$app/src/"
if [[ -f "$project/source/tmdb_build_key.h" ]]; then
    cp "$project/source/tmdb_build_key.h" "$app/src/tmdb_build_key.h"
fi
cp "$project/ps5/icon0.png" "$app/sce_sys/icon0.png"
# PacBrew's .pc files contain Linux-only linker flags even for their PS5
# archives. Normalize the flags in the throwaway pinned template checkout.
python3 "$project/scripts/patch-ps5-linker.py" "$app/tools/build.sh" --without-ps5-keyboard

(
    cd "$app"
    make init TITLE_ID=PPSA99283 APP_NAME='UAPlay PS5' APP_CATEGORY=media
    # The media title must use the console's page-table defaults. The template
    # supplies 256 MiB, which the kernel rejects before eboot starts on 13.60.
    python3 - sce_sys/param.json <<'PY'
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
metadata = json.loads(path.read_text(encoding="utf-8"))
metadata.pop("kernel", None)
metadata["contentVersion"] = "01.000.012"
path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PY
    make libc
    APP_DEFINITIONS='__PS5__ SDL_MAIN_HANDLED UAPLAY_PS5_SOFTWARE' \
    PACBREW_PACKAGES='sdl2 SDL2_ttf SDL2_image libcurl SDL_kitchensink libavformat libavcodec libavutil libswscale libswresample libass' \
    bash tools/build.sh Folder
)

test -s "$app/dist/PPSA99283/eboot.bin"
mkdir -p "$app/dist/PPSA99283/romfs"
cp -a "$project/romfs/." "$app/dist/PPSA99283/romfs/"
mkdir -p -- "$PS5_NATIVE_OUTPUT"
cp -a "$app/dist/PPSA99283" "$PS5_NATIVE_OUTPUT/"
printf 'Native UAPlay candidate staged at %s/PPSA99283\n' "$PS5_NATIVE_OUTPUT"
