/* Native Windows UI loads the shared player only when needed.
   Keep VLC / FFmpeg out of the executable's initial PE import table. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <SDL.h>
#include "playback.h"
#include "posters.h"

static HMODULE player_module;
static char load_error[256]="VLC player module not loaded";
static mpv_event empty_event={MPV_EVENT_NONE,NULL};
static mpv_handle * (*fn_mpv_create)(void);
static int (*fn_mpv_initialize)(mpv_handle *p);
static int (*fn_mpv_set_option_string)(mpv_handle *p,const char *name,const char *value);
static int (*fn_mpv_set_property_string)(mpv_handle *p,const char *name,const char *value);
static int (*fn_mpv_get_property)(mpv_handle *p,const char *name,mpv_format format,void *out);
static int (*fn_mpv_command_async)(mpv_handle *p,unsigned long long userdata,const char **args);
static int (*fn_mpv_request_log_messages)(mpv_handle *p,const char *level);
static mpv_event * (*fn_mpv_wait_event)(mpv_handle *p,double timeout);
static const char * (*fn_mpv_error_string)(int code);
static void (*fn_mpv_terminate_destroy)(mpv_handle *p);
static int (*fn_mpv_render_context_create)(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args);
static int (*fn_mpv_render_context_render)(mpv_render_context *context,const mpv_render_param *args);
static void (*fn_mpv_render_context_free)(mpv_render_context *context);
static int (*fn_ps5_kitchen_tracks)(mpv_handle *p,int subtitles,struct MediaTrack *out,int max);
static int (*fn_ps5_kitchen_select_track)(mpv_handle *p,int subtitles,int id);
static int (*fn_ps5_kitchen_set_volume)(mpv_handle *p,double volume);
static const char * (*fn_ps5_kitchen_last_error)(mpv_handle *p);
static const char * (*fn_ps5_kitchen_diagnostic)(mpv_handle *p);
static const char * (*fn_ps5_vlc_picture_info)(mpv_handle *p);
static int load_player(void){
    if(player_module)return 1;
    wchar_t path[32768];
    DWORD n=GetModuleFileNameW(NULL,path,32768);
    if(!n||n>=32768){snprintf(load_error,sizeof(load_error),"Cannot locate player module");return 0;}
    wchar_t *slash=wcsrchr(path,L'\\');
    if(!slash || (size_t)(slash-path)+32>=32768)return 0;
    wcscpy(slash+1,L"uaplay-player.dll");
    fprintf(stderr,"[Desktop Preview] Loading VLC player module\n");
    HMODULE lib=LoadLibraryExW(path,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!lib){snprintf(load_error,sizeof(load_error),"VLC DLL load failed (Windows error %lu)",(unsigned long)GetLastError());fprintf(stderr,"%s\n",load_error);return 0;}
    void (*bind_renderer)(SDL_Renderer *)=(void (*)(SDL_Renderer *))GetProcAddress(lib,"desktop_player_bind");
    if(!bind_renderer){snprintf(load_error,sizeof(load_error),"Player renderer export missing");goto fail;}
    fn_mpv_create=(mpv_handle * (*)(void))GetProcAddress(lib,"mpv_create");
    if(!fn_mpv_create){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_create");goto fail;}
    fn_mpv_initialize=(int (*)(mpv_handle *p))GetProcAddress(lib,"mpv_initialize");
    if(!fn_mpv_initialize){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_initialize");goto fail;}
    fn_mpv_set_option_string=(int (*)(mpv_handle *p,const char *name,const char *value))GetProcAddress(lib,"mpv_set_option_string");
    if(!fn_mpv_set_option_string){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_set_option_string");goto fail;}
    fn_mpv_set_property_string=(int (*)(mpv_handle *p,const char *name,const char *value))GetProcAddress(lib,"mpv_set_property_string");
    if(!fn_mpv_set_property_string){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_set_property_string");goto fail;}
    fn_mpv_get_property=(int (*)(mpv_handle *p,const char *name,mpv_format format,void *out))GetProcAddress(lib,"mpv_get_property");
    if(!fn_mpv_get_property){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_get_property");goto fail;}
    fn_mpv_command_async=(int (*)(mpv_handle *p,unsigned long long userdata,const char **args))GetProcAddress(lib,"mpv_command_async");
    if(!fn_mpv_command_async){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_command_async");goto fail;}
    fn_mpv_request_log_messages=(int (*)(mpv_handle *p,const char *level))GetProcAddress(lib,"mpv_request_log_messages");
    if(!fn_mpv_request_log_messages){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_request_log_messages");goto fail;}
    fn_mpv_wait_event=(mpv_event * (*)(mpv_handle *p,double timeout))GetProcAddress(lib,"mpv_wait_event");
    if(!fn_mpv_wait_event){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_wait_event");goto fail;}
    fn_mpv_error_string=(const char * (*)(int code))GetProcAddress(lib,"mpv_error_string");
    if(!fn_mpv_error_string){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_error_string");goto fail;}
    fn_mpv_terminate_destroy=(void (*)(mpv_handle *p))GetProcAddress(lib,"mpv_terminate_destroy");
    if(!fn_mpv_terminate_destroy){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_terminate_destroy");goto fail;}
    fn_mpv_render_context_create=(int (*)(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args))GetProcAddress(lib,"mpv_render_context_create");
    if(!fn_mpv_render_context_create){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_render_context_create");goto fail;}
    fn_mpv_render_context_render=(int (*)(mpv_render_context *context,const mpv_render_param *args))GetProcAddress(lib,"mpv_render_context_render");
    if(!fn_mpv_render_context_render){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_render_context_render");goto fail;}
    fn_mpv_render_context_free=(void (*)(mpv_render_context *context))GetProcAddress(lib,"mpv_render_context_free");
    if(!fn_mpv_render_context_free){snprintf(load_error,sizeof(load_error),"Missing player export: mpv_render_context_free");goto fail;}
    fn_ps5_kitchen_tracks=(int (*)(mpv_handle *p,int subtitles,struct MediaTrack *out,int max))GetProcAddress(lib,"ps5_kitchen_tracks");
    if(!fn_ps5_kitchen_tracks){snprintf(load_error,sizeof(load_error),"Missing player export: ps5_kitchen_tracks");goto fail;}
    fn_ps5_kitchen_select_track=(int (*)(mpv_handle *p,int subtitles,int id))GetProcAddress(lib,"ps5_kitchen_select_track");
    if(!fn_ps5_kitchen_select_track){snprintf(load_error,sizeof(load_error),"Missing player export: ps5_kitchen_select_track");goto fail;}
    fn_ps5_kitchen_set_volume=(int (*)(mpv_handle *p,double volume))GetProcAddress(lib,"ps5_kitchen_set_volume");
    if(!fn_ps5_kitchen_set_volume){snprintf(load_error,sizeof(load_error),"Missing player export: ps5_kitchen_set_volume");goto fail;}
    fn_ps5_kitchen_last_error=(const char * (*)(mpv_handle *p))GetProcAddress(lib,"ps5_kitchen_last_error");
    if(!fn_ps5_kitchen_last_error){snprintf(load_error,sizeof(load_error),"Missing player export: ps5_kitchen_last_error");goto fail;}
    fn_ps5_kitchen_diagnostic=(const char * (*)(mpv_handle *p))GetProcAddress(lib,"ps5_kitchen_diagnostic");
    if(!fn_ps5_kitchen_diagnostic){snprintf(load_error,sizeof(load_error),"Missing player export: ps5_kitchen_diagnostic");goto fail;}
    fn_ps5_vlc_picture_info=(const char * (*)(mpv_handle *p))GetProcAddress(lib,"ps5_vlc_picture_info");
    if(!fn_ps5_vlc_picture_info){snprintf(load_error,sizeof(load_error),"Missing player export: ps5_vlc_picture_info");goto fail;}
    bind_renderer(ui_renderer);player_module=lib;
    fprintf(stderr,"[Desktop Preview] VLC player module ready\n");
    return 1;
fail:
    fprintf(stderr,"%s\n",load_error);
    FreeLibrary(lib);
    fn_mpv_create=NULL;
    fn_mpv_initialize=NULL;
    fn_mpv_set_option_string=NULL;
    fn_mpv_set_property_string=NULL;
    fn_mpv_get_property=NULL;
    fn_mpv_command_async=NULL;
    fn_mpv_request_log_messages=NULL;
    fn_mpv_wait_event=NULL;
    fn_mpv_error_string=NULL;
    fn_mpv_terminate_destroy=NULL;
    fn_mpv_render_context_create=NULL;
    fn_mpv_render_context_render=NULL;
    fn_mpv_render_context_free=NULL;
    fn_ps5_kitchen_tracks=NULL;
    fn_ps5_kitchen_select_track=NULL;
    fn_ps5_kitchen_set_volume=NULL;
    fn_ps5_kitchen_last_error=NULL;
    fn_ps5_kitchen_diagnostic=NULL;
    fn_ps5_vlc_picture_info=NULL;
    return 0;
}
mpv_handle * mpv_create(void){if(!load_player())return NULL;return fn_mpv_create?fn_mpv_create():NULL;}
int mpv_initialize(mpv_handle *p){return fn_mpv_initialize?fn_mpv_initialize(p):-1;}
int mpv_set_option_string(mpv_handle *p,const char *name,const char *value){return fn_mpv_set_option_string?fn_mpv_set_option_string(p,name,value):-1;}
int mpv_set_property_string(mpv_handle *p,const char *name,const char *value){return fn_mpv_set_property_string?fn_mpv_set_property_string(p,name,value):-1;}
int mpv_get_property(mpv_handle *p,const char *name,mpv_format format,void *out){return fn_mpv_get_property?fn_mpv_get_property(p,name,format,out):-1;}
int mpv_command_async(mpv_handle *p,unsigned long long userdata,const char **args){return fn_mpv_command_async?fn_mpv_command_async(p,userdata,args):-1;}
int mpv_request_log_messages(mpv_handle *p,const char *level){return fn_mpv_request_log_messages?fn_mpv_request_log_messages(p,level):-1;}
mpv_event * mpv_wait_event(mpv_handle *p,double timeout){return fn_mpv_wait_event?fn_mpv_wait_event(p,timeout):&empty_event;}
const char * mpv_error_string(int code){return fn_mpv_error_string?fn_mpv_error_string(code):load_error;}
void mpv_terminate_destroy(mpv_handle *p){if(fn_mpv_terminate_destroy)fn_mpv_terminate_destroy(p);}
int mpv_render_context_create(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args){return fn_mpv_render_context_create?fn_mpv_render_context_create(out,p,args):-1;}
int mpv_render_context_render(mpv_render_context *context,const mpv_render_param *args){return fn_mpv_render_context_render?fn_mpv_render_context_render(context,args):-1;}
void mpv_render_context_free(mpv_render_context *context){if(fn_mpv_render_context_free)fn_mpv_render_context_free(context);}
int ps5_kitchen_tracks(mpv_handle *p,int subtitles,struct MediaTrack *out,int max){return fn_ps5_kitchen_tracks?fn_ps5_kitchen_tracks(p,subtitles,out,max):0;}
int ps5_kitchen_select_track(mpv_handle *p,int subtitles,int id){return fn_ps5_kitchen_select_track?fn_ps5_kitchen_select_track(p,subtitles,id):0;}
int ps5_kitchen_set_volume(mpv_handle *p,double volume){return fn_ps5_kitchen_set_volume?fn_ps5_kitchen_set_volume(p,volume):0;}
const char * ps5_kitchen_last_error(mpv_handle *p){return fn_ps5_kitchen_last_error?fn_ps5_kitchen_last_error(p):load_error;}
const char * ps5_kitchen_diagnostic(mpv_handle *p){return fn_ps5_kitchen_diagnostic?fn_ps5_kitchen_diagnostic(p):load_error;}
const char * ps5_vlc_picture_info(mpv_handle *p){return fn_ps5_vlc_picture_info?fn_ps5_vlc_picture_info(p):"VLC unavailable";}
