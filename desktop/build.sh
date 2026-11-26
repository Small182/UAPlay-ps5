#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-build/desktop-preview}"
mkdir -p "$out"
# Compile the original console UI and VLC bridge; only firmware calls are adapted.
# __PS5__ selects the shared PS5 layout, not a PS5 ABI on this host.
packages=(sdl2 SDL2_ttf SDL2_image libcurl libvlc libswscale libavutil)
read -r -a cflags <<< "$(pkg-config --cflags "${packages[@]}")"
read -r -a libs <<< "$(pkg-config --libs "${packages[@]}")"
sources=()
for f in source/*.c; do
    [[ "$f" == source/console_curl.c ]] || sources+=("$f")
done
cc -std=gnu11 -O2 -g -Wall -Wextra -D__PS5__ -DUAPLAY_PS5_SOFTWARE \
    -DUAPLAY_USE_VLC_COMPAT -DUAPLAY_DESKTOP_PREVIEW -DSDL_MAIN_HANDLED \
    -Isource "${cflags[@]}" "${sources[@]}" ps5/libvlc_mpv.c desktop/host_platform.c \
    -o "$out/uaplay-preview" "${libs[@]}" -lm -pthread
cp -a romfs "$out/"
echo "Preview built: $out/uaplay-preview"
