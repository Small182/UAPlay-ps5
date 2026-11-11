/*
 * UAPlay PS5 playback bridge. Playback core: the open-source SDL_kitchensink
 * decoder (used by BFplayer) + SDL queued audio + PS5 OpenGL RGBA presentation.
 * No Nuvio/EVO/browser host, no libmpv binary and no external title launch.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "../ps5/kitchensink_mpv.h"
#include "playback.h"

#include <SDL.h>
#include <kitchensink2/kitchensink.h>
#ifdef UAPLAY_PS5_SOFTWARE
#include "posters.h"
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mpv_handle {
    Kit_Source *source;
    Kit_Player *decoder;
    SDL_AudioDeviceID audio;
    int ready, event_stage, eof_sent, user_stopped;
    double volume;
    int error;
    char error_text[256];
    char user_agent[160], referrer[512];
};

struct mpv_render_context {
    mpv_handle *owner;
#ifdef UAPLAY_PS5_SOFTWARE
    SDL_Texture *texture;
#else
    GLuint program, vao, vbo, texture;
#endif
    int width, height;
};

static void audio_tick(mpv_handle *p) {
    if (!p || !p->decoder || !p->audio ||
        Kit_GetPlayerState(p->decoder)!=KIT_PLAYING) return;
    Uint32 queued=SDL_GetQueuedAudioSize(p->audio);
    if (queued>=32768) return;
    Uint8 buffer[8192];
    int bytes=Kit_GetPlayerAudioData(p->decoder,queued,buffer,sizeof(buffer));
    if (bytes<=0 || bytes>(int)sizeof(buffer)) return;
    if (p->volume<99.9) {
        /* Audio output was explicitly requested as signed 16-bit PCM. */
        for (int i=0;i+1<bytes;i+=2) {
            int sample=(int)(int16_t)((unsigned)buffer[i] | ((unsigned)buffer[i+1]<<8));
            sample=(int)(sample*p->volume/100.0);
            if (sample>32767)sample=32767;
            if (sample< -32768)sample=-32768;
            buffer[i]=(Uint8)sample;
            buffer[i+1]=(Uint8)((unsigned)sample>>8);
        }
    }
    SDL_QueueAudio(p->audio,buffer,(Uint32)bytes);
}

static void clear_stream(mpv_handle *p) {
    if (p->audio) {
        SDL_ClearQueuedAudio(p->audio);
        SDL_CloseAudioDevice(p->audio);
        p->audio=0;
    }
    if (p->decoder) {
        Kit_PlayerStop(p->decoder);
        Kit_ClosePlayer(p->decoder);
        p->decoder=NULL;
    }
    if (p->source) {
        Kit_CloseSource(p->source);
        p->source=NULL;
    }
    p->ready=0;
}

mpv_handle *mpv_create(void) {
    mpv_handle *p=calloc(1,sizeof(*p));
    if (p)p->volume=100.0;
    return p;
}
int mpv_initialize(mpv_handle *p) {
    if (!p || Kit_Init(KIT_INIT_NETWORK|KIT_INIT_ASS)) {
        if(p)snprintf(p->error_text,sizeof(p->error_text),"%s",Kit_GetError());
        return -1;
    }
    p->ready=1;
    return 0;
}
int mpv_set_option_string(mpv_handle *p,const char *name,const char *value) {
    /* mpv-specific tuning is intentionally not forwarded to FFmpeg. */
    (void)p;(void)name;(void)value;
    return 0;
}

