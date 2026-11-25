#!/usr/bin/env bash
# Experimental static libVLC cross-build from the pinned PS5 upstream port.
set -euo pipefail
: "${PS5_NATIVE_TEMPLATE:?}"
export PS5_PAYLOAD_SDK="${PS5_NATIVE_TEMPLATE}/.deps/native/ps5-payload-sdk"
export VLC_PS5_WORK="${VLC_PS5_WORK:-/tmp/uaplay-vlc-work}"
export TARGET=ps5
upstream="${UAPLAY_VLC_UPSTREAM:-/tmp/uaplay-vlc-upstream}"
rev=7f104abb5a88dd675b60261769dc86d5dfb0a0d4
git clone --quiet https://github.com/ZiZc3/VLC.git "$upstream"
git -C "$upstream" checkout --detach "$rev"
mkdir -p "$VLC_PS5_WORK/src"
curl --fail --location --retry 3 https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz -o "$VLC_PS5_WORK/vlc.tar.xz"
tar -xf "$VLC_PS5_WORK/vlc.tar.xz" -C "$VLC_PS5_WORK/src"
bash "$upstream/ps5/deps/build-deps.sh"
bash "$upstream/ps5/configure-vlc.sh"
NM="$PS5_PAYLOAD_SDK/bin/prospero-nm" APP_MODULES="vlc_entry__ps5http" bash "$upstream/ps5/gen-static-modules.sh"
test -s "$VLC_PS5_WORK/static-ps5/static_modules.c"
test -s "$VLC_PS5_WORK/build-vlc-ps5/lib/.libs/libvlc.a"
