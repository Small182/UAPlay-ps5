#ifndef UAPLAY_PLATFORM_PATHS_H
#define UAPLAY_PLATFORM_PATHS_H

/* A native PS5 title's writable sandbox mount. Never write into /app0. */
#ifdef __PS5__
#define UAPLAY_DATA_ROOT "/download0/uaplay"
#define UAPLAY_CATALOG_DIR UAPLAY_DATA_ROOT
#define UAPLAY_LIBRARY_DIR UAPLAY_DATA_ROOT "/library"
#define UAPLAY_POSTER_DIR UAPLAY_DATA_ROOT "/posters"
#define UAPLAY_ASSET(file) "romfs/" file
#else
#define UAPLAY_DATA_ROOT "sdmc:/switch"
#define UAPLAY_CATALOG_DIR "sdmc:/switch/uaplay"
#define UAPLAY_LIBRARY_DIR "sdmc:/switch/uaflix"
#define UAPLAY_POSTER_DIR "sdmc:/switch/uaplay/posters"
#define UAPLAY_ASSET(file) "romfs:/" file
#endif

#endif
