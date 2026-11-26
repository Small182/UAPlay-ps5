#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$MSYSTEM" == UCRT64 ]] || { echo "Run in MSYS2 UCRT64"; exit 1; }
out="${1:-build/windows-preview}"
mkdir -p "$out"
out="$(realpath "$out")"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
# Official portable VideoLAN VLC package with matching C SDK.
version=3.0.23
archive="vlc-$version-win64.zip"
url="https://download.videolan.org/pub/videolan/vlc/$version/win64/$archive"
curl -fL --retry 3 "$url" -o "$work/$archive"
curl -fL --retry 3 "$url.sha256" -o "$work/checksum"
expected="$(awk '{print $1;exit}' "$work/checksum")"
[[ "$expected" =~ ^[a-fA-F0-9]{64}$ ]]
(cd "$work" && printf '%s  %s\n' "$expected" "$archive" | sha256sum -c -)
unzip -q "$work/$archive" -d "$work"
vlc="$(find "$work" -type f -name libvlc.dll -print -quit)"
vlc="$(dirname "$vlc")"
test -f "$vlc/sdk/include/vlc/vlc.h"
test -f "$vlc/libvlccore.dll"
cp "$vlc/"*.dll "$out/"
cp -a "$vlc/plugins" "$out/"
# Preserve upstream licensing files for the bundled VLC distribution.
mkdir -p "$out/licenses/vlc"
for f in COPYING COPYING.LIB AUTHORS THANKS README; do
    [[ ! -f "$vlc/$f" ]] || cp "$vlc/$f" "$out/licenses/vlc/"
done
packages=(sdl2 SDL2_ttf SDL2_image libcurl libswscale libavutil)
read -r -a cflags <<< "$(pkg-config --cflags "${packages[@]}")"
read -r -a libs <<< "$(pkg-config --libs "${packages[@]}")"
sources=()
for f in source/*.c; do
    [[ "$f" == source/console_curl.c ]] || sources+=("$f")
done
gcc -std=gnu11 -O2 -g -Wall -Wextra -D__PS5__ -DUAPLAY_PS5_SOFTWARE \
    -DUAPLAY_USE_VLC_COMPAT -DUAPLAY_DESKTOP_PREVIEW -DSDL_MAIN_HANDLED \
    -include desktop/win_compat.h -Isource -I"$vlc/sdk/include" "${cflags[@]}" \
    "${sources[@]}" ps5/libvlc_mpv.c desktop/host_platform.c desktop/virtual_pad.c \
    -o "$out/UAPlay.exe" "${libs[@]}" "$vlc/libvlc.dll" -lm -pthread
cp -a romfs "$out/"
ca=""
for p in /ucrt64/etc/ssl/certs/ca-bundle.crt /ucrt64/etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-bundle.crt /etc/ssl/certs/ca-certificates.crt; do
    if [[ -s "$p" ]]; then ca="$p"; break; fi
done
test -n "$ca"
cp "$ca" "$out/romfs/cacert.pem"
# Collect all transitive PE imports from the host prefix. System DLLs stay in
# Windows; VLC's bundled runtime and plugin directory take precedence.
while :; do
    added=0
    while IFS= read -r name; do
        [[ -n "$name" ]] || continue
        [[ -f "$out/$name" ]] && continue
        path="$(find /ucrt64/bin -maxdepth 1 -iname "$name" -print -quit)"
        if [[ -n "$path" ]]; then
            cp "$path" "$out/$name"
            added=1
        fi
    done < <(find "$out" -type f \( -iname '*.dll' -o -iname '*.exe' \) -print0 | xargs -0 objdump -p 2>/dev/null | awk '/DLL Name:/{print $3}' | sort -fu)
    [[ "$added" == 1 ]] || break
done
# Include license texts for the MSYS2 packages used in the executable/runtime.
if [[ -d /ucrt64/share/licenses ]]; then cp -a /ucrt64/share/licenses "$out/licenses/msys2"; fi
cp desktop/README.md "$out/README.txt"
if [[ -f LICENSE ]]; then cp LICENSE "$out/"; fi
if [[ -f COPYING ]]; then cp COPYING "$out/"; fi
echo "Portable Windows bundle: $out/UAPlay.exe"
