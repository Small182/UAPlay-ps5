# UAPlay PS5 — native SDL/OpenGL port

Development branch: `native-sdl`. This is **the original UAPlay C interface**,
not the EVO/Nuvio HTML application. The old experimental `evo-host` branch is
separate and is not imported into this build.

## Architecture

| Feature | Implementation in this branch |
| --- | --- |
| Film/series catalog and search | Original `source/catalog.c`, TMDb API v3 |
| Details, cards, navigation and UI layout | Original `source/main.c` (SDL2 + custom OpenGL painter) |
| Posters | Original `source/posters.c` (SDL_image and curl) |
| Favorites/watch history | Original `source/library.c` in `/download0/uaplay` (PS5 sandbox) |
| Controller labels | PlayStation ✕ ◯ □ △ L1 R1 OPTIONS |
| Graphics | OpenGL 3.3 Core shaders + VAO, PS5 SDL2/OpenGL bridge |
| HLS/HTTP video decoding | SDL_kitchensink / FFmpeg, following BFplayer's playback approach |
| Video presentation | Native OpenGL RGBA texture, letterboxed to UI viewport |
| Audio | SDL2 queued signed 16-bit PCM output |
| Player screens and controls | Original UI; thin compatibility adapter `ps5/kitchensink_mpv.c` |
| Packaged files | Native PS5 `PPSA99282` title folder, full `romfs/` + UAPlay icon |

The compatibility adapter maps a narrow subset of UAPlay's libmpv calls to
SDL_kitchensink. **It does not bundle libmpv and is not a complete libmpv
replacement**. Networking with `Referer`/`User-Agent` restrictions, external
subtitles, speed and HLS quality switching are not implemented by the adapter.
They must not be advertised as functional until verified or implemented.

## Building

The [`native-sdl.yml`](.github/workflows/native-sdl.yml) workflow:

1. Runs the portable catalog tests.
2. Uses a pinned public PS5 homebrew title template.
3. Downloads the public PS5 OpenGL SDK 1.0.2 from the `darkxex/ps5-opengl`
   preservation release, checking the exact published SHA-256 digest.
4. Builds a real SDL2/OpenGL PS5 backend with the SDK's integration scripts.
5. Resolves PacBrew `SDL2_ttf`, `SDL2_image`, `libcurl`, `SDL_kitchensink`
   and FFmpeg/libass static dependencies,
   ports, compiles the original UAPlay C files and its native decoder adapter.
6. Packages `PPSA99282/eboot.bin`, native runtime modules, fonts and metadata.

SDL bridge and library compilation can take considerable time. The workflow's output is
named `UAPlay-PS5-native-UNTESTED` on purpose: a successful link does not imply
that the video renderer, input and codecs work on firmware 13.60.

To build using prebuilt *verified* SDK prefixes on Linux/WSL, set these four
variables and run `bash scripts/build-native-ps5.sh`:

```sh
export PS5_NATIVE_TEMPLATE=/absolute/path/to/ps5-homebrew-ui
export PS5_OPENGL_PREFIX=/absolute/path/to/ps5-opengl-gl46
export PS5_SDL_PREFIX=/absolute/path/to/sdl-gl-bridge-sdk
export PS5_NATIVE_OUTPUT=/absolute/path/to/new-output-folder
bash scripts/build-native-ps5.sh
```

`PS5_NATIVE_TEMPLATE` must already have its verified public SDK and PacBrew
dependencies prepared; the output directory must not exist. The script stages
into a unique temporary directory and does not modify the original Switch repo
or template files. The TMDb API v3 key comes from the `TMDB_API_KEY` GitHub
Actions secret, generated as an untracked header in CI.

## Unresolved verification points

- The PS5 SDL2 GL bridge and SDL_kitchensink ports must link together without
  pulling the older SDL2 video backend from PacBrew.
- Check the `SDL_PIXELFORMAT_RGBA32` → OpenGL RGBA upload path, orientation and
  display timing on the actual console.
- Test FFmpeg network streams (HTTP/HLS/HTTPS), audio synchronization, seek,
  Unicode subtitles and provider-specific headers.
- Connect the native PS5 text-entry dialog. Catalog browsing currently works
  without it, but text search is not usable from the controller.
- Improve asynchronous stream opening, because a slow provider can block the
  UI thread temporarily.
- Test on the user's firmware 13.60 before creating a production Release.

The previous native **bootstrap** title (`ps5/bootstrap_main.cpp`) is a separate
graphics/title smoke test and does not contain the UAPlay interface. Do not
confuse a bootstrap artifact or an `evo-host` artifact with this native port.

## External dependencies and licenses

SDL2 (zlib), SDL_kitchensink (zlib), FFmpeg (LGPL/GPL depending on configuration),
and the public PS5 OpenGL SDK are compiled or linked as external dependencies.
The native wrapper is GPL-3.0-or-later. Source references:

- <https://github.com/Small182/UAPlay-nx>
- <https://github.com/ItsBlurf/BFplayer>
- <https://github.com/katajakasa/SDL_kitchensink>
- <https://github.com/darkxex/ps5-opengl>
- <https://github.com/leolobato/ps5-homebrew-ui>