static int open_stream(mpv_handle *p,const char *url) {
    if(!p || !p->ready || !url || !*url)return -1;
    clear_stream(p);
    p->event_stage=0;p->eof_sent=0;p->error=0;p->user_stopped=0;
    /* Kit_CreateSourceFromUrl may wait for the remote server. Networking, TLS,
       codecs and stream headers still require real-hardware validation. */
    p->source=Kit_CreateSourceFromUrl(url);
    if(!p->source)goto error;
    Kit_VideoFormatRequest video;
    Kit_ResetVideoFormatRequest(&video);
    video.format=SDL_PIXELFORMAT_RGBA32;
    video.width=1280;
    video.height=720;
    Kit_AudioFormatRequest audio;
    Kit_ResetAudioFormatRequest(&audio);
    audio.format=AUDIO_S16SYS;
    audio.is_signed=1;
    audio.bytes=2;
    audio.sample_rate=48000;
    audio.channels=2;
    int v=Kit_GetBestSourceStream(p->source,KIT_STREAMTYPE_VIDEO);
    int a=Kit_GetBestSourceStream(p->source,KIT_STREAMTYPE_AUDIO);
    if(v<0 && a<0)goto error;
    int s=v<0?-1:Kit_GetBestSourceStream(p->source,KIT_STREAMTYPE_SUBTITLE);
    p->decoder=Kit_CreatePlayer(p->source,v,a,s,&video,&audio,1280,720);
    if(!p->decoder)goto error;
    if (a>=0) {
        Kit_PlayerInfo info={0};
        Kit_GetPlayerInfo(p->decoder,&info);
        SDL_AudioSpec desired={0};
        desired.freq=info.audio_format.sample_rate>0?
                     info.audio_format.sample_rate:48000;
        desired.format=AUDIO_S16SYS;
        desired.channels=info.audio_format.channels>0?
                         (Uint8)info.audio_format.channels:2;
        desired.samples=1024;
        p->audio=SDL_OpenAudioDevice(NULL,0,&desired,NULL,0);
        if(!p->audio)fprintf(stderr,"UAPlay: audio unavailable: %s\n",SDL_GetError());
        else SDL_PauseAudioDevice(p->audio,0);
    }
    Kit_PlayerPlay(p->decoder);
    p->ready=1;
    p->event_stage=1;
    return 0;
error:
    snprintf(p->error_text,sizeof(p->error_text),"%s",Kit_GetError());
    fprintf(stderr,"UAPlay PS5 video open: %s\n",p->error_text);
    clear_stream(p);
    p->error=-1;p->event_stage=3;
    return -1;
}

int mpv_set_property_string(mpv_handle *p,const char *name,const char *value) {
    if(!p || !name || !value)return -1;
    if(!strcmp(name,"user-agent")) {
        snprintf(p->user_agent,sizeof(p->user_agent),"%s",value);return 0;
    }
    if(!strcmp(name,"referrer")) {
        snprintf(p->referrer,sizeof(p->referrer),"%s",value);return 0;
    }
    if(!strcmp(name,"pause") && p->decoder) {
        if(!strcmp(value,"yes") || !strcmp(value,"true")){
            Kit_PlayerPause(p->decoder);
            if(p->audio)SDL_PauseAudioDevice(p->audio,1);
        }else{
            Kit_PlayerPlay(p->decoder);
            if(p->audio)SDL_PauseAudioDevice(p->audio,0);
        }
        return 0;
    }
    if(!strcmp(name,"volume"))return ps5_kitchen_set_volume(p,atof(value));
    if((!strcmp(name,"aid") || !strcmp(name,"sid")) && p->decoder) {
        int id=!strcmp(value,"no")?0:atoi(value);
        return ps5_kitchen_select_track(p,!strcmp(name,"sid"),id)?0:-1;
    }
    return -1; /* Do not silently claim unsupported mpv options work. */
}

int mpv_get_property(mpv_handle *p,const char *name,mpv_format format,void *out) {
    if(!p || !name || !out)return -1;
    if(format==MPV_FORMAT_DOUBLE && p->decoder) {
        double result;
        if(!strcmp(name,"duration")) result=Kit_GetPlayerDuration(p->decoder);
        else if(!strcmp(name,"time-pos"))result=Kit_GetPlayerPosition(p->decoder);
        else return -1;
        *(double*)out=isfinite(result)?result:0;
        return 0;
    }
    if(format==MPV_FORMAT_FLAG && !strcmp(name,"pause")) {
        *(int*)out=p->decoder && Kit_GetPlayerState(p->decoder)==KIT_PAUSED;
        return 0;
    }
    return -1;
}

