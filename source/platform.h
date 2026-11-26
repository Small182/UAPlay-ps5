#ifndef UAPLAY_PLATFORM_H
#define UAPLAY_PLATFORM_H

/* Storage roots per console. Host-side tests keep the Switch layout. */
#if defined(UAPLAY_DESKTOP_PREVIEW)
#define UAPLAY_DATA_ROOT "preview-data"
#define UAPLAY_DATA_DIR UAPLAY_DATA_ROOT
#define UAPLAY_ROMFS "romfs/"
#ifdef _WIN32
#define UAPLAY_CA_BUNDLE UAPLAY_ROMFS "cacert.pem"
#else
#define UAPLAY_CA_BUNDLE "/etc/ssl/certs/ca-certificates.crt"
#endif
#elif defined(__PS5__)
#define UAPLAY_CONSOLE 1
#define UAPLAY_DATA_ROOT "/download0"
#define UAPLAY_ROMFS "/app0/romfs/"
#define UAPLAY_CA_BUNDLE UAPLAY_ROMFS "cacert.pem"
#define UAPLAY_DATA_DIR "/download0/uaplay"
#define UAPLAY_PREVIOUS_DATA_DIR "/download0/uaflix"
#elif defined(__vita__)
#define UAPLAY_CONSOLE 1
#define UAPLAY_DATA_ROOT "ux0:data"
#define UAPLAY_ROMFS "app0:romfs/"
/* OpenSSL on the Vita has no system trust store; ship Mozilla's bundle. */
#define UAPLAY_CA_BUNDLE UAPLAY_ROMFS "cacert.pem"
#else
#ifdef __SWITCH__
#define UAPLAY_CONSOLE 1
#endif
#define UAPLAY_DATA_ROOT "sdmc:/switch"
#define UAPLAY_ROMFS "romfs:/"
#endif
#ifndef UAPLAY_DATA_DIR
#define UAPLAY_DATA_DIR UAPLAY_DATA_ROOT "/uaplay"
#endif
/* Read only while migrating data from older Switch builds. */
#ifndef UAPLAY_PREVIOUS_DATA_DIR
#define UAPLAY_PREVIOUS_DATA_DIR UAPLAY_DATA_ROOT "/uaflix"
#endif

#endif
