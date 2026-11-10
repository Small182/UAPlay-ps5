# Native build status

`native-sdl` branch: real UAPlay UI sources, PlayStation controller hints,
OpenGL Core 3.3 shader migration and SDL_kitchensink/FFmpeg playback adapter
are implemented as **source changes**.

Portable catalog regression tests run locally. The PS5 cross-link and
firmware-13.60 hardware playback have **not** been confirmed. This is an
experimental native candidate, **not a ready-to-install release**.

The `evo-host`/Nuvio experiment is abandoned for this port. A successful
`ps5-bootstrap.yml` or portable-test workflow is not evidence of a working
UAPlay PS5 application. See [PORTING.md](PORTING.md).
