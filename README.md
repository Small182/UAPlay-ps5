# UAPlay PS5 — native homebrew

Native community port based on [UAPlay-nx](https://github.com/Small182/UAPlay-nx) for Nintendo Switch. Works with experimental PS5 native title loading, **not Sony retail PKG**. The Switch repository is unchanged.

**Active network-fix test branch:** [`fw13-native-network-v0510`](https://github.com/Small182/UAPlay-ps5/tree/fw13-native-network-v0510)

**Console report addressed:** movie catalogue empty, TV catalogue displayed `HTTP 0: Failed initialization`, and pressing language switch in the old search keyboard exited the app. The latest test branch adds console-tested native PS5 libcurl/DNS/socket compat, retries early libcurl init failure, fixes offline asset paths, surfaces catalogue errors, and replaces the keyboard with QWERTY / ЙЦУКЕН grid navigation. **Not yet confirmed working on physical PS5.**

The owner confirmed that the previous `fw13-exfat` native build started on **PS5 firmware 13.00**, but it had broken search, no covers, missing IPTV, an invalid button legend, square focus outlines, very small UI and an OPTIONS exit bug.

## Updated Switch features (0.5.10)

The newer PS5 branch incorporates:

- Latest film/TV metadata and provider source discovery from Switch.
- IPTV channels, country selection, favorites and native player support.
- Offline downloads, queue controls, retry and stored-video playback.
- Ukrainian/English DualSense virtual keyboard and working search entry.
- Latest player controls (L1 previous/R1 next episode, left-stick seek by 10/60 seconds; right-stick volume and software video brightness, round center controls, touch overlay toggling).
- Rounded focus highlighting and safe ASCII PlayStation button legend for missing Unicode glyphs.
- High-resolution PS5 typography and native 1080p VideoOut preference when available.
- Wallhaven wallpaper gallery and stored backgrounds through PS5 SDL textures.
- Verified TLS trust bundle and non-sensitive network diagnostics.

**Important:** This branch is a buildable *test candidate*, not yet revalidated on physical PS5 after these changes. Exact feature parity with all Switch 0.5.10 appearance/advanced settings and SubDL player features remains unfinished. Full current Switch source is included for comparison under `switch-upstream/source/`.

## How to get the build

1. Open [GitHub Actions](https://github.com/Small182/UAPlay-ps5/actions) and select **UAPlay native PS5 VideoOut + updated Switch features**.
2. Choose a **successful** run on branch `fw13-native-network-v0510`.
3. Download **UAPlay-PS5-exFAT-FW13-UNTESTED** and extract the ZIP to obtain `PPSA99283.exfat`.
4. Copy the resulting image into the ShadowMountPlus native homebrew scan directory (for example `/data/homebrew/`), using the supported native PS5 homebrew environment.

Do **not** install `.exfat` as a PS4 PKG, and do not overwrite working installations without keeping a backup.

See [TESTING_FW13.md](TESTING_FW13.md) and [STATUS.md](STATUS.md) for hardware test details and caveats. If covers or searches still fail, examine `/download0/uaplay/network.log`; if the display is low-resolution, check `/download0/uaplay/native-boot.log`.

## Controller legend

| Action | DualSense |
| --- | --- |
| Confirm / select | Cross (X) |
| Back | Circle (O) |
| Search / extras | Square ([]) |
| OPTIONS | Main settings |
| Previous / next episode | L1 / R1 |
| Video seek | Left stick: left/right ±10s, up/down ±60s |
| Playback volume | Right stick left/right |
| Video brightness overlay | Right stick up/down |

Video brightness is a picture overlay, not a hardware TV backlight controller.

## Build method

GitHub Actions uses pinned public PS5 homebrew toolchain dependencies and checks `eboot.bin`, title assets and 64 KiB-cluster exFAT package structure. CI success does not guarantee firmware compatibility, playable HLS streams or functional PS5 controller/keyboard.
