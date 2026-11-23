# UAPlay PS5 — status after Switch 0.5.10 synchronisation

**Hardware:** The owner confirmed that the prior `fw13-exfat` native PS5 build launched on a firmware **13.00** PS5. Its search, covers, IPTV, button legend, OPTIONS and UI quality needed fixes. The newer `fw13-ui-fixes-sync` builds are **not yet verified on the console**.

## Source reference

Switch upstream: [Small182/UAPlay-nx](https://github.com/Small182/UAPlay-nx), version `0.5.10`, inspected at commit `4e6edf9a714a8eaa28a089d277da4aa982a5be3c`.

The complete current Switch source/reference snapshot is preserved in `switch-upstream/source/` without overwriting native PS5 platform code.

## Integrated into native PS5 source

- Latest Switch catalogue and provider-resolution backend, additional providers, language logic and TMDb detail fetching.
- IPTV country index, network playlists, favorites and native channel playback UI.
- Downloads queue, pause/resume, retry, deletion and offline file playback.
- On-console Ukrainian/English DualSense virtual search keyboard.
- Software-native rounded focus ring and plain-font controller labels to avoid missing glyphs.
- Gamepad player shortcuts: left-stick 10s/60s seeking, right-stick volume and software video brightness, L1/R1 episode switch, central round playback controls, tap controls toggle and HUD.
- Upscaled PS5 text rendering, prefer 1920×1080 mode when available, fallback to default VideoOut.
- Wallhaven wallpaper gallery backend ported from OpenGL to SDL textures, list/settings navigation, persistent wallpaper file.
- Verified HTTPS root-CA bundle included in PS5 title. Diagnostic CURL/HTTP codes written to `/download0/uaplay/network.log` without disclosing media URLs or API keys.

## Remaining differences and validation

This is **not yet pixel-perfect feature parity** with the latest Switch UI. In particular, the complete advanced appearance menus, full per-provider configuration and SubDL UI are not yet integrated. PS5's FFmpeg/SDL_kitchensink player does not implement every libmpv command or subtitle feature identically. Hardware validation remains needed for catalogue TLS, actual movie covers, IPTV and HLS, subtitles, 1080p output and controller behavior after the UI changes.

Do not copy the Switch NRO or Switch system-service calls into the PS5 title. PS5 installation uses the experimental `PPSA99283.exfat`, not a PS4 PKG.

## Actions

[PS5 native CI](https://github.com/Small182/UAPlay-ps5/actions) builds the updated native title and UFS2/exFAT images when changes are pushed to `fw13-ui-fixes-sync`. A green CI build verifies cross-compilation and packaging only.

## Console feedback checklist

1. Confirm launch and whether selecting OPTIONS opens settings.
2. Search for a title using the DualSense keyboard; verify catalogue and metadata results.
3. Confirm covers load over HTTPS; if not, inspect `/download0/uaplay/network.log`.
4. Open IPTV and try a lawful/free M3U channel.
5. Open Wallpapers in Settings, browse and apply an image.
6. Confirm native VideoOut resolution via `/download0/uaplay/native-boot.log`.
7. Verify gamepad navigation, rounded focus, video playback and audio.
