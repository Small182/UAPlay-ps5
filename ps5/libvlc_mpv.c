/* SPDX-License-Identifier: GPL-3.0-or-later
 * UAPlay native PS5 libVLC adapter. Reuses the catalogue's existing player API.
 * Native PS5 hardware video decoder with SDL video output and software fallback.
 */
#define _POSIX_C_SOURCE 200809L
#include "../ps5/kitchensink_mpv.h"
#include "playback.h"
#include "posters.h"
#include "platform_paths.h"
#include <sys/stat.h>
#include <stdarg.h>
#include <SDL.h>
#include <vlc/vlc.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Upstream VLC-PS5 has weak hooks provided by its standalone player.
 * A PS5 title cannot import undefined weak application symbols, so implement
 * these locally. Cancellation is atomic because VLC calls it on worker threads. */
/* The console fills imported decoder addresses only after loading module 207.
 * Inspect the actual GOT slots rather than calling unresolved imports. */
extern int sceSysmoduleLoadModule(uint16_t id);
extern void vlc_ps5_vdec_prepare(void);
extern void vlc_ps5_vdec_probe(const char *when);
static int hw_ready,hw_checked,hw_active,hw_failed;
#define IMPORT_READY(sym) ({ uintptr_t addr; __asm__ volatile("movq " #sym "@GOTPCREL(%%rip), %0" : "=r"(addr)); addr != 0; })
int vlc_ps5_vdec_available(void){return __atomic_load_n(&hw_ready,__ATOMIC_ACQUIRE);}
void vlc_ps5_vdec_active(int on){__atomic_store_n(&hw_active,on,__ATOMIC_RELEASE);}
void vlc_ps5_vdec_failed(void){__atomic_store_n(&hw_failed,1,__ATOMIC_RELEASE);}
void ps5_vlc_hw_init(void){
    if(hw_checked)return;
    hw_checked=1;
    if(!IMPORT_READY(sceSysmoduleLoadModule))return;
    int rc=sceSysmoduleLoadModule(207);
    if(rc!=0) {fprintf(stderr,"UAPlay: Videodec2 load %#x\n",(unsigned)rc);}
    if(!IMPORT_READY(sceVideodec2QueryComputeMemoryInfo) ||
       !IMPORT_READY(sceVideodec2AllocateComputeQueue) ||
       !IMPORT_READY(sceVideodec2QueryDecoderMemoryInfo) ||
       !IMPORT_READY(sceVideodec2CreateDecoder) ||
       !IMPORT_READY(sceVideodec2DeleteDecoder) ||
       !IMPORT_READY(sceVideodec2Decode) ||
       !IMPORT_READY(sceVideodec2Flush) ||
       !IMPORT_READY(sceVideodec2Reset) ||
       !IMPORT_READY(sceVideodec2GetPictureInfo)){
        fprintf(stderr,"UAPlay: Videodec2 unavailable; software decode\n");
        return;
    }
    vlc_ps5_vdec_prepare();
    vlc_ps5_vdec_probe("UAPlay sandbox");
    __atomic_store_n(&hw_ready,1,__ATOMIC_RELEASE);
}
void vlc_ps5_copy_plane_shift(uint8_t *dst,size_t dp,const uint8_t *src,size_t sp,
                             size_t bytes,unsigned rows,unsigned shift){
    for(unsigned y=0;y<rows;y++,dst+=dp,src+=sp){
        if(!shift)memcpy(dst,src,bytes);
        else for(size_t i=0;i<bytes/2;i++)
            ((uint16_t*)dst)[i]=(uint16_t)(((const uint16_t*)src)[i]<<shift);
    }
}
void vlc_ps5_split_plane16(uint8_t *u,size_t up,uint8_t *v,size_t vp,
                           const uint8_t *src,size_t sp,size_t bytes,unsigned rows){
    for(unsigned y=0;y<rows;y++,u+=up,v+=vp,src+=sp)
        for(size_t i=0;i<bytes/4;i++){
            ((uint16_t*)u)[i]=((const uint16_t*)src)[2*i];
            ((uint16_t*)v)[i]=((const uint16_t*)src)[2*i+1];
        }
}
static long next_open_serial;
static long cancelled_serial;
int vlc_ps5_stream_cancelled(long serial){
    return serial>0 && serial<=__atomic_load_n(&cancelled_serial,__ATOMIC_ACQUIRE);
}
int vlc_ps5_log_details(void){return 0;}
/* Large picture allocations use the upstream libps5compat implementation. */
/* CPU renderer: copy only visible rows, including their distinct pitches. */
void vlc_ps5_copy_plane(uint8_t *dst,size_t dst_pitch,const uint8_t *src,
                        size_t src_pitch,size_t bytes,unsigned rows){
    for(unsigned i=0;i<rows;i++){
        memcpy(dst,src,bytes);
        dst+=dst_pitch;src+=src_pitch;
    }
}
static char init_error[256]="VLC: initialization not attempted";
static void init_status(const char *stage,const char *detail){
    snprintf(init_error,sizeof(init_error),"VLC %s%s%s",stage,detail&&*detail?": ":"",detail&&*detail?detail:"");
    fprintf(stderr,"%s\n",init_error);
    FILE *f=fopen(UAPLAY_DATA_ROOT "/vlc-init.log","a");
    if(f){fprintf(f,"%s\n",init_error);fclose(f);}
}
static SDL_SpinLock vlc_log_lock;
static char vlc_video_warning[128];
static void vlc_console_log(void *opaque,int level,const libvlc_log_t *ctx,const char *fmt,va_list args){
    (void)opaque;(void)ctx;
    if(level<LIBVLC_WARNING)return;
    char line[256];vsnprintf(line,sizeof(line),fmt,args);
    const char *module=NULL;
    libvlc_log_get_context(ctx,&module,NULL,NULL);
    if(level>=LIBVLC_ERROR || (module && (strstr(module,"vout")||strstr(module,"vmem")||strstr(module,"decoder")||
                  strstr(module,"avcodec")||strstr(module,"ps5vdec")||strstr(module,"swscale")||
                  strstr(module,"video")||strstr(module,"chroma")))){
        SDL_AtomicLock(&vlc_log_lock);
        snprintf(vlc_video_warning,sizeof(vlc_video_warning),"%.20s: %.100s",module?module:"VLC",line);
        SDL_AtomicUnlock(&vlc_log_lock);
    }
}
/* Native PS5 audio output (same PCM format and grain as upstream VLC). */
extern int sceAudioOutInit(void);
extern int sceAudioOutOpen(int32_t user,int32_t type,int32_t index,uint32_t len,uint32_t freq,uint32_t param);
extern int sceAudioOutOutput(int32_t handle,const void *buf);
extern int sceAudioOutClose(int32_t handle);
extern void *vlc_ps5_big_alloc(size_t bytes);
extern int vlc_ps5_big_free(void *ptr);
#define AUDIO_RATE 48000u
#define AUDIO_GRAIN 256u
#define AUDIO_CAP (4u*AUDIO_RATE)
struct mpv_handle {
    libvlc_instance_t *vlc;
    libvlc_media_player_t *mp;
    SDL_mutex *lock;
    SDL_mutex *audio_lock;
    SDL_Thread *audio_thread;
    int audio_port,audio_running,audio_paused,audio_gain;
    int16_t *pcm;
    unsigned audio_head,audio_count,audio_packets;
    int64_t audio_pts;
    unsigned char *frame,*ready_frame;
    size_t frame_size,offsets[3];
    unsigned width,height,pitches[3],lines[3],serial,decoded,generation;
    unsigned ten_bit,two_planes,full_range,p010,frame_hw;
    Uint32 playing_since,opened_at,stats_at;
    unsigned setup_count,display_count,setup_ok,setup_fail;
    int video_tracks,core_decoded,core_lost,core_shown;
    char source_chroma[5];
    libvlc_time_t last_video_time;
    char chroma[5];
    int active,started,loaded,volume;
    long open_serial;
    int software_retry;
    unsigned quality_height; /* zero = highest available up to 1080p */
    libvlc_time_t resume_at;
    char ua[160],referer[512],error[256],diag[160];
};
struct mpv_render_context {
    mpv_handle *p;
    SDL_Texture *texture;
    unsigned width,height,serial,generation;
    uint32_t *rgb;
    size_t rgb_capacity;
    struct SwsContext *scaler;
};

