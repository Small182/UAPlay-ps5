/* This translation unit lives only in the Windows VLC DLL. */
#include <SDL.h>
#include "posters.h"
SDL_Renderer *ui_renderer;
__declspec(dllexport) void desktop_player_bind(SDL_Renderer *renderer){ui_renderer=renderer;}
