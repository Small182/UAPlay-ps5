# UAPlay-PS5

Experimental **native PlayStation 5 homebrew port** of [UAPlay-nx](https://github.com/Small182/UAPlay-nx).

> **Work in progress — native PS5 source port, not yet verified on hardware.**

The goal is to retain the Switch app's catalogue, details, favorites, streaming-source discovery and visual layout, changing only the controller hints and platform-specific runtime.

## Controller legend

| Switch UI | PlayStation UI |
| --- | --- |
| A | ✕ (Cross) |
| B | ◯ (Circle) |
| X | □ (Square) |
| Y | △ (Triangle) |
| L / R | L1 / R1 |
| − | OPTIONS |

## Native port groundwork on `native-sdl`

- Imported C sources, bundled assets, metadata snapshots, fonts and tests.
- Started `__PS5__` guards and PS5 storage paths (`/download0/uaplay`).
- Initial PlayStation button hint mapping.
- OpenGL Core-profile shader port of the actual UAPlay renderer.
- SDL_kitchensink/FFmpeg playback adapter using native OpenGL + SDL audio;
  no browser, EVO or Nuvio frontend.
- A pinned-source GitHub Actions native cross-build, with portable C
  regression tests. The complete executable successfully compiled in
  [run 42918448050](https://github.com/Small182/UAPlay-ps5/actions/runs/42918448050).

## What still needs verification

- Actual loading of the successfully cross-linked native executable on PS5.
- Actual video/audio and HLS playback on firmware 13.60.
- Full subtitles, codec controls and provider-specific HTTP header handling.
- PS5 on-screen keyboard (search entry is not yet functional).
- Real console validation.

See [PORTING.md](PORTING.md) for engineering tasks and [STATUS.md](STATUS.md) for build status.

## CI

[GitHub Actions](https://github.com/Small182/UAPlay-ps5/actions) has portable
tests and an experimental native `native-sdl.yml` build. The successful
`native-sdl` run uploads **UAPlay-PS5-native-UNTESTED**, a ZIP with
`UAPlay-PS5-native-candidate.tar.gz` inside. Extract both archives to obtain
the `PPSA99282` native title folder (`eboot.bin`, `sce_sys/`, `sce_module/`,
`romfs/` and bundled assets). This is **not a Sony retail PKG**, and it must
be launched using a compatible homebrew title/folder loader on a PS5 with
the necessary homebrew access. See [PORTING.md](PORTING.md).

The workflow successfully produces **UAPlay-PS5-FFPKG-UNTESTED** containing
`PPSA99282.ffpkg` (UFS2, for ShadowMountPlus). This PS5 image is **not a
PS4-compatible `.pkg`**. Place the image directly in a ShadowMountPlus scan
location such as `/data/homebrew/`; the image itself contains `sce_sys/` at
its root. [Native packaging run 43070340195](https://github.com/Small182/UAPlay-ps5/actions/runs/43070340195)
passed. PS5 hardware validation is still required. If it fails to mount or
launch, inspect `/data/shadowmount/debug.log` before changing launch methods.

## Upstream

The [Switch version](https://github.com/Small182/UAPlay-nx) remains the functional source of the application. Do not replace its NRO with files from this port.
