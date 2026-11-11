/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../ps5/native_display.h"
#include "platform_paths.h"

#include <stdio.h>

extern void hui_release_splash(void);
extern int sceSystemServiceHideSplashScreen(void);

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
    SDL_Window *window=SDL_CreateWindow("UAPlay",0,0,mode.w,mode.h,SDL_WINDOW_SHOWN);
    if(!window){
        ps5_boot_trace("Window creation failed",SDL_GetError());
        SDL_Quit();
        return NULL;
    }
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
    *renderer=display;
    return window;
}

void ps5_release_splash(void){
    hui_release_splash();
    (void)sceSystemServiceHideSplashScreen();
}
