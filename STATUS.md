# Native build status

`native-sdl` branch: real UAPlay UI sources, PlayStation controller hints,
OpenGL Core 3.3 shader migration and SDL_kitchensink/FFmpeg playback adapter
are implemented as **source changes**.

Portable catalog regression tests passed. The PS5 cross-link, native ELF
conversion, title packaging, and GitHub Actions artifact upload **succeeded**
in [run 42918448050](https://github.com/Small182/UAPlay-ps5/actions/runs/42918448050).
The archive includes `PPSA99282/eboot.bin` and the original UAPlay assets.
Playback and input on firmware 13.60 have **not** been confirmed. This is a
buildable experimental candidate, **not a validated stable release**.

The new CI packaging step attempts an additional `PPSA99282.ffpkg` UFS2 image
for ShadowMountPlus. A successful UFS2 checksum and superblock check confirms
the image format, not on-console launch compatibility.

The `evo-host`/Nuvio experiment is abandoned for this port. A successful
`ps5-bootstrap.yml` or portable-test workflow is not evidence of a working
UAPlay PS5 application. See [PORTING.md](PORTING.md).
