/* Host-only adapters. No symbols from this file enter the PS5 build. */
#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <curl/curl.h>
#include "../ps5/native_display.h"
#include "console_curl.h"

void ps5_boot_trace(const char *stage,const char *detail){
    fprintf(stderr,"[Desktop Preview] %s%s%s\n",stage,detail?": ":"",detail?detail:"");
}
SDL_Window *ps5_open_display(SDL_Renderer **renderer){
    *renderer=NULL;
    if(SDL_Init(SDL_INIT_VIDEO)<0)return NULL;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    SDL_Window *w=SDL_CreateWindow("UAPlay PS5 Desktop Preview",SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,1280,720,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!w)return NULL;
    *renderer=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
    if(!*renderer || SDL_RenderSetLogicalSize(*renderer,1280,720)<0 ||
       SDL_SetRenderDrawBlendMode(*renderer,SDL_BLENDMODE_BLEND)<0){
        if(*renderer)SDL_DestroyRenderer(*renderer);
        SDL_DestroyWindow(w);return NULL;
    }
    return w;
}
void ps5_release_splash(void){}
const char *console_curl_ca_file(void){return "/etc/ssl/certs/ca-certificates.crt";}
void console_curl_setup(CURL *c){
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_CAINFO,console_curl_ca_file());
}
int console_curl_nonblocking(int fd){int flags=fcntl(fd,F_GETFL,0);return flags<0?-1:fcntl(fd,F_SETFL,flags|O_NONBLOCK);}
void *__wrap_aligned_alloc(size_t a,size_t n){void *p=NULL;return posix_memalign(&p,a,n)==0?p:NULL;}
void *vlc_ps5_big_alloc(size_t n){return __wrap_aligned_alloc(64,n);}
int vlc_ps5_big_free(void *p){free(p);return 1;}
void hui_heap_stats(size_t *l,size_t *p,size_t *b,size_t *f){if(l)*l=0;if(p)*p=0;if(b)*b=0;if(f)*f=0;}

/* Keep the same PCM callback/clock path; substitute SDL for sceAudioOut. */
int sceAudioOutInit(void){return SDL_InitSubSystem(SDL_INIT_AUDIO);}
int sceAudioOutOpen(int32_t user,int32_t type,int32_t index,uint32_t len,uint32_t freq,uint32_t param){
    (void)user;(void)type;(void)index;(void)param;
    SDL_AudioSpec want={0};want.freq=(int)freq;want.format=AUDIO_S16SYS;want.channels=2;want.samples=(Uint16)len;
    SDL_AudioDeviceID dev=SDL_OpenAudioDevice(NULL,0,&want,NULL,0);
    if(!dev)return -1;
    SDL_PauseAudioDevice(dev,0);return (int)dev;
}
int sceAudioOutOutput(int32_t port,const void *buf){
    SDL_AudioDeviceID dev=(SDL_AudioDeviceID)port;
    while(SDL_GetQueuedAudioSize(dev)>4096)SDL_Delay(2);
    return SDL_QueueAudio(dev,buf,256*2*sizeof(int16_t));
}
int sceAudioOutClose(int32_t port){SDL_CloseAudioDevice((SDL_AudioDeviceID)port);return 0;}
