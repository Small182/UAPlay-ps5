# UAPlay PS5 — experimental native EVO title

This is an **unvalidated test build** for a homebrew-enabled PS5 on firmware
13.60, derived from Nuvio's native EVO host. Passing GitHub Actions does not
confirm that it launches, receives DualSense input, or plays video on hardware.

## Build artifact

Download `UAPlay-PS5-EVO-native-UNTESTED` from the successful GitHub Actions
run and unpack `UAPlay-PS5-EVO-test.tar.gz`. It contains:

- `PPSA99282/` — native title including `eboot.bin`, `sce_module/`,
  `sce_sys/`, and the UAPlay `webui/` folder.
- `helpers/` — diagnostic, registration, and permission ELFs.
- `build.json` — checksums for the title files and the native build's inputs.

## Console test

1. On firmware 13.60, enable the existing compatible homebrew environment,
   start ShadowMount and an ELF loader at port `9021`, and enable FTP.
2. With a fresh, unoccupied title ID, copy the **entire** `PPSA99282` folder
   through FTP to `/data/homebrew/PPSA99282`.
3. Refresh ShadowMount's homebrew titles and open the UAPlay tile.
4. Confirm that the catalog loads. Use a lawful direct HLS or MP4 stream to
   check native EVO playback; pressing Circle should return to browsing.

Do not overwrite an existing title or assume that a successful copy is proof
of installation. Keep logs free of stream URLs, API keys, and account data.

## What is and isn't present

- Implemented: Ukrainian catalog UI, TMDb lookup (when `TMDB_API_KEY` is
  present during packaging), favorites, film/series details, PlayStation button
  legend, and handoff of a direct HTTP(S) media URL to the EVO player.
- Experimental: UAFlix/UASerials source discovery and a subset of episode
  listings, dependent on the public UAFresh proxy allowing cross-origin
  requests. Not yet confirmed on hardware.
- Pending: full UAPlay-nx source/playlist parity, verified DualSense input,
  on-device audio, video, and navigation testing.

This is not a PS5 retail package or an officially supported PlayStation app.