int mpv_command_async(mpv_handle *p,unsigned long long userdata,const char **args) {
    (void)userdata;
    if(!p || !args || !args[0])return -1;
    if(!strcmp(args[0],"loadfile"))return args[1]?open_stream(p,args[1]):-1;
    if(!strcmp(args[0],"cycle") && args[1] && !strcmp(args[1],"pause") && p->decoder){
        return mpv_set_property_string(p,"pause",
            Kit_GetPlayerState(p->decoder)==KIT_PAUSED?"no":"yes");
    }
    if(!strcmp(args[0],"stop")) {
        p->user_stopped=1;clear_stream(p);p->event_stage=0;return 0;
    }
    if(!strcmp(args[0],"seek") && p->decoder && args[1]) {
        double time=atof(args[1]);
        if(!args[2] || strcmp(args[2],"absolute"))time+=Kit_GetPlayerPosition(p->decoder);
        if(time<0)time=0;
        return Kit_PlayerSeek(p->decoder,time)?-1:0;
    }
    if(!strcmp(args[0],"set") && args[1] && args[2])
        return mpv_set_property_string(p,args[1],args[2]);
    return -1;
}

int mpv_request_log_messages(mpv_handle *p,const char *level) {
    (void)p;(void)level;return 0;
}
mpv_event *mpv_wait_event(mpv_handle *p,double timeout) {
    (void)timeout;
    static mpv_event result;
    static mpv_event_end_file end;
    result.event_id=MPV_EVENT_NONE;result.data=NULL;
    if(!p)return &result;
    audio_tick(p);
    if(p->event_stage==1){p->event_stage=2;result.event_id=MPV_EVENT_START_FILE;}
    else if(p->event_stage==2 && p->decoder){
        unsigned int frames=0;
        Kit_GetPlayerVideoBufferState(p->decoder,&frames,NULL,NULL,NULL);
        if(frames>0 || Kit_GetPlayerVideoStream(p->decoder)<0){
            p->event_stage=0;result.event_id=MPV_EVENT_FILE_LOADED;
        }else if(Kit_GetPlayerState(p->decoder)==KIT_STOPPED){
            p->event_stage=3;
        }
    }
    else if(p->event_stage==3){
        p->event_stage=0;end.reason=MPV_END_FILE_REASON_ERROR;end.error=p->error;
        result.event_id=MPV_EVENT_END_FILE;result.data=&end;
    }else if(p->decoder && !p->eof_sent && !p->user_stopped &&
             Kit_GetPlayerState(p->decoder)==KIT_STOPPED) {
        p->eof_sent=1;end.reason=MPV_END_FILE_REASON_EOF;end.error=0;
        result.event_id=MPV_EVENT_END_FILE;result.data=&end;
    }
    return &result;
}
const char *mpv_error_string(int code) {
    return code<0?"Помилка SDL_kitchensink / FFmpeg":"OK";
}
void mpv_terminate_destroy(mpv_handle *p) {
    if(!p)return;
    clear_stream(p);
    if(p->ready)Kit_Quit();
    free(p);
}

#ifdef UAPLAY_PS5_SOFTWARE
int mpv_render_context_create(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args){
    (void)args;
    if(!out || !p || !p->ready)return -1;
    *out=calloc(1,sizeof(**out));
    if(!*out)return -1;
    (*out)->owner=p;
    return 0;
}

