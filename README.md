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
- Started `__PS5__` guards and PS5 storage paths (`/data/uaplay`).
- Initial PlayStation button hint mapping.
- OpenGL Core-profile shader port of the actual UAPlay renderer.
- SDL_kitchensink/FFmpeg playback adapter using native OpenGL + SDL audio;
  no browser, EVO or Nuvio frontend.
- A pinned-source GitHub Actions native cross-build experiment, and portable C
  regression tests.

## What still needs verification

- Successful native cross-link with the complete PS5 SDL/OpenGL/FFmpeg stack.
- Actual video/audio and HLS playback on firmware 13.60.
- Full subtitles, codec controls and provider-specific HTTP header handling.
- PS5 on-screen keyboard (search entry is not yet functional).
- Real console validation.

See [PORTING.md](PORTING.md) for engineering tasks and [STATUS.md](STATUS.md) for build status.

## CI

[GitHub Actions](https://github.com/Small182/UAPlay-ps5/actions) has portable
tests and an experimental native `native-sdl.yml` build. Only a successful
native cross-build will upload `UAPlay-PS5-native-UNTESTED`, and that still
requires console validation. See [PORTING.md](PORTING.md).

## Upstream

The [Switch version](https://github.com/Small182/UAPlay-nx) remains the functional source of the application. Do not replace its NRO with files from this port.