static void frame_free(void *ptr){
    if(ptr&&!vlc_ps5_big_free(ptr))free(ptr);
}
static void *frame_alloc(size_t size){
    void *ptr=vlc_ps5_big_alloc(size);
    if(!ptr&&posix_memalign(&ptr,64,size)!=0)ptr=NULL;
    return ptr;
}
static void video_buffers_free(mpv_handle *p){
    frame_free(p->frame);frame_free(p->ready_frame);
    p->frame=p->ready_frame=NULL;p->frame_size=0;
    p->serial=p->decoded=0;p->frame_hw=0;
}
static unsigned video_setup(void **opaque,char *chroma,unsigned *w,unsigned *h,
                            unsigned *pitch,unsigned *lines){
    mpv_handle *p=*opaque;
    if(!*w||!*h||*w>4096||*h>2304)return 0;
    char original[5]={0};memcpy(original,chroma,4);
    unsigned nv12=!memcmp(chroma,"NV12",4),p010=!memcmp(chroma,"P010",4);
    unsigned ten=p010||!memcmp(chroma,"I0A",3)||!memcmp(chroma,"I2A",3)||!memcmp(chroma,"I4A",3);
    unsigned two=nv12||p010,full=!memcmp(chroma,"J4",2);
    memcpy(chroma,two?(ten?"P010":"NV12"):ten?"I0AL":full?"J420":"I420",4);
    unsigned bpp=ten?2:1,cw=(*w+1)/2,ch=(*h+1)/2;
    SDL_LockMutex(p->lock);
    video_buffers_free(p);
    p->setup_count++;
    memcpy(p->source_chroma,original,5);
    p->width=*w;p->height=*h;p->ten_bit=ten;p->two_planes=two;
    p->full_range=full;p->p010=two&&ten;
    memcpy(p->chroma,chroma,4);p->chroma[4]=0;
    pitch[0]=(*w*bpp+255)&~255u;lines[0]=(*h+15)&~15u;
    pitch[1]=(cw*bpp*(two?2:1)+255)&~255u;lines[1]=(ch+15)&~15u;
    pitch[2]=two?0:pitch[1];lines[2]=two?0:lines[1];
    size_t size=0;
    for(unsigned i=0;i<3;i++){
        p->pitches[i]=pitch[i];p->lines[i]=lines[i];p->offsets[i]=size;
        size+=(size_t)pitch[i]*lines[i];
    }
    p->frame_size=size;
    p->frame=frame_alloc(size);p->ready_frame=frame_alloc(size);
    int ok=p->frame&&p->ready_frame;
    if(!ok){p->setup_fail++;video_buffers_free(p);}else p->setup_ok++;
    p->generation++;
    snprintf(p->diag,sizeof(p->diag),"VLC %s %.4s %ux%u%s",
             __atomic_load_n(&hw_active,__ATOMIC_ACQUIRE)?"HW":"SW",chroma,*w,*h,ok?"":" alloc failed");
    if(!ok)snprintf(p->error,sizeof(p->error),"VLC video buffer allocation failed");
    SDL_UnlockMutex(p->lock);
    return ok?1:0;
}
static void video_cleanup(void *opaque){
    mpv_handle *p=opaque;SDL_LockMutex(p->lock);
    video_buffers_free(p);p->generation++;SDL_UnlockMutex(p->lock);
}
static void *video_lock(void *opaque,void **planes){
    mpv_handle *p=opaque;
    SDL_LockMutex(p->lock);
    for(unsigned i=0;i<(p->two_planes?2u:3u);i++)planes[i]=p->frame+p->offsets[i];
    return p;
}
static void video_unlock(void *opaque,void *picture,void *const *planes){
    (void)picture;(void)planes;mpv_handle *p=opaque;
    if(p->frame&&p->ready_frame){
        /* VLC has finished Prepare: the frame is valid even if Display is
           skipped by its clock. Swap, don't copy or wait on Display. */
        unsigned char *prev=p->ready_frame;p->ready_frame=p->frame;p->frame=prev;
        p->frame_hw=__atomic_load_n(&hw_active,__ATOMIC_ACQUIRE)>0;
        p->decoded++;p->serial++;
    }
    SDL_UnlockMutex(p->lock);
}
static void video_display(void *opaque,void *picture){
    (void)picture;mpv_handle *p=opaque;
    SDL_LockMutex(p->lock);p->display_count++;SDL_UnlockMutex(p->lock);
}
static void audio_clear(mpv_handle *p){
    if(!p->audio_lock)return;
    SDL_LockMutex(p->audio_lock);p->audio_head=p->audio_count=0;p->audio_pts=0;
    SDL_UnlockMutex(p->audio_lock);
}
static int audio_setup(void **opaque,char *format,unsigned *rate,unsigned *channels){
    mpv_handle *p=*opaque;
    memcpy(format,"S16N",4);*rate=AUDIO_RATE;*channels=2;
    audio_clear(p);return 0;
}
static void audio_play(void *opaque,const void *samples,unsigned count,int64_t pts){
    mpv_handle *p=opaque;
    if(!p->pcm||!p->audio_lock||!samples)return;
    const int16_t *src=samples;
    if(count>AUDIO_CAP){src+=(size_t)(count-AUDIO_CAP)*2;pts+=(int64_t)(count-AUDIO_CAP)*1000000/AUDIO_RATE;count=AUDIO_CAP;}
    SDL_LockMutex(p->audio_lock);
    if(p->audio_count&&p->audio_count+count>AUDIO_CAP){
        unsigned skip=p->audio_count+count-AUDIO_CAP;
        p->audio_head=(p->audio_head+skip)%AUDIO_CAP;p->audio_count-=skip;
        p->audio_pts+=(int64_t)skip*1000000/AUDIO_RATE;
    }
    if(!p->audio_count)p->audio_pts=pts;
    unsigned tail=(p->audio_head+p->audio_count)%AUDIO_CAP;
    unsigned first=count<AUDIO_CAP-tail?count:AUDIO_CAP-tail;
    memcpy(p->pcm+(size_t)tail*2,src,(size_t)first*4);
    memcpy(p->pcm,src+(size_t)first*2,(size_t)(count-first)*4);
    p->audio_count+=count;p->audio_packets++;
    SDL_UnlockMutex(p->audio_lock);
}
static void audio_pause(void *opaque,int64_t pts){
    (void)pts;mpv_handle *p=opaque;SDL_LockMutex(p->audio_lock);p->audio_paused=1;SDL_UnlockMutex(p->audio_lock);
}
static void audio_resume(void *opaque,int64_t pts){
    (void)pts;mpv_handle *p=opaque;SDL_LockMutex(p->audio_lock);p->audio_paused=0;SDL_UnlockMutex(p->audio_lock);
}
static void audio_flush(void *opaque,int64_t pts){(void)pts;audio_clear(opaque);}
static void audio_drain(void *opaque){(void)opaque;}
static void audio_volume(void *opaque,float volume,bool mute){
    mpv_handle *p=opaque;
    int gain=mute?0:(int)(volume*100.0f);
    if(gain<0)gain=0;if(gain>200)gain=200;
    __atomic_store_n(&p->audio_gain,gain,__ATOMIC_RELEASE);
}
static int audio_worker(void *opaque){
    mpv_handle *p=opaque;
    int16_t output[AUDIO_GRAIN*2];
    while(__atomic_load_n(&p->audio_running,__ATOMIC_ACQUIRE)){
        memset(output,0,sizeof(output));
        SDL_LockMutex(p->audio_lock);
        unsigned at=0;
        if(!p->audio_paused&&p->audio_count){
            int64_t now=libvlc_clock()+2LL*AUDIO_GRAIN*1000000/AUDIO_RATE;
            int64_t delta=p->audio_pts-now;
            if(delta>15000){
                int64_t wait=delta*AUDIO_RATE/1000000;
                at=wait>AUDIO_GRAIN?AUDIO_GRAIN:(unsigned)wait;
            }else if(delta<-60000){
                int64_t late=(-delta)*AUDIO_RATE/1000000;
                unsigned skip=late>p->audio_count?p->audio_count:(unsigned)late;
                p->audio_head=(p->audio_head+skip)%AUDIO_CAP;p->audio_count-=skip;
                p->audio_pts+=(int64_t)skip*1000000/AUDIO_RATE;
            }
            int gain=__atomic_load_n(&p->audio_gain,__ATOMIC_ACQUIRE);
            unsigned n=p->audio_count<AUDIO_GRAIN-at?p->audio_count:AUDIO_GRAIN-at;
            for(unsigned i=0;i<n;i++){
                unsigned j=(p->audio_head+i)%AUDIO_CAP;
                for(unsigned ch=0;ch<2;ch++){
                    int v=p->pcm[(size_t)j*2+ch]*gain/100;
                    output[(at+i)*2+ch]=(int16_t)(v>32767?32767:v<-32768?-32768:v);
                }
            }
            p->audio_head=(p->audio_head+n)%AUDIO_CAP;p->audio_count-=n;
            p->audio_pts+=(int64_t)n*1000000/AUDIO_RATE;
        }
        SDL_UnlockMutex(p->audio_lock);
        if(sceAudioOutOutput(p->audio_port,output)<0)SDL_Delay(5);
    }
    return 0;
}
static void audio_start(mpv_handle *p){
    p->pcm=malloc((size_t)AUDIO_CAP*4);
    if(!p->pcm)return;
    sceAudioOutInit();
    p->audio_port=sceAudioOutOpen(0xff,0,0,AUDIO_GRAIN,AUDIO_RATE,1);
    if(p->audio_port<0){init_status("sceAudioOutOpen failed",NULL);return;}
    __atomic_store_n(&p->audio_running,1,__ATOMIC_RELEASE);
    p->audio_thread=SDL_CreateThread(audio_worker,"vlc-pcm",p);
    if(!p->audio_thread)__atomic_store_n(&p->audio_running,0,__ATOMIC_RELEASE);
}
static void stop(mpv_handle *p){
    p->active=0;
    if(p->open_serial>0){
        long prev=__atomic_load_n(&cancelled_serial,__ATOMIC_RELAXED);
        while(prev<p->open_serial &&
              !__atomic_compare_exchange_n(&cancelled_serial,&prev,p->open_serial,
                                           1,__ATOMIC_RELEASE,__ATOMIC_RELAXED)){}
        p->open_serial=0;
    }
    if(p->mp)libvlc_media_player_stop(p->mp);
    audio_clear(p);
    SDL_LockMutex(p->audio_lock);p->audio_packets=0;SDL_UnlockMutex(p->audio_lock);
    p->playing_since=0;p->opened_at=0;p->last_video_time=0;p->stats_at=0;
    p->setup_count=p->display_count=p->setup_ok=p->setup_fail=0;p->video_tracks=p->core_decoded=p->core_lost=p->core_shown=0;
    SDL_AtomicLock(&vlc_log_lock);vlc_video_warning[0]=0;SDL_AtomicUnlock(&vlc_log_lock);
    p->loaded=p->started=0;
    __atomic_store_n(&hw_active,0,__ATOMIC_RELEASE);
    __atomic_store_n(&hw_failed,0,__ATOMIC_RELEASE);
    p->generation++;
}
mpv_handle *mpv_create(void){
    mpv_handle *p=calloc(1,sizeof(*p));
    if(!p){init_status("calloc","out of memory");return NULL;}
    p->lock=SDL_CreateMutex();p->audio_lock=SDL_CreateMutex();
    p->volume=p->audio_gain=100;p->audio_port=-1;
    if(!p->lock||!p->audio_lock){init_status("SDL_CreateMutex",SDL_GetError());
        if(p->lock)SDL_DestroyMutex(p->lock);if(p->audio_lock)SDL_DestroyMutex(p->audio_lock);
        free(p);return NULL;}return p;
}
int mpv_initialize(mpv_handle *p){
    if(!p){init_status("initialize","null handle");return -1;}
    mkdir(UAPLAY_DATA_ROOT,0777);
    mkdir(UAPLAY_DATA_ROOT "/cache",0777);
    mkdir(UAPLAY_DATA_ROOT "/.cache",0777);
    setenv("HOME",UAPLAY_DATA_ROOT,1);
    setenv("TMPDIR",UAPLAY_DATA_ROOT "/cache",1);
    /* Check the actual allocation route before creating VLC frame pools. */
    extern void *__wrap_aligned_alloc(size_t alignment,size_t size);
    void *test=__wrap_aligned_alloc(64,512);
    if(!test || ((uintptr_t)test&63u)){free(test);init_status("aligned_alloc failed",NULL);return -1;}
    free(test);
    init_status("hardware probe",NULL);
    ps5_vlc_hw_init();
    /* Match the pinned VLC-PS5 player's options. The static ps5vdec plugin
       enables hardware decoding by default; no explicit flag is required. */
    const char *args[]={
        "--ignore-config","--no-video-title-show","--no-sub-autodetect-file",
        "--aout=adummy","--adaptive-use-access","--subsdec-encoding=UTF-8",
        "--gnutls-dir-trust=/app0/certs","--no-gnutls-system-trust",
        "--freetype-font=/app0/fonts/subtitle.ttf",
        "--ssa-fontsdir=/app0/fonts/fallback","--stats","--no-drop-late-frames","--no-skip-frames","--verbose=2"
    };
    init_status("libvlc_new",NULL);
    p->vlc=libvlc_new(sizeof(args)/sizeof(args[0]),args);
    if(!p->vlc){
        init_status("libvlc_new failed",libvlc_errmsg());
        libvlc_clearerr();
        const char *minimal[]={"--ignore-config","--verbose=2"};
        p->vlc=libvlc_new(2,minimal);
        if(!p->vlc){
            init_status("libvlc_new (minimal) failed",libvlc_errmsg());
            return -1;
        }
        init_status("minimal options accepted",NULL);
    }
    libvlc_log_set(p->vlc,vlc_console_log,NULL);
    libvlc_set_user_agent(p->vlc,"UAPlay","VLC/3.0.24 PS5");
    init_status("libvlc_media_player_new",NULL);
    p->mp=libvlc_media_player_new(p->vlc);
    if(!p->mp){init_status("libvlc_media_player_new failed",libvlc_errmsg());return -1;}
    libvlc_video_set_callbacks(p->mp,video_lock,video_unlock,video_display,p);
    libvlc_video_set_format_callbacks(p->mp,video_setup,video_cleanup);
    libvlc_audio_set_callbacks(p->mp,audio_play,audio_pause,audio_resume,audio_flush,audio_drain,p);
    libvlc_audio_set_format_callbacks(p->mp,audio_setup,NULL);
    libvlc_audio_set_volume_callback(p->mp,audio_volume);
    audio_start(p);
    snprintf(p->diag,sizeof(p->diag),"libVLC %s",libvlc_get_version());
    init_status("ready",p->diag);
    return 0;
}
int mpv_set_option_string(mpv_handle *p,const char *n,const char *v){(void)p;(void)n;(void)v;return 0;}
int ps5_kitchen_set_volume(mpv_handle *p,double v){
    if(!p||!p->mp)return 0;p->volume=(int)v;__atomic_store_n(&p->audio_gain,p->volume,__ATOMIC_RELEASE);return 1;
}
int mpv_set_property_string(mpv_handle *p,const char *n,const char *v){
    if(!p||!p->mp||!n||!v)return -1;
    if(!strcmp(n,"options/hls-bitrate")||!strcmp(n,"hls-bitrate")){
        if(!strcmp(v,"max")||!strcmp(v,"auto"))p->quality_height=0;
        else if(!strcmp(v,"800000"))p->quality_height=360;
        else if(!strcmp(v,"1500000"))p->quality_height=480;
        else if(!strcmp(v,"3000000"))p->quality_height=720;
        else if(!strcmp(v,"6000000"))p->quality_height=1080;
        else return -1;
        return 0;
    }
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
static void apply_stream_quality(mpv_handle *p,libvlc_media_t *m){
    /* VLC 3 adaptive demuxer: choose the best representation that fits
       the requested resolution. These are caps, not promises of a variant.
       Progressive MP4 and single-variant HLS remain unchanged. */
    char option[64];
    unsigned height=p->quality_height?p->quality_height:1080;
    libvlc_media_add_option_flag(m,":adaptive-logic=highest",libvlc_media_option_trusted);
    snprintf(option,sizeof(option),":adaptive-maxheight=%u",height);
    libvlc_media_add_option_flag(m,option,libvlc_media_option_trusted);
    libvlc_media_add_option_flag(m,":adaptive-maxwidth=1920",libvlc_media_option_trusted);
}
int mpv_command_async(mpv_handle *p,unsigned long long userdata,const char **a){
    (void)userdata;if(!p||!p->mp||!a||!a[0])return -1;
    if(!strcmp(a[0],"loadfile")&&a[1]){
        stop(p);p->error[0]=0;p->software_retry=0;p->resume_at=0;
        libvlc_media_t *m=strstr(a[1],"://")?libvlc_media_new_location(p->vlc,a[1]):libvlc_media_new_path(p->vlc,a[1]);
        if(!m){snprintf(p->error,sizeof(p->error),"libVLC: unable to create media");return -1;}
        char opt[600];
        p->open_serial=__atomic_add_fetch(&next_open_serial,1,__ATOMIC_RELAXED);
        snprintf(opt,sizeof(opt),":ps5http-serial=%ld",p->open_serial);
        libvlc_media_add_option(m,opt);
        if(p->ua[0]){snprintf(opt,sizeof(opt),":http-user-agent=%s",p->ua);libvlc_media_add_option(m,opt);}
        if(p->referer[0]){snprintf(opt,sizeof(opt),":http-referrer=%s",p->referer);libvlc_media_add_option(m,opt);}
        apply_stream_quality(p,m);
        libvlc_media_player_set_media(p->mp,m);libvlc_media_release(m);
        p->active=1;p->opened_at=SDL_GetTicks();snprintf(p->diag,sizeof(p->diag),"VLC opening");
        audio_resume(p,0);
        int rc=libvlc_media_player_play(p->mp);
        if(rc<0)snprintf(p->error,sizeof(p->error),"libVLC: %s",libvlc_errmsg()?libvlc_errmsg():"unable to start playback");
        return rc;
    }
    if(!strcmp(a[0],"stop")){stop(p);return 0;}
    if(!strcmp(a[0],"cycle")&&a[1]&&!strcmp(a[1],"pause")){libvlc_media_player_pause(p->mp);return 0;}
    if(!strcmp(a[0],"seek")&&a[1]){
        double seconds=strtod(a[1],NULL);
        if(!isfinite(seconds)||fabs(seconds)>864000.0||!p->loaded||
           !libvlc_media_player_is_seekable(p->mp))return -1;
        libvlc_time_t t=(libvlc_time_t)(seconds*1000.0);
        if(!a[2]||strncmp(a[2],"absolute",8)){
            libvlc_time_t current=libvlc_media_player_get_time(p->mp);
            if(current<0)return -1;
            t+=current;
        }
        libvlc_time_t duration=libvlc_media_player_get_length(p->mp);
        if(t<0)t=0;
        if(duration>0&&t>=duration)t=duration>1000?duration-1000:0;
        libvlc_media_player_set_time(p->mp,t);return 0;
    }
    if(!strcmp(a[0],"set")&&a[1]&&a[2])return mpv_set_property_string(p,a[1],a[2]);
    if(!strcmp(a[0],"sub-add")&&a[1])return libvlc_media_player_add_slave(p->mp,libvlc_media_slave_type_subtitle,a[1],1);
    return -1;
}
mpv_event *mpv_wait_event(mpv_handle *p,double timeout){
    (void)timeout;static mpv_event e;static mpv_event_end_file end;
    e.event_id=MPV_EVENT_NONE;e.data=NULL;if(!p||!p->active)return &e;
    libvlc_state_t state=libvlc_media_player_get_state(p->mp);
    if((state==libvlc_Ended||state==libvlc_Error)&&!p->loaded &&
       p->opened_at&&SDL_GetTicks()-p->opened_at>=1500 &&
       !p->software_retry&&vlc_ps5_vdec_available())
        __atomic_store_n(&hw_failed,1,__ATOMIC_RELEASE);
    if(__atomic_exchange_n(&hw_failed,0,__ATOMIC_ACQ_REL) && !p->software_retry){
        libvlc_media_t *old=libvlc_media_player_get_media(p->mp);
        char *url=old?libvlc_media_get_mrl(old):NULL;
        if(old)libvlc_media_release(old);
        libvlc_time_t pos=p->loaded?p->last_video_time:0;
        libvlc_time_t length=libvlc_media_player_get_length(p->mp);
        if(length>0&&pos>=length-30000)pos=0;
        if(url){
            stop(p);
            p->software_retry=1;
            p->resume_at=pos>0?pos:0;
            libvlc_media_t *m=libvlc_media_new_location(p->vlc,url);
            libvlc_free(url);
            if(m){
                libvlc_media_add_option_flag(m,":no-ps5vdec",libvlc_media_option_trusted);
                libvlc_media_add_option_flag(m,":avcodec-hw=none",libvlc_media_option_trusted);
                libvlc_media_add_option_flag(m,":no-drop-late-frames",libvlc_media_option_trusted);
                libvlc_media_add_option_flag(m,":no-skip-frames",libvlc_media_option_trusted);
                char opt[600];
                p->open_serial=__atomic_add_fetch(&next_open_serial,1,__ATOMIC_RELAXED);
                snprintf(opt,sizeof(opt),":ps5http-serial=%ld",p->open_serial);
                libvlc_media_add_option(m,opt);
                if(p->ua[0]){snprintf(opt,sizeof(opt),":http-user-agent=%s",p->ua);libvlc_media_add_option(m,opt);}
                if(p->referer[0]){snprintf(opt,sizeof(opt),":http-referrer=%s",p->referer);libvlc_media_add_option(m,opt);}
                apply_stream_quality(p,m);
        libvlc_media_player_set_media(p->mp,m);libvlc_media_release(m);
                p->active=1;p->opened_at=SDL_GetTicks();
                snprintf(p->diag,sizeof(p->diag),"VLC hardware failed; software retry");
                audio_resume(p,0);
                if(libvlc_media_player_play(p->mp)==0)return &e;
            }
            p->active=0;
            snprintf(p->error,sizeof(p->error),"VLC software fallback failed");
            end.reason=MPV_END_FILE_REASON_ERROR;end.error=-1;
            e.event_id=MPV_EVENT_END_FILE;e.data=&end;return &e;
        }
    }
    libvlc_state_t s=libvlc_media_player_get_state(p->mp);
    if(p->loaded&&s==libvlc_Playing){
        libvlc_time_t now=libvlc_media_player_get_time(p->mp);
        if(now>=0)p->last_video_time=now;
        if(p->resume_at>0&&libvlc_media_player_is_seekable(p->mp)){
            libvlc_time_t length=libvlc_media_player_get_length(p->mp);
            if(length>0&&p->resume_at<length-30000)
                libvlc_media_player_set_time(p->mp,p->resume_at);
            p->resume_at=0;
        }
    }
    if(!p->started){p->started=1;e.event_id=MPV_EVENT_START_FILE;return &e;}
    if(s==libvlc_Error||s==libvlc_Ended){
        if(!p->loaded&&p->opened_at&&SDL_GetTicks()-p->opened_at<1500)return &e;
        libvlc_time_t length=libvlc_media_player_get_length(p->mp);
        int premature=!p->loaded||(length>30000&&p->last_video_time+15000<length);
        int failed=s==libvlc_Error||premature;
        if(failed&&!p->error[0])
            snprintf(p->error,sizeof(p->error),"VLC %s: %s at %.1fs / %.1fs, %u frames",
                p->software_retry?"SW":"HW/auto",s==libvlc_Error?"error":"early EOF",
                p->last_video_time/1000.0,length/1000.0,p->decoded);
        p->active=0;end.reason=failed?MPV_END_FILE_REASON_ERROR:MPV_END_FILE_REASON_EOF;end.error=failed?-1:0;
        e.event_id=MPV_EVENT_END_FILE;e.data=&end;return &e;
    }
    if(!p->loaded){
        Uint32 now=SDL_GetTicks();
        if(!p->stats_at||now-p->stats_at>=1000){
            p->stats_at=now;
            p->video_tracks=libvlc_video_get_track_count(p->mp);
            libvlc_media_t *m=libvlc_media_player_get_media(p->mp);
            libvlc_media_stats_t st={0};
            if(m){
                if(libvlc_media_get_stats(m,&st)){
                    p->core_decoded=st.i_decoded_video;
                    p->core_shown=st.i_displayed_pictures;
                    p->core_lost=st.i_lost_pictures;
                }
                libvlc_media_release(m);
            }
        }
        SDL_LockMutex(p->lock);unsigned frames=p->serial;SDL_UnlockMutex(p->lock);
        SDL_LockMutex(p->audio_lock);unsigned audio=p->audio_packets;SDL_UnlockMutex(p->audio_lock);
        if((s==libvlc_Playing||audio>0)&&!p->playing_since)p->playing_since=now;
        if(!frames&&p->playing_since&&now-p->playing_since>8000 && !p->software_retry){
            /* A failed OpenDecoder may never announce hw_active. Do not
               wait indefinitely for a callback that was never installed. */
            __atomic_store_n(&hw_failed,1,__ATOMIC_RELEASE);
        }
        if(frames){
            p->loaded=1;e.event_id=MPV_EVENT_FILE_LOADED;
        }else if(p->software_retry&&p->opened_at&&now-p->opened_at>20000){
            snprintf(p->error,sizeof(p->error),"%s",ps5_kitchen_diagnostic(p));
            p->active=0;end.reason=MPV_END_FILE_REASON_ERROR;end.error=-1;
            e.event_id=MPV_EVENT_END_FILE;e.data=&end;
        }
    }
    return &e;
}
int mpv_request_log_messages(mpv_handle *p,const char *level){(void)p;(void)level;return 0;}
const char *mpv_error_string(int code){return code<0?"libVLC playback/render error":"OK";}
const char *ps5_kitchen_last_error(mpv_handle *p){return p?(p->error[0]?p->error:NULL):init_error;}
const char *ps5_kitchen_diagnostic(mpv_handle *p){
    static char details[256];
    if(!p)return "VLC not initialized";
    char warning[128];
    SDL_AtomicLock(&vlc_log_lock);memcpy(warning,vlc_video_warning,sizeof(warning));SDL_AtomicUnlock(&vlc_log_lock);
    SDL_LockMutex(p->lock);
    extern void hui_heap_stats(size_t*,size_t*,size_t*,size_t*);
    size_t live=0,failures=0;hui_heap_stats(&live,NULL,NULL,&failures);
    snprintf(details,sizeof(details),"%s D:%d S:%u/%u P:%u F:%u %.4s H:%zuM E:%zu %.65s",
        p->software_retry?"SW":__atomic_load_n(&hw_active,__ATOMIC_ACQUIRE)?"HW":"AUTO",
        p->core_decoded,p->setup_ok,p->setup_fail,p->decoded,p->display_count,
        p->chroma,live>>20,failures,warning);
    SDL_UnlockMutex(p->lock);
    return details;
}
void mpv_terminate_destroy(mpv_handle *p){
    if(!p)return;stop(p);if(p->mp)libvlc_media_player_release(p->mp);
    if(p->vlc)libvlc_release(p->vlc);
    __atomic_store_n(&p->audio_running,0,__ATOMIC_RELEASE);
    if(p->audio_thread)SDL_WaitThread(p->audio_thread,NULL);
    if(p->audio_port>=0)sceAudioOutClose(p->audio_port);
    free(p->pcm);video_buffers_free(p);SDL_DestroyMutex(p->audio_lock);SDL_DestroyMutex(p->lock);free(p);
}
int mpv_render_context_create(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args){
    (void)args;if(!out||!p){init_status("render context","invalid arguments");return -1;}
    *out=calloc(1,sizeof(**out));if(!*out){init_status("render context","out of memory");return -1;}(*out)->p=p;return 0;
}
const char *ps5_vlc_picture_info(mpv_handle *p){
    static char info[96];
    if(!p)return "";
    SDL_LockMutex(p->lock);
    if(p->serial)snprintf(info,sizeof(info),"%s  %ux%u  %u-bit",
        p->frame_hw?"HW":"SW",p->width,p->height,p->ten_bit?10u:8u);
    else snprintf(info,sizeof(info),"VLC");
    SDL_UnlockMutex(p->lock);
    return info;
}
static unsigned clip8(int v){return v<0?0u:v>255?255u:(unsigned)v;}
static unsigned sample8(const unsigned char *row,unsigned x,int ten,int p010){
    if(!ten)return row[x];
    unsigned v=((const uint16_t*)row)[x];
    return p010?v>>8:v>>2;
}
static void convert_yuv(mpv_handle *p,uint32_t *rgb,unsigned ow,unsigned oh){
    const unsigned char *y=p->ready_frame,*u=y+p->offsets[1],*v=y+p->offsets[2];
    for(unsigned row=0;row<oh;row++){
        unsigned sy=(uint64_t)row*p->height/oh;
        const unsigned char *yr=y+(size_t)sy*p->pitches[0];
        const unsigned char *ur=u+(size_t)(sy/2)*p->pitches[1];
        const unsigned char *vr=v+(size_t)(sy/2)*p->pitches[2];
        for(unsigned col=0;col<ow;col++){
            unsigned sx=(uint64_t)col*p->width/ow,cx=sx/2;
            int yy=(int)sample8(yr,sx,p->ten_bit,p->p010);
            int uu=(int)sample8(ur,p->two_planes?cx*2:cx,p->ten_bit,p->p010)-128;
            int vv=(int)sample8(p->two_planes?ur:vr,p->two_planes?cx*2+1:cx,p->ten_bit,p->p010)-128;
            int base=p->full_range?256*yy:298*(yy-16);
            int r=(base+(p->full_range?403:459)*vv+128)>>8;
            int g=(base-(p->full_range?48:55)*uu-(p->full_range?120:136)*vv+128)>>8;
            int b=(base+(p->full_range?475:541)*uu+128)>>8;
            rgb[(size_t)row*ow+col]=0xff000000u|(clip8(r)<<16)|(clip8(g)<<8)|clip8(b);
        }
    }
}
/* Use FFmpeg's SIMD-capable scaler on the original YUV planes. Keep the
 * earlier converter as a compatibility fallback if swscale rejects a format. */
static void scale_picture(mpv_render_context *c,unsigned w,unsigned h){
    mpv_handle *p=c->p;
    enum AVPixelFormat fmt=p->two_planes?
        (p->ten_bit?AV_PIX_FMT_P010LE:AV_PIX_FMT_NV12):
        (p->ten_bit?AV_PIX_FMT_YUV420P10LE:AV_PIX_FMT_YUV420P);
    c->scaler=sws_getCachedContext(c->scaler,p->width,p->height,fmt,
        w,h,AV_PIX_FMT_BGRA,SWS_BILINEAR|SWS_ACCURATE_RND,NULL,NULL,NULL);
    if(c->scaler){
        const int *coeff=sws_getCoefficients(p->height>=720?SWS_CS_ITU709:SWS_CS_ITU601);
        sws_setColorspaceDetails(c->scaler,coeff,p->full_range,coeff,1,0,1<<16,1<<16);
        const uint8_t *src[4]={p->ready_frame,p->ready_frame+p->offsets[1],
            p->two_planes?NULL:p->ready_frame+p->offsets[2],NULL};
        int stride[4]={(int)p->pitches[0],(int)p->pitches[1],(int)p->pitches[2],0};
        uint8_t *dst[4]={(uint8_t*)c->rgb,NULL,NULL,NULL};
        int out_stride[4]={(int)w*4,0,0,0};
        if(sws_scale(c->scaler,src,stride,0,p->height,dst,out_stride)==(int)h)return;
    }
    convert_yuv(p,c->rgb,w,h);
}
int mpv_render_context_render(mpv_render_context *c,const mpv_render_param *args){
    (void)args;if(!c||!ui_renderer)return -1;mpv_handle *p=c->p;
    SDL_SetRenderDrawColor(ui_renderer,0,0,0,255);SDL_RenderClear(ui_renderer);
    SDL_LockMutex(p->lock);
    if(c->generation!=p->generation){if(c->texture)SDL_DestroyTexture(c->texture);c->texture=NULL;c->serial=0;c->generation=p->generation;}
    if(p->ready_frame&&p->serial&&p->serial!=c->serial){
        /* The UI uses 1280x720 logical coordinates, not output pixels.
         * Keep source resolution up to native 1080p; avoid a 720p round trip. */
        int out_w=1920,out_h=1080;
        if(SDL_GetRendererOutputSize(ui_renderer,&out_w,&out_h)!=0 || out_w<1 || out_h<1){
            out_w=1920;out_h=1080;
        }
        if(out_w>1920)out_w=1920;if(out_h>1080)out_h=1080;
        double scale=fmin(1.0,fmin((double)out_w/p->width,(double)out_h/p->height));
        unsigned w=(unsigned)(p->width*scale),h=(unsigned)(p->height*scale);
        if(!w||!h){SDL_UnlockMutex(p->lock);return -1;}
        size_t pixels=(size_t)w*h;
        if(c->rgb_capacity<pixels){
            uint32_t *next=realloc(c->rgb,pixels*sizeof(*next));
            if(!next){snprintf(p->error,sizeof(p->error),"VLC RGB buffer allocation failed");SDL_UnlockMutex(p->lock);return -1;}
            c->rgb=next;c->rgb_capacity=pixels;
        }
        scale_picture(c,w,h);
        if(!c->texture||c->width!=w||c->height!=h){
            if(c->texture)SDL_DestroyTexture(c->texture);
            c->texture=SDL_CreateTexture(ui_renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
            c->width=w;c->height=h;
        }
        if(c->texture&&SDL_UpdateTexture(c->texture,NULL,c->rgb,w*4)<0){
            SDL_DestroyTexture(c->texture);c->texture=NULL;
        }
        if(!c->texture){
            SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(c->rgb,w,h,32,w*4,SDL_PIXELFORMAT_ARGB8888);
            if(surface){SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_NONE);c->texture=SDL_CreateTextureFromSurface(ui_renderer,surface);SDL_FreeSurface(surface);}
        }
        if(!c->texture){snprintf(p->error,sizeof(p->error),"VLC SDL texture: %s",SDL_GetError());SDL_UnlockMutex(p->lock);return -1;}
        SDL_SetTextureBlendMode(c->texture,SDL_BLENDMODE_NONE);
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(c->texture,SDL_ScaleModeLinear);
#endif
        c->serial=p->serial;
        if(c->serial==1){
            unsigned luma=p->ready_frame[0];
            snprintf(p->diag,sizeof(p->diag),"VLC %s %.4s %ux%u -> %ux%u Y=%u",
                __atomic_load_n(&hw_active,__ATOMIC_ACQUIRE)?"HW":"SW",p->chroma,p->width,p->height,w,h,luma);
            init_status("first picture",p->diag);
        }
    }
    SDL_UnlockMutex(p->lock);
    if(!c->texture)return 0;
    double scale=fmin(1280.0/c->width,720.0/c->height);
    SDL_Rect dst={0,0,(int)(c->width*scale),(int)(c->height*scale)};dst.x=(1280-dst.w)/2;dst.y=(720-dst.h)/2;
    return SDL_RenderCopy(ui_renderer,c->texture,NULL,&dst);
}
void mpv_render_context_free(mpv_render_context *c){if(c){if(c->texture)SDL_DestroyTexture(c->texture);sws_freeContext(c->scaler);free(c->rgb);free(c);}}
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
