#ifndef UAPLAY_PS5_NATIVE_DISPLAY_H
#define UAPLAY_PS5_NATIVE_DISPLAY_H

#include <SDL.h>

SDL_Window *ps5_open_display(SDL_Renderer **renderer);
void ps5_release_splash(void);
void ps5_boot_trace(const char *stage,const char *detail);

#endif
