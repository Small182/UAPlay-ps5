#ifndef LAMPA_POSTERS_H
#define LAMPA_POSTERS_H

#if defined(UAPLAY_PS5_SOFTWARE)
#elif defined(__PS5__)
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>
#else
#include <GLES2/gl2.h>
#endif
#include "catalog.h"
#include <SDL.h>

#if defined(__PS4__) || defined(UAPLAY_PS5_SOFTWARE)
#include <stdint.h>
typedef uintptr_t UiTexture;
extern SDL_Renderer *ui_renderer;
#else
typedef GLuint UiTexture;
#endif

UiTexture ui_texture_create(SDL_Surface *rgba);
void ui_texture_destroy(UiTexture texture);

/* Start the background image downloader after SDL and curl initialization. */
void posters_init(void);
/* Returns cached texture or zero while the background download is pending. */
UiTexture poster_texture(const Movie *movie, int *network_budget);
/* Begin fetching the currently visible cards and the next few cards. */
void posters_prefetch(const Movie *movies, int count, int start, int ahead);
void posters_destroy(void);

#endif
