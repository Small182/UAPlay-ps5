# UAPlay PS5 — native port (experimental)

Native homebrew port of [UAPlay-nx](https://github.com/Small182/UAPlay-nx) to the PlayStation 5. The **Switch repository is not modified**.

**Branch:** `fw13-exfat` (based on the existing native `native-ps5` VideoOut branch). **Status:** untested on physical PS5 firmware 13.00; not a stable release.

## Native implementation

- Ported SDL-based UAPlay C catalogue and interface with PlayStation controller labels.
- Native PS5 VideoOut software renderer, avoiding the earlier OpenGL/EVO boot experiments.
- FFmpeg/SDL_kitchensink playback adapter (video, audio and seeking); on-console codecs, TLS/HLS and DualSense remain unverified.
- Native title folder `PPSA99283` containing `eboot.bin`, runtime modules, title metadata and UAPlay fonts/assets.
- GitHub Actions cross-compilation and ShadowMountPlus-compatible **64 KiB cluster exFAT** packaging. A separate lightweight packaging smoke test verifies image readback and exFAT boot-sector geometry.

## Build artifacts

Open [GitHub Actions](https://github.com/Small182/UAPlay-ps5/actions), select **UAPlay native PS5 VideoOut + ShadowMount exFAT** on branch `fw13-exfat`, then select a **successful** run:

- `UAPlay-PS5-Folder-UNTESTED`: native title folder tar.gz.
- `UAPlay-PS5-VideoOut-UNTESTED`: UFS2 `PPSA99283.ffpkg`.
- `UAPlay-PS5-exFAT-FW13-UNTESTED`: `PPSA99283.exfat`, 64 KiB clusters.

GitHub Actions may package a binary without it being compatible with a specific console firmware. **Do not mistake an artifact or green CI check for confirmed launch or playable streaming.**

See [TESTING_FW13.md](TESTING_FW13.md) for Ukrainian installation and troubleshooting guidance and [STATUS.md](STATUS.md) for exact limitations.

## Unfinished work

Hardware validation on firmware 13.00; on-screen keyboard; provider-specific headers; full audio/subtitle support; and synchronization with the newest Switch UI/features. The imported PS5 C sources lag behind the current UAPlay-nx source, so this is a native-port development candidate, **not** feature parity with UAPlay-nx v0.5.9.

## Third-party components

The native build fetches pinned public PS5 homebrew tooling and PacBrew dependencies. It does not embed Sony firmware, proprietary system files or keys. This is a community homebrew build, not an official PS5 application.
