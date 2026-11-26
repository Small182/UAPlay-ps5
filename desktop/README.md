# UAPlay PS5 Desktop Preview

This is a host preview of the **same** native PS5 UI, not a PS5 emulator. 
It uses the console's SDL software drawing path, input mapping, catalog and
libVLC adapter, with native firmware calls replaced by host implementations.
The PS5 build does not include desktop/ and its hardware decode path is unchanged.

## Ubuntu 24.04 / Windows WSL2 (WSLg)
```sh
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev libvlc-dev vlc-plugin-base vlc-plugin-video-output libswscale-dev libavutil-dev ffmpeg ca-certificates
bash desktop/build.sh
cd build/desktop-preview
./uaplay-preview
```

Run from the directory containing romfs/. Storage is isolated in preview-data/.
## Windows x64 portable
Download the **UAPlay-PS5-Desktop-Windows-x64** artifact from a successful
Windows Preview Actions run. Extract the entire archive into a folder you
can write to and double-click UAPlay.exe. Keep the DLLs, plugins and romfs
alongside the EXE. No WSL, installer, admin access or controller driver is
required. Your organization's application control policy must permit execution.
The first Windows CI run must pass before the artifact is considered tested.
For live catalog use, put your own TMDB key in preview-data/tmdb.key or configure
the UAPlay-web gateway in settings. CI deliberately does not embed a secret.

The preview creates an SDL virtual DualSense-style controller automatically.
No physical controller or system-level driver is required. Keyboard presses
and releases are converted to controller buttons and analog axes; the existing
PS5 navigation and player logic receives those events.

| Keyboard | Virtual controller |
|---|---|
| Arrow keys | D-pad |
| Enter / Space | Cross |
| Backspace / Escape | Circle |
| X / Y | Square / Triangle |
| Q / E (or Tab) | L1 / R1 |
| M | Options |
| W A S D | Left stick |
| I J K L | Right stick |
| 1 / 2 | L2 / R2 |
| C / V | L3 / R3 |
| T | Touchpad |
| F1 | Show keyboard controls in a dialog |

In playback, A/D seek ±10 seconds, W/S seek ±60 seconds, J/L control
volume and I/K control picture brightness. Arrow keys select the player
panel buttons. Search uses the actual console on-screen keyboard.
Focus loss releases all virtual buttons and axes. F1 is desktop-only.
The smoke test also checks the virtual controller mapping and release.
This tests app-level SDL input, not the PS5's real controller driver.

## CI
The separate Desktop Preview workflow compiles the app, runs the existing
portable checks, generates a short H.264/AAC test clip, renders screenshots of
the real drawing functions in light/dark themes with offline test data, and
checks the VLC first-frame, seek and pause paths. The UI tests artifact contains
PNG screenshots and a log, even when the runtime test fails. This is a
render smoke test, not a complete interactive navigation test or pixel-perfect
golden image comparison. VideoOut, PS5 hardware decoding, real DualSense
behavior on the console, and exFAT loading still require hardware.
