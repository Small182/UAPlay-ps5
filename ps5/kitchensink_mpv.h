#ifndef UAPLAY_PS5_KITCHENSINK_MPV_H
#define UAPLAY_PS5_KITCHENSINK_MPV_H

/*
 * Small source-compatibility surface for UAPlay's Switch libmpv UI.
 * Native PS5 decoding is provided by SDL_kitchensink / FFmpeg (BFplayer's
 * playback stack), not mpv, EVO or Nuvio. This is NOT a libmpv implementation.
 */
#include <stddef.h>

typedef struct mpv_handle mpv_handle;
typedef struct mpv_render_context mpv_render_context;

typedef enum {
    MPV_FORMAT_NONE=0,
    MPV_FORMAT_FLAG=3,
    MPV_FORMAT_DOUBLE=5,
} mpv_format;

enum {
    MPV_EVENT_NONE=0,
    MPV_EVENT_START_FILE=6,
    MPV_EVENT_END_FILE=7,
    MPV_EVENT_FILE_LOADED=8,
    MPV_EVENT_LOG_MESSAGE=20,
};
enum { MPV_END_FILE_REASON_EOF=0, MPV_END_FILE_REASON_ERROR=4 };
typedef struct { int reason, error; } mpv_event_end_file;
typedef struct { const char *prefix, *text; } mpv_event_log_message;
typedef struct { int event_id; void *data; } mpv_event;
typedef struct { void *(*get_proc_address)(void *,const char *); void *get_proc_address_ctx; }
    mpv_opengl_init_params;
typedef struct { int fbo,w,h,internal_format; } mpv_opengl_fbo;
typedef struct { int type; void *data; } mpv_render_param;
#define MPV_RENDER_API_TYPE_OPENGL "opengl"
enum {
    MPV_RENDER_PARAM_INVALID=0,
    MPV_RENDER_PARAM_API_TYPE=1,
    MPV_RENDER_PARAM_OPENGL_INIT_PARAMS=2,
    MPV_RENDER_PARAM_OPENGL_FBO=3,
    MPV_RENDER_PARAM_FLIP_Y=4,
};

mpv_handle *mpv_create(void);
int mpv_initialize(mpv_handle *p);
int mpv_set_option_string(mpv_handle *p,const char *name,const char *value);
int mpv_set_property_string(mpv_handle *p,const char *name,const char *value);
int mpv_get_property(mpv_handle *p,const char *name,mpv_format format,void *out);
int mpv_command_async(mpv_handle *p,unsigned long long userdata,const char **args);
int mpv_request_log_messages(mpv_handle *p,const char *level);
mpv_event *mpv_wait_event(mpv_handle *p,double timeout);
const char *mpv_error_string(int code);
void mpv_terminate_destroy(mpv_handle *p);
int mpv_render_context_create(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args);
int mpv_render_context_render(mpv_render_context *context,const mpv_render_param *args);
void mpv_render_context_free(mpv_render_context *context);

/* PS5-only extension for the existing audio/subtitle picker. */
struct MediaTrack;
int ps5_kitchen_tracks(mpv_handle *p,int subtitles,struct MediaTrack *out,int max);
int ps5_kitchen_select_track(mpv_handle *p,int subtitles,int id);
int ps5_kitchen_set_volume(mpv_handle *p,double volume);

#endif
