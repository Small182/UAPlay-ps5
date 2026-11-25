# UAPlay-web 0.5.69 → PS5 native interface port

This branch is a native SDL2/VideoOut recreation of the presentation and
interaction patterns from [UAPlay-web](https://github.com/Small182/UAPlay-web)
`main` at commit `ffeebb1433f1157843c4d0c7c69d73109a9d8cee`.
It does **not** embed a browser, ProsperoWeb, a WebView or execute web CSS/JS.
The PS5 FW 13.00 / ShadowMountPlus image keeps the existing native libmpv
pipeline and Cloudflare API integration from `fw13-full-web-native`.

## Web visual vocabulary mapped into native UI

| UAPlay-web | Native PS5 rendering |
| --- | --- |
| `--bg: #090b0e`, `--panel: #171c24` | Retained RGB values; disabled historical dark-grey wash-out |
| Light `--bg: #e8edf0`, `--panel: #f8fafb` | Remapped to web light palette and text contrast |
| `.topbar`, tabs, clock | 80px header, current title tabs and controller navigation |
| `.home-hero` | 222px hero, 132×198 poster, metadata and primary/favorite actions |
| `.btn.primary` | Accent-filled and contrast-aware text, secondary neutral fill |
| `.poster-grid`, `.poster-card` | 6-column 150×221 cover cards, one-line titles, visible focus ring |
| `.settings-card` | Full-width stacked settings and appearance cards |
| `.channel-grid`, `.channel-entry` | 4-column compact IPTV cards, logo, title, group and favorite star |
| `.search-box` (IPTV) | Focusable query field with native Ukrainian keyboard |
| `.switch-player-panel` | Central play/pause / ±10s round buttons, left volume, right tools |
| `.player-settings-menu` | In-video overlay for video settings and audio/subtitle tracks |
| `.episode-watch-state` | Locally persisted watched / started / resume markers |

Other Switch-style screens from UAPlay-web (details, seasons, episode picker,
sources, wallpaper selection, download manager, color swatches) retain the
native PS5 equivalent and existing DualSense behavior.

## Console input

- D-pad/left stick: focus navigation; Cross: activate; Circle: back.
- Home / catalog: shoulder section navigation; Square: search.
- IPTV: 4 columns, **R3** searches channel names/groups, **L3** clears,
  Triangle adds/removes favorites; L1/R1 pages.
- Native player: Cross play/pause, left/right ±10 sec, Options settings,
  Triangle audio, Square subtitles, and volume via right stick.
- Player settings/tracks open **over the currently playing video**.
- Onscreen Ukrainian and QWERTY keyboards remain functional.
- Touch/pointer hit boxes have been updated to the new cards and player panels.

## Validation and outstanding device-specific concerns

- `scripts/test-portable.sh` tests episode persistence, catalog behavior
  and case-insensitive Ukrainian/English IPTV search.
- `.github/workflows/native-ps5.yml` creates `PPSA99283.exfat`
  through `UAPlay-PS5-exFAT-FW13-UNTESTED`.
- A green CI result confirms compile/package and host smoke checks; it **does not
  verify** physical GPU/decoder behavior, stream CORS, device network,
  frame pacing or pixel-perfect appearance on a real FW13 PS5.
- Web-specific iPhone Dynamic Island handling, PWA, Safari fullscreen and
  IndexedDB are not copied: they do not exist in the native PS5 environment.
  The PS5 uses the console's native video and filesystem instead.
- Cloudflare Worker endpoint can be entered under Settings → UAPlay-web API
  using its **hostname only**, or compiled through the existing Actions
  `UAPLAY_WEB_API_URL` variable. Without it, native direct TMDB/provider
  fallback remains active.
