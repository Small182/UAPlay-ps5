# UAPlay native PS5 port status — 2026-11-23

## What is implemented

- Native C catalogue/UI imported from UAPlay-nx, PlayStation button labels and PS5 storage paths.
- Native PS5 VideoOut software renderer and SDL_kitchensink/FFmpeg playback adapter.
- Cross-compilation to a PS5 title folder containing `eboot.bin`, `sce_sys/`, `sce_module/`, and `romfs/`.
- A UFS2 FFPKG image builder and a new **64 KiB-cluster exFAT image** builder for ShadowMountPlus.
- Exact readback comparison of every packaged file and exFAT filesystem boot-sector check.
- [Successful packaging smoke test 45270765450](https://github.com/Small182/UAPlay-ps5/actions/runs/45270765450).

## Build evidence

The native PS5 VideoOut pipeline previously succeeded on the `native-ps5` branch in [run 43094228045](https://github.com/Small182/UAPlay-ps5/actions/runs/43094228045). A new `fw13-exfat` run is configured to compile that application again and publish `PPSA99283.exfat`; see [current GitHub Actions](https://github.com/Small182/UAPlay-ps5/actions).

## What is NOT confirmed

- Firmware **13.00** native title loader acceptance or successful startup.
- Actual video/audio, TLS, HLS, hardware controller and subtitle behavior.
- On-screen keyboard and feature parity with the latest UAPlay-nx.
- Resolution of previously reported console error `CE-107694-7`.

`PPSA99283.exfat` is a **hardware-untested development candidate**, not a verified working release. Success in a Linux CI test or filesystem packaging is not evidence of PS5 compatibility. More diagnosis requires console-side ShadowMount and native startup logs.
