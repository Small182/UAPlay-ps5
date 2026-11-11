#ifndef UAPLAY_PLATFORM_PATHS_H
#define UAPLAY_PLATFORM_PATHS_H

/* A native PS5 title's writable sandbox mount. Never write into /app0. */
#ifdef __PS5__
#define UAPLAY_DATA_ROOT "/download0/uaplay"
#define UAPLAY_CATALOG_DIR UAPLAY_DATA_ROOT
#define UAPLAY_LIBRARY_DIR UAPLAY_DATA_ROOT "/library"
#define UAPLAY_POSTER_DIR UAPLAY_DATA_ROOT "/posters"
#ifdef UAPLAY_PS5_SOFTWARE
#define UAPLAY_ASSET(file) "/app0/romfs/" file
#else
#define UAPLAY_ASSET(file) "romfs/" file
#endif
#elif defined(__PS4__)
/* PS4 packages mount read-only at /app0; use /data for persistent files. */
#define UAPLAY_DATA_ROOT "/data/uaplay"
#define UAPLAY_CATALOG_DIR UAPLAY_DATA_ROOT
#define UAPLAY_LIBRARY_DIR UAPLAY_DATA_ROOT "/library"
#define UAPLAY_POSTER_DIR UAPLAY_DATA_ROOT "/posters"
#define UAPLAY_ASSET(file) "/app0/romfs/" file
#else
#define UAPLAY_DATA_ROOT "sdmc:/switch"
#define UAPLAY_CATALOG_DIR "sdmc:/switch/uaplay"
#define UAPLAY_LIBRARY_DIR "sdmc:/switch/uaflix"
#define UAPLAY_POSTER_DIR "sdmc:/switch/uaplay/posters"
#define UAPLAY_ASSET(file) "romfs:/" file
#endif

#endif
