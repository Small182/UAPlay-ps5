#ifndef UAPLAY_EVO_BRIDGE_H
#define UAPLAY_EVO_BRIDGE_H

#include <stddef.h>

/* EVO's loopback API works only while the EVO host is active in this title.
 * This adapter alone does not launch an installed EVO Player application. */
int uaplay_evo_build_request(const char *stream_url, const char *title,
                             char *out, size_t capacity);

/* Caller must have initialized libcurl. Returns 0 only on HTTP 200. */
int uaplay_evo_handoff(const char *stream_url, const char *title,
                       char *error, size_t error_capacity);

#endif
