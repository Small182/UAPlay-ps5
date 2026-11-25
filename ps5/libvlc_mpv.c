/* SPDX-License-Identifier: GPL-3.0-or-later
 * UAPlay native PS5 libVLC adapter. Reuses the catalogue's existing player API.
 * First integration uses CPU video callbacks and SDL output, not Vulkan/HW decode.
 */
#define _POSIX_C_SOURCE 200809L
#include "../ps5/kitchensink_mpv.h"
#include "playback.h"
#include "posters.h"
#include <SDL.h>
#include <vlc/vlc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

struct mpv_handle {
    libvlc_instance_t *vlc;
    libvlc_media_player_t *mp;
    SDL_mutex *lock;
    SDL_AudioDeviceID audio;
    unsigned char *frame;
    unsigned width,height,pitch,serial,decoded,generation;
    int active,started,loaded,volume;
    char ua[160],referer[512],error[256],diag[160];
};
struct mpv_render_context {
    mpv_handle *p;
    SDL_Texture *texture;
    unsigned width,height,serial,generation;
};

static unsigned video_setup(void **opaque,char *chroma,unsigned *w,unsigned *h,
                            unsigned *pitch,unsigned *lines) {
    mpv_handle *p=*opaque;
    if(!*w || !*h)return 0;
    if(*w>1280 || *h>720){
        double scale=fmin(1280.0/(*w),720.0/(*h));
        *w=(unsigned)(*w*scale);*h=(unsigned)(*h*scale);
    }
    if(!*w || !*h)return 0;
    memcpy(chroma,"RV32",4);
    *pitch=(*w*4+63)&~63u;*lines=(*h+15)&~15u;
    SDL_LockMutex(p->lock);
    free(p->frame);
    p->frame=NULL;
    if(posix_memalign((void**)&p->frame,64,(size_t)*pitch**lines)!=0)p->frame=NULL;
    p->width=*w;p->height=*h;p->pitch=*pitch;
    p->serial=p->decoded=0;
    snprintf(p->diag,sizeof(p->diag),"VLC video %ux%u",*w,*h);
    SDL_UnlockMutex(p->lock);
    return p->frame?1:0;
}
static void video_cleanup(void *opaque){
    mpv_handle *p=opaque;
    SDL_LockMutex(p->lock);
    free(p->frame);p->frame=NULL;p->serial=p->decoded=0;
    SDL_UnlockMutex(p->lock);
}
static void *video_lock(void *opaque,void **planes){
    mpv_handle *p=opaque;
    SDL_LockMutex(p->lock);
    *planes=p->frame;
    return p;
}
static void video_unlock(void *opaque,void *picture,void *const *planes){
    (void)picture;(void)planes;
    mpv_handle *p=opaque;
    p->decoded++;
    SDL_UnlockMutex(p->lock);
}
static void video_display(void *opaque,void *picture){
    (void)picture;mpv_handle *p=opaque;
    SDL_LockMutex(p->lock);p->serial=p->decoded;SDL_UnlockMutex(p->lock);
}
static void audio_play(void *opaque,const void *samples,unsigned count,int64_t pts){
    (void)pts;
    mpv_handle *p=opaque;
    if(p->audio && SDL_GetQueuedAudioSize(p->audio)<192000)
        SDL_QueueAudio(p->audio,samples,count*4);
}
static void audio_pause(void *opaque,int64_t pts){
    (void)pts;mpv_handle *p=opaque;if(p->audio)SDL_PauseAudioDevice(p->audio,1);
}
static void audio_resume(void *opaque,int64_t pts){
    (void)pts;mpv_handle *p=opaque;if(p->audio)SDL_PauseAudioDevice(p->audio,0);
}
static void audio_flush(void *opaque,int64_t pts){
    (void)pts;mpv_handle *p=opaque;if(p->audio)SDL_ClearQueuedAudio(p->audio);
}
static void audio_drain(void *opaque){(void)opaque;}
static void stop(mpv_handle *p){
    p->active=0;
    if(p->mp)libvlc_media_player_stop(p->mp);
    if(p->audio)SDL_ClearQueuedAudio(p->audio);
    p->loaded=p->started=0;
    p->generation++;
}
mpv_handle *mpv_create(void){
    mpv_handle *p=calloc(1,sizeof(*p));
    if(!p)return NULL;
    p->lock=SDL_CreateMutex();p->volume=100;
    if(!p->lock){free(p);return NULL;}return p;
}
int mpv_initialize(mpv_handle *p){
    const char *args[]={"--no-video-title-show","--no-osd","--no-plugins-cache",
        "--avcodec-hw=none","--network-caching=2000","--verbose=-1"};
    if(!p)return -1;
    p->vlc=libvlc_new(sizeof(args)/sizeof(args[0]),args);
    if(!p->vlc)return -1;
    p->mp=libvlc_media_player_new(p->vlc);
    if(!p->mp)return -1;
    libvlc_video_set_callbacks(p->mp,video_lock,video_unlock,video_display,p);
    libvlc_video_set_format_callbacks(p->mp,video_setup,video_cleanup);
    SDL_AudioSpec spec={0};
    spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=1024;
    p->audio=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
    if(p->audio)SDL_PauseAudioDevice(p->audio,0);
    libvlc_audio_set_callbacks(p->mp,audio_play,audio_pause,audio_resume,audio_flush,audio_drain,p);
    libvlc_audio_set_format(p->mp,"S16N",48000,2);
    snprintf(p->diag,sizeof(p->diag),"libVLC %s",libvlc_get_version());
    return 0;
}
int mpv_set_option_string(mpv_handle *p,const char *n,const char *v){(void)p;(void)n;(void)v;return 0;}
int ps5_kitchen_set_volume(mpv_handle *p,double v){
    if(!p||!p->mp)return 0;p->volume=(int)v;return libvlc_audio_set_volume(p->mp,p->volume)==0;
}
int mpv_set_property_string(mpv_handle *p,const char *n,const char *v){
    if(!p||!p->mp||!n||!v)return -1;
    if(!strcmp(n,"user-agent")){snprintf(p->ua,sizeof(p->ua),"%s",v);return 0;}
    if(!strcmp(n,"referrer")){snprintf(p->referer,sizeof(p->referer),"%s",v);return 0;}
    if(!strcmp(n,"pause")){libvlc_media_player_set_pause(p->mp,!strcmp(v,"yes")||!strcmp(v,"true"));return 0;}
    if(!strcmp(n,"volume"))return ps5_kitchen_set_volume(p,atof(v))?0:-1;
    if(!strcmp(n,"aid")||!strcmp(n,"sid"))return ps5_kitchen_select_track(p,!strcmp(n,"sid"),!strcmp(v,"no")?-1:atoi(v))?0:-1;
    return -1;
}
int mpv_get_property(mpv_handle *p,const char *n,mpv_format f,void *out){
    if(!p||!p->mp||!out)return -1;
    if(f==MPV_FORMAT_DOUBLE){
        libvlc_time_t t;
        if(!strcmp(n,"duration"))t=libvlc_media_player_get_length(p->mp);
        else if(!strcmp(n,"time-pos"))t=libvlc_media_player_get_time(p->mp);
        else return -1;
        *(double*)out=t>0?t/1000.0:0;return 0;
    }
    if(f==MPV_FORMAT_FLAG&&!strcmp(n,"pause")){*(int*)out=libvlc_media_player_get_state(p->mp)==libvlc_Paused;return 0;}
    return -1;
}
int mpv_command_async(mpv_handle *p,unsigned long long userdata,const char **a){
    (void)userdata;if(!p||!p->mp||!a||!a[0])return -1;
    if(!strcmp(a[0],"loadfile")&&a[1]){
        stop(p);p->error[0]=0;
        libvlc_media_t *m=strstr(a[1],"://")?libvlc_media_new_location(p->vlc,a[1]):libvlc_media_new_path(p->vlc,a[1]);
        if(!m){snprintf(p->error,sizeof(p->error),"libVLC: unable to create media");return -1;}
        char opt[600];
        if(p->ua[0]){snprintf(opt,sizeof(opt),":http-user-agent=%s",p->ua);libvlc_media_add_option(m,opt);}
        if(p->referer[0]){snprintf(opt,sizeof(opt),":http-referrer=%s",p->referer);libvlc_media_add_option(m,opt);}
        libvlc_media_player_set_media(p->mp,m);libvlc_media_release(m);
        p->active=1;snprintf(p->diag,sizeof(p->diag),"VLC opening");
        if(p->audio)SDL_PauseAudioDevice(p->audio,0);
        int rc=libvlc_media_player_play(p->mp);
        if(rc<0)snprintf(p->error,sizeof(p->error),"libVLC: %s",libvlc_errmsg()?libvlc_errmsg():"unable to start playback");
        return rc;
    }
    if(!strcmp(a[0],"stop")){stop(p);return 0;}
    if(!strcmp(a[0],"cycle")&&a[1]&&!strcmp(a[1],"pause")){libvlc_media_player_pause(p->mp);return 0;}
    if(!strcmp(a[0],"seek")&&a[1]){
        libvlc_time_t t=(libvlc_time_t)(atof(a[1])*1000);
        if(!a[2]||strcmp(a[2],"absolute"))t+=libvlc_media_player_get_time(p->mp);
        libvlc_media_player_set_time(p->mp,t<0?0:t);return 0;
    }
    if(!strcmp(a[0],"set")&&a[1]&&a[2])return mpv_set_property_string(p,a[1],a[2]);
    if(!strcmp(a[0],"sub-add")&&a[1])return libvlc_media_player_add_slave(p->mp,libvlc_media_slave_type_subtitle,a[1],1);
    return -1;
}
mpv_event *mpv_wait_event(mpv_handle *p,double timeout){
    (void)timeout;static mpv_event e;static mpv_event_end_file end;
    e.event_id=MPV_EVENT_NONE;e.data=NULL;if(!p||!p->active)return &e;
    libvlc_state_t s=libvlc_media_player_get_state(p->mp);
    if(!p->started){p->started=1;e.event_id=MPV_EVENT_START_FILE;return &e;}
    if(s==libvlc_Error||s==libvlc_Ended){
        if(s==libvlc_Error && !p->error[0])snprintf(p->error,sizeof(p->error),"libVLC media player error");
        p->active=0;end.reason=s==libvlc_Error?MPV_END_FILE_REASON_ERROR:MPV_END_FILE_REASON_EOF;end.error=s==libvlc_Error?-1:0;
        e.event_id=MPV_EVENT_END_FILE;e.data=&end;return &e;
    }
    if(!p->loaded){
        SDL_LockMutex(p->lock);unsigned frames=p->serial;SDL_UnlockMutex(p->lock);
        if(frames||(s==libvlc_Playing&&libvlc_video_get_track_count(p->mp)==0)){
            p->loaded=1;e.event_id=MPV_EVENT_FILE_LOADED;
        }
    }
    return &e;
}
int mpv_request_log_messages(mpv_handle *p,const char *level){(void)p;(void)level;return 0;}
const char *mpv_error_string(int code){return code<0?"libVLC playback/render error":"OK";}
const char *ps5_kitchen_last_error(mpv_handle *p){return p&&p->error[0]?p->error:NULL;}
const char *ps5_kitchen_diagnostic(mpv_handle *p){return p?p->diag:"VLC not initialized";}
void mpv_terminate_destroy(mpv_handle *p){
    if(!p)return;stop(p);if(p->mp)libvlc_media_player_release(p->mp);
    if(p->vlc)libvlc_release(p->vlc);if(p->audio)SDL_CloseAudioDevice(p->audio);
    free(p->frame);SDL_DestroyMutex(p->lock);free(p);
}
int mpv_render_context_create(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args){
    (void)args;if(!out||!p)return -1;*out=calloc(1,sizeof(**out));if(!*out)return -1;(*out)->p=p;return 0;
}
int mpv_render_context_render(mpv_render_context *c,const mpv_render_param *args){
    (void)args;if(!c||!ui_renderer)return -1;mpv_handle *p=c->p;
    SDL_SetRenderDrawColor(ui_renderer,0,0,0,255);SDL_RenderClear(ui_renderer);
    if(c->generation!=p->generation){if(c->texture)SDL_DestroyTexture(c->texture);c->texture=NULL;c->serial=0;c->generation=p->generation;}
    SDL_LockMutex(p->lock);
    if(p->frame&&p->serial&&p->serial!=c->serial){
        SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(p->frame,p->width,p->height,32,p->pitch,SDL_PIXELFORMAT_RGB888);
        SDL_Texture *next=surface?SDL_CreateTextureFromSurface(ui_renderer,surface):NULL;
        if(surface)SDL_FreeSurface(surface);
        if(!next){snprintf(p->error,sizeof(p->error),"VLC SDL texture: %s",SDL_GetError());SDL_UnlockMutex(p->lock);return -1;}
        if(c->texture)SDL_DestroyTexture(c->texture);
        c->texture=next;c->serial=p->serial;c->width=p->width;c->height=p->height;
    }
    SDL_UnlockMutex(p->lock);
    if(!c->texture)return 0;
    double scale=fmin(1280.0/c->width,720.0/c->height);
    SDL_Rect dst={0,0,(int)(c->width*scale),(int)(c->height*scale)};dst.x=(1280-dst.w)/2;dst.y=(720-dst.h)/2;
    return SDL_RenderCopy(ui_renderer,c->texture,NULL,&dst);
}
void mpv_render_context_free(mpv_render_context *c){if(c){if(c->texture)SDL_DestroyTexture(c->texture);free(c);}}
int ps5_kitchen_tracks(mpv_handle *p,int subtitles,MediaTrack *out,int max){
    if(!p||!p->mp||!out)return 0;
    libvlc_track_description_t *head=subtitles?libvlc_video_get_spu_description(p->mp):libvlc_audio_get_track_description(p->mp);
    int selected=subtitles?libvlc_video_get_spu(p->mp):libvlc_audio_get_track(p->mp),n=0;
    for(libvlc_track_description_t *t=head;t&&n<max;t=t->p_next){
        out[n].id=t->i_id;out[n].selected=t->i_id==selected;snprintf(out[n].label,sizeof(out[n].label),"%s",t->psz_name?t->psz_name:"");n++;
    }
    if(head)libvlc_track_description_list_release(head);return n;
}
int ps5_kitchen_select_track(mpv_handle *p,int subtitles,int id){
    if(!p||!p->mp)return 0;return (subtitles?libvlc_video_set_spu(p->mp,id):libvlc_audio_set_track(p->mp,id))==0;
}
