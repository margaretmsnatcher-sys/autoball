#ifndef HUD_H
#define HUD_H

#include "renderer.h"
#include "match.h"
#include <SDL3/SDL.h>

typedef struct {
    SDL_GPUTexture *font_atlas;
    int             atlas_w;
    int             atlas_h;
    int             glyph_w;
    int             glyph_h;
} HUDFont;

typedef struct {
    HUDFont font;
    int     show_debug;
} HUD;

int  hud_init(HUD *hud, Renderer *r);
void hud_shutdown(HUD *hud);
void hud_draw(HUD *hud, Renderer *r, const MatchState *match);

#endif /* HUD_H */
