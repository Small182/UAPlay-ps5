#!/usr/bin/env bash
set -eEuo pipefail
trap 'echo "Windows build failed at line $LINENO: $BASH_COMMAND" >&2' ERR
cd "$(dirname "$0")/.."
[[ "$MSYSTEM" == UCRT64 ]] || { echo "Run in MSYS2 UCRT64"; exit 1; }
out="${1:-build/windows-preview}"
mkdir -p "$out"
out="$(realpath "$out")"
# All runtime libraries must come from one UCRT64 package set. Mixing the
# official VLC ZIP (different CRT/toolchain) with MSYS2 is no longer allowed.
common=(sdl2 SDL2_ttf SDL2_image libcurl)
media=(libvlc libswscale libavutil)
read -r -a cflags <<< "$(pkg-config --cflags "${common[@]}")"
read -r -a libs <<< "$(pkg-config --libs "${common[@]}")"
read -r -a mcflags <<< "$(pkg-config --cflags "${media[@]}")"
read -r -a mlibs <<< "$(pkg-config --libs "${media[@]}")"
flags=(-std=gnu11 -O2 -g -Wall -Wextra -D__PS5__ -DUAPLAY_PS5_SOFTWARE
       -DUAPLAY_USE_VLC_COMPAT -DUAPLAY_DESKTOP_PREVIEW -DSDL_MAIN_HANDLED
       -include desktop/win_compat.h -Isource)
sources=()
for f in source/*.c; do
    [[ "$f" == source/console_curl.c ]] || sources+=("$f")
done
echo "Building standalone UI executable"
gcc "${flags[@]}" "${cflags[@]}" "${sources[@]}" desktop/host_platform.c \
    desktop/virtual_pad.c desktop/windows_player.c -o "$out/UAPlay.exe" "${libs[@]}" -lm -pthread
echo "Building on-demand shared libVLC player"
gcc "${flags[@]}" "${cflags[@]}" "${mcflags[@]}" -shared \
    ps5/libvlc_mpv.c desktop/host_platform.c desktop/windows_player_module.c desktop/windows_player.def \
    -o "$out/uaplay-player.dll" "${libs[@]}" "${mlibs[@]}" -lm -pthread
test -f /ucrt64/bin/libvlc.dll
test -d /ucrt64/lib/vlc/plugins
cp /ucrt64/bin/libvlc.dll /ucrt64/bin/libvlccore.dll "$out/"
cp -a /ucrt64/lib/vlc/plugins "$out/"
# This is an embedded player, not VLC's Qt interface. Avoid pulling in
# unrelated UI/visualization modules and their large Qt dependency trees.
rm -rf "$out/plugins/gui" "$out/plugins/visualization"
rm -f "$out/plugins/plugins.dat"
if [[ -d /ucrt64/lib/vlc/lua ]]; then cp -a /ucrt64/lib/vlc/lua "$out/"; fi
cp -a romfs "$out/"
ca=""
for p in /ucrt64/etc/ssl/certs/ca-bundle.crt /ucrt64/etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-bundle.crt /etc/ssl/certs/ca-certificates.crt; do
    if [[ -s "$p" ]]; then ca="$p"; break; fi
done
test -n "$ca"
cp "$ca" "$out/romfs/cacert.pem"
# The Python PE auditor collects imports recursively for both binaries,
# every shipped plugin and optional SDL_image codecs in one pass.
for pattern in 'libjpeg-*.dll' 'libpng*.dll' 'libwebp*.dll' 'libtiff-*.dll' 'libavif*.dll' 'libjxl*.dll'; do
    while IFS= read -r -d '' dll; do
        [[ -f "$out/${dll##*/}" ]] || cp "$dll" "$out/"
    done < <(find /ucrt64/bin -maxdepth 1 -iname "$pattern" -print0)
done
mkdir -p "$out/licenses"
if [[ -d /ucrt64/share/licenses ]]; then cp -a /ucrt64/share/licenses "$out/licenses/msys2"; fi
cp desktop/README.md "$out/README.txt"
if [[ -f LICENSE ]]; then cp LICENSE "$out/"; fi
if [[ -f COPYING ]]; then cp COPYING "$out/"; fi
cygpath -am /ucrt64/bin > "$(dirname "$out")/windows-host-bin.txt"
echo "Portable Windows bundle: $out/UAPlay.exe"
