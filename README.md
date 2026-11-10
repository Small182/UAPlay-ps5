# UAPlay-PS5

Experimental **native PlayStation 5 homebrew port** of [UAPlay-nx](https://github.com/Small182/UAPlay-nx).

> **Work in progress — not yet an installable or playable PS5 app.**

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

## What is ready

- Imported C sources, bundled assets, metadata snapshots, fonts and tests.
- Started `__PS5__` guards and PS5 storage paths (`/data/uaplay`).
- Initial PlayStation button hint mapping.
- GitHub Actions workflow for portable C regression tests.

## What is not ready

- PS5 native build and installable title folder.
- PS5-targeted libmpv/FFmpeg/video and audio integration.
- Native application startup/heap integration and SDK packaging.
- PS5 on-screen keyboard (search entry is not yet functional).
- Real console validation.

See [PORTING.md](PORTING.md) for engineering tasks and [STATUS.md](STATUS.md) for build status.

## CI

[GitHub Actions](https://github.com/Small182/UAPlay-ps5/actions) runs `bash scripts/test-portable.sh` on Ubuntu. A passing check confirms portable catalogue logic only, **not** a PS5 binary build.

## Upstream

The [Switch version](https://github.com/Small182/UAPlay-nx) remains the functional source of the application. Do not replace its NRO with files from this port.