int mpv_render_context_render(mpv_render_context *ctx,const mpv_render_param *args){
    (void)args;
    if(!ctx || !ctx->owner || !ui_renderer)return -1;
    SDL_SetRenderDrawColor(ui_renderer,0,0,0,255);
    SDL_RenderClear(ui_renderer);
    mpv_handle *p=ctx->owner;
    if(!p->decoder)return 0;
    unsigned char **planes=NULL;
    int *strides=NULL;
    SDL_Rect area={0};
    int result=0;
    if(Kit_LockPlayerVideoRawFrame(p->decoder,&planes,&strides,&area)==0){
        if(planes && planes[0] && strides && area.w>0 && area.h>0 && strides[0]/4>=area.w){
            if(!ctx->texture || area.w!=ctx->width || area.h!=ctx->height){
                if(ctx->texture)SDL_DestroyTexture(ctx->texture);
                ctx->texture=SDL_CreateTexture(ui_renderer,SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STREAMING,area.w,area.h);
                ctx->width=area.w;
                ctx->height=area.h;
            }
            result=ctx->texture?SDL_UpdateTexture(ctx->texture,NULL,planes[0],strides[0]):-1;
        }
        Kit_UnlockPlayerVideoRawFrame(p->decoder);
    }
    if(result<0 || !ctx->texture)return result;
    SDL_Rect dst={0,0,1280,720};
    if((int64_t)ctx->width*dst.h>(int64_t)ctx->height*dst.w){
        dst.h=(int)((int64_t)dst.w*ctx->height/ctx->width);
        dst.y=(720-dst.h)/2;
    }else{
        dst.w=(int)((int64_t)dst.h*ctx->width/ctx->height);
        dst.x=(1280-dst.w)/2;
    }
    return SDL_RenderCopy(ui_renderer,ctx->texture,NULL,&dst);
}

void mpv_render_context_free(mpv_render_context *ctx){
    if(!ctx)return;
    if(ctx->texture)SDL_DestroyTexture(ctx->texture);
    free(ctx);
}
#else
static GLuint shader(GLenum kind,const char *code) {
    GLuint id=glCreateShader(kind);
    glShaderSource(id,1,&code,NULL);glCompileShader(id);
    GLint good=0;glGetShaderiv(id,GL_COMPILE_STATUS,&good);
    if(!good){
        char message[1024]={0};glGetShaderInfoLog(id,sizeof(message),NULL,message);
        fprintf(stderr,"UAPlay video GLSL: %s\n",message);
        glDeleteShader(id);return 0;
    }
    return id;
}
int mpv_render_context_create(mpv_render_context **out,mpv_handle *p,const mpv_render_param *args) {
    (void)args;
    if(!out || !p || !p->ready)return -1;
    *out=NULL;
    mpv_render_context *ctx=calloc(1,sizeof(*ctx));
    if(!ctx)return -1;
    ctx->owner=p;
    static const char *vs="#version 330 core\n"
        "layout(location=0) in vec2 position;layout(location=1) in vec2 texCoord;"
        "out vec2 coord;void main(){coord=texCoord;gl_Position=vec4(position,0.,1.);}";
    static const char *fs="#version 330 core\n"
        "in vec2 coord;out vec4 color;uniform sampler2D frame;"
        "void main(){color=texture(frame,coord);}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
    if(!v || !f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);free(ctx);return -1;}
    ctx->program=glCreateProgram();
    glAttachShader(ctx->program,v);glAttachShader(ctx->program,f);
    glLinkProgram(ctx->program);glDeleteShader(v);glDeleteShader(f);
    GLint good=0;glGetProgramiv(ctx->program,GL_LINK_STATUS,&good);
    if(!good){glDeleteProgram(ctx->program);free(ctx);return -1;}
    glGenVertexArrays(1,&ctx->vao);
    glGenBuffers(1,&ctx->vbo);
    glGenTextures(1,&ctx->texture);
    glBindTexture(GL_TEXTURE_2D,ctx->texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    *out=ctx;
    return 0;
}

