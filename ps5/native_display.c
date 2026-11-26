/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ps5/native_display.h"
#include "platform_paths.h"

#include <stdio.h>

extern void hui_release_splash(void);
extern int sceSystemServiceHideSplashScreen(void);

/* The payload SDL video driver eagerly calls the keyboard/IME services,
   which are unavailable to this native media title. DualSense uses SDL's
   separate joystick driver. Keep the video driver independent of IME. */
struct SDL_VideoDevice;

int __wrap_PS5_Keyboard_Init(void){
    return 0;
}

int __wrap_PS5_Keyboard_Open(void){
    return 0;
}

int __wrap_PS5_Keyboard_PumpEvents(void){
    return 0;
}

SDL_bool __wrap_PS5_HasScreenKeyboardSupport(struct SDL_VideoDevice *device){
    (void)device;
    return SDL_FALSE;
}

void __wrap_PS5_ShowScreenKeyboard(struct SDL_VideoDevice *device,SDL_Window *window){
    (void)device;
    (void)window;
}

void __wrap_PS5_HideScreenKeyboard(struct SDL_VideoDevice *device,SDL_Window *window){
    (void)device;
    (void)window;
}

SDL_bool __wrap_PS5_IsScreenKeyboardShown(struct SDL_VideoDevice *device,SDL_Window *window){
    (void)device;
    (void)window;
    return SDL_FALSE;
}

void ps5_boot_trace(const char *stage,const char *detail){
    FILE *log=fopen(UAPLAY_DATA_ROOT "/native-boot.log","a");
    if(log){
        fprintf(log,"%s%s%s\n",stage,detail?": ":"",detail?detail:"");
        fclose(log);
    }
    fprintf(stderr,"[UAPlay PS5] %s%s%s\n",stage,detail?": ":"",detail?detail:"");
}

SDL_Window *ps5_open_display(SDL_Renderer **renderer){
    FILE *log=fopen(UAPLAY_DATA_ROOT "/native-boot.log","w");
    if(log){
        fputs("Entered native PS5 display startup\n",log);
        fclose(log);
    }
    *renderer=NULL;
    ps5_boot_trace("Selecting native PS5 VideoOut",NULL);
    if(!SDL_SetHint(SDL_HINT_VIDEODRIVER,"ps5") || SDL_Init(SDL_INIT_VIDEO)<0){
        ps5_boot_trace("SDL video initialization failed",SDL_GetError());
        return NULL;
    }
    ps5_boot_trace("Creating VideoOut window",SDL_GetCurrentVideoDriver());
    SDL_DisplayMode mode;
    if(SDL_GetDesktopDisplayMode(0,&mode)<0){
        ps5_boot_trace("Display mode query failed",SDL_GetError());
        SDL_Quit();
        return NULL;
    }
    char size_message[100];
    snprintf(size_message,sizeof(size_message),"native display %dx%d",mode.w,mode.h);
    ps5_boot_trace("PS5 VideoOut mode",size_message);
    /* Use the actual display dimensions: SDL_RenderSetLogicalSize then maps
       the 1280x720 app coordinate space onto the native video output. */
    /* Prefer native 1920x1080 when exposed by the PS5 SDL VideoOut driver.
       Fallback to its desktop mode if high-resolution allocation fails. */
    SDL_DisplayMode target=mode;
    int display_modes=SDL_GetNumDisplayModes(0);
    for(int i=0;i<display_modes;i++){
        SDL_DisplayMode candidate;
        if(SDL_GetDisplayMode(0,i,&candidate)<0)continue;
        if(candidate.w==1920 && candidate.h==1080){
            target=candidate;
            break;
        }
    }
    /* SDL draws into the VideoOut framebuffer, not an intermediate 720p
       texture. Keep the same logical layout and filter sampled textures. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    SDL_Window *window=SDL_CreateWindow("UAPlay",0,0,target.w,target.h,SDL_WINDOW_SHOWN);
    if(!window && (target.w!=mode.w || target.h!=mode.h)){
        ps5_boot_trace("1080p window unavailable; using desktop mode",SDL_GetError());
        window=SDL_CreateWindow("UAPlay",0,0,mode.w,mode.h,SDL_WINDOW_SHOWN);
    }
    if(!window){
        ps5_boot_trace("Window creation failed",SDL_GetError());
        SDL_Quit();
        return NULL;
    }
    SDL_GetWindowSize(window,&target.w,&target.h);
    snprintf(size_message,sizeof(size_message),"active output %dx%d",target.w,target.h);
    ps5_boot_trace("PS5 UI output",size_message);
    ps5_boot_trace("Creating software renderer",NULL);
    SDL_Renderer *display=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!display || SDL_RenderSetLogicalSize(display,1280,720)<0 ||
       SDL_SetRenderDrawBlendMode(display,SDL_BLENDMODE_BLEND)<0){
        ps5_boot_trace("Renderer setup failed",SDL_GetError());
        if(display)SDL_DestroyRenderer(display);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }
    int output_w=0,output_h=0;
    if(SDL_GetRendererOutputSize(display,&output_w,&output_h)==0){
        snprintf(size_message,sizeof(size_message),"framebuffer %dx%d; logical 1280x720",output_w,output_h);
        ps5_boot_trace("PS5 UI raster resolution",size_message);
        if(output_w<1920 || output_h<1080)
            ps5_boot_trace("1080p unavailable in current VideoOut mode; native-resolution fallback",NULL);
    }
    *renderer=display;
    return window;
}

void ps5_release_splash(void){
    hui_release_splash();
    (void)sceSystemServiceHideSplashScreen();
}
