# UAPlay PS5 native port — work in progress

This tree is based on the Switch UAPlay codebase. It is **not** a runnable PS5
application yet. The automated workflow checks the cross-platform catalog
code; it does **not** produce a PS5 executable or installable title.

## Implemented groundwork

- `__PS5__` platform guards for libnx-only initialization and lifecycle calls.
- Dedicated data paths under `/data/uaplay`, separate from Switch SD paths.
- PS5 button legend (`✕`, `◯`, `□`, `△`, `L1`, `R1`, `OPTIONS`) and unchanged
  SDL south/east/west/north mapping rather than Switch's ABXY remapping.
- Portable catalog regression tests in GitHub Actions.

## Required for an actual native build

1. Pin a tested PS5 Payload SDK, SDL2, SDL2_ttf, SDL2_image, curl, and GLES
   implementation. The public PS5 OpenGL/SDL2 bridge is a candidate, not yet
   validated against this application.
2. Port **libmpv** and its FFmpeg/audio dependencies, or replace the playback
   backend with a proven PS5 decoder while retaining the existing player UI.
3. Add a PS5-native application entry point, heap/runtime bootstrap and title
   packaging, using an SDK-compatible native folder template.
4. Implement the PS5 text-entry dialog. The keyboard function currently
   returns no entry on PS5; catalog browsing is unaffected, search is blocked.
5. Ensure asset paths are relative to the native title working directory;
   stage the entire `romfs/` folder alongside the executable.
6. Test actual DualSense buttons, TLS, video, audio, HLS and storage on PS5
   firmware 13.60. No hardware validation has been performed.

The UI layout, catalog, film/series details, favorites and navigation are not
intentionally redesigned for this port. Native compilation and successful CI
are separate milestones; neither implies playback works on hardware.
