# UAPlay PS4 compatibility target

This is **the real UAPlay C interface** cross-compiled as a PS4 package for
PS4 and the PS5's PS4 backward-compatibility runtime. It does not use Nuvio,
EVO, ProsperoWeb or the native PS5 UFS2/ffpkg executable.

PS4 dependencies: PacBrew OpenOrbis + `SDL2`, `SDL2_ttf`, `SDL2_image`,
`libcurl`, and its PS4 `libmpv`/FFmpeg port. `libmpv` is used directly;
the PS5 SDL_kitchensink shim is deliberately excluded.

Build in GitHub Actions via `ps4-compat.yml`; download the
`UAPlay-PS4-PKG-UNTESTED` artifact and extract `UAPlay-PS4.pkg`.

This is an **experimental cross-build**, not a verified working PS5 release.
Runtime support depends on a PS4-compatible homebrew environment, working
Piglet/GLES2, compatible PS4 libmpv modules and the target console setup.
An ordinary unmodified PS5 cannot install or run unsigned PS4 PKGs.

Data, favourites and watch history are kept under `/data/uaplay` and persist
across PKG upgrades. The package embeds the TMDb build key in CI; the key
remains in the executable, so only use a key intended for a public client.

Known initial limitations: the PS4 software keyboard and hardware verification
are not yet completed; the player may require additional PS4-specific runtime
initialization or codecs. Do not call this a tested release yet.
