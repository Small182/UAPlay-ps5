# UAPlay PS5 Desktop Preview

This is a host preview of the **same** native PS5 UI, not a PS5 emulator. 
It uses the console's SDL software drawing path, input mapping, catalog and
libVLC adapter, with native firmware calls replaced by host implementations.
The PS5 build does not include desktop/ and its hardware decode path is unchanged.

## Ubuntu 24.04 / Windows WSL2 (WSLg)
```sh
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev libvlc-dev vlc-plugin-base libswscale-dev libavutil-dev ffmpeg ca-certificates
bash desktop/build.sh
cd build/desktop-preview
./uaplay-preview
```

Run from the directory containing romfs/. Storage is isolated in preview-data/.
A Windows native .exe is not supplied by this initial workflow; Windows with WSLg
can run the Linux artifact after installing the runtime libraries.
For live catalog use, put your own TMDB key in preview-data/tmdb.key or configure
the UAPlay-web gateway in settings. CI deliberately does not embed a secret.

Arrow keys move, Enter acts as Cross, Backspace as Circle, X as Square,
Y as Triangle, M/Escape as Options. SDL game controllers work when exposed to
the host; WSL controller passthrough is not guaranteed.

## CI
The separate Desktop Preview workflow compiles the app, runs the existing
portable checks, generates a short H.264/AAC test clip, renders screenshots of
the real drawing functions in light/dark themes with offline test data, and
checks the VLC first-frame, seek and pause paths. The UI tests artifact contains
PNG screenshots and a log, even when the runtime test fails. This is a
render smoke test, not a complete interactive navigation test or pixel-perfect
golden image comparison. VideoOut, PS5 hardware decoding, real DualSense
behavior on the console, and exFAT loading still require hardware.