int mpv_render_context_render(mpv_render_context *ctx,const mpv_render_param *args) {
    (void)args;
    if(!ctx || !ctx->owner)return -1;
    mpv_handle *p=ctx->owner;
    glViewport(0,0,1280,720);
    glClearColor(0.f,0.f,0.f,1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if(!p->decoder)return 0;
    unsigned char **planes=NULL;int *strides=NULL;
    SDL_Rect area={0};
    if(Kit_LockPlayerVideoRawFrame(p->decoder,&planes,&strides,&area)==0){
        if(planes && planes[0] && strides && strides[0]>0 && area.w>0 && area.h>0){
            if(area.w!=ctx->width || area.h!=ctx->height){
                ctx->width=area.w;ctx->height=area.h;
                glBindTexture(GL_TEXTURE_2D,ctx->texture);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,ctx->width,ctx->height,
                             0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
            }
            glBindTexture(GL_TEXTURE_2D,ctx->texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,strides[0]/4);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,ctx->width,ctx->height,
                            GL_RGBA,GL_UNSIGNED_BYTE,planes[0]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        }
        Kit_UnlockPlayerVideoRawFrame(p->decoder);
    }
    if(ctx->width<1 || ctx->height<1)return 0;
    float aspect=(float)ctx->width/(float)ctx->height;
    float screen=1280.f/720.f;
    float x=aspect>screen?1.f:aspect/screen;
    float y=aspect>screen?screen/aspect:1.f;
    const float vertices[]={-x,-y,0,1, x,-y,1,1, -x,y,0,0,
                            x,-y,1,1, x,y,1,0, -x,y,0,0};
    GLint old_vao=0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&old_vao);
    glUseProgram(ctx->program);
    glBindVertexArray(ctx->vao);
    glBindBuffer(GL_ARRAY_BUFFER,ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,ctx->texture);
    glUniform1i(glGetUniformLocation(ctx->program,"frame"),0);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray((GLuint)old_vao);
    return 0;
}

void mpv_render_context_free(mpv_render_context *ctx) {
    if(!ctx)return;
    glDeleteTextures(1,&ctx->texture);
    glDeleteBuffers(1,&ctx->vbo);
    glDeleteVertexArrays(1,&ctx->vao);
    glDeleteProgram(ctx->program);
    free(ctx);
}

#endif

int ps5_kitchen_tracks(mpv_handle *p,int subtitles,MediaTrack *out,int max) {
    if(!p || !out || max<1)return 0;
    int count=0;
    if(subtitles){
        out[count].id=0;
        out[count].selected=p->decoder && Kit_GetPlayerSubtitleStream(p->decoder)<0;
        snprintf(out[count].label,sizeof(out[count].label),"Вимкнути субтитри");
        count++;
    }
    if(!p->source || !p->decoder)return count;
    Kit_StreamType type=subtitles?KIT_STREAMTYPE_SUBTITLE:KIT_STREAMTYPE_AUDIO;
    int indices[48]={0};
    int n=Kit_GetSourceStreamList(p->source,type,indices,48);
    if(n<0)n=0;
    int selected=subtitles?Kit_GetPlayerSubtitleStream(p->decoder):
                           Kit_GetPlayerAudioStream(p->decoder);
    for(int i=0;i<n && count<max && i<48;i++){
        MediaTrack *entry=&out[count++];
        entry->id=indices[i]+1; /* MPV compatibility: zero means disabled. */
        entry->selected=indices[i]==selected;
        snprintf(entry->label,sizeof(entry->label),"%s %d%s",
                 subtitles?"Субтитри":"Аудіодоріжка",i+1,
                 entry->selected?" [активна]":"");
    }
    return count;
}
int ps5_kitchen_select_track(mpv_handle *p,int subtitles,int id) {
    if(!p || !p->decoder)return 0;
    Kit_StreamType type=subtitles?KIT_STREAMTYPE_SUBTITLE:KIT_STREAMTYPE_AUDIO;
    return Kit_SetPlayerStream(p->decoder,type,id==0?-1:id-1)==0;
}
int ps5_kitchen_set_volume(mpv_handle *p,double volume) {
    if(!p)return -1;
    if(volume<0)volume=0;if(volume>130)volume=130;
    p->volume=volume;
    return 0;
}
