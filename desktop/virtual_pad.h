#ifndef UAPLAY_PREVIEW_PAD_H
#define UAPLAY_PREVIEW_PAD_H
#include <SDL.h>
SDL_GameController *preview_pad_open(void);
int preview_pad_owns(SDL_GameController *pad);
void preview_pad_close(void);
int preview_poll_event(SDL_Event *event);
int preview_pad_selftest(void);
#endif
