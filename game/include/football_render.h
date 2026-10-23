#ifndef FOOTBALL_RENDER_H
#define FOOTBALL_RENDER_H
#include <citro2d.h>
#include "menus.h"
#include "playcall_ui.h"
#include "evolution_ui.h"
#define FB_RENDER_ACTOR_CAP 12
#define FB_RENDER_TEXTURE_BUDGET (4u * 1024u * 1024u)

typedef struct {
    const rr_frame *front_frame; /* Borrowed from existing adversary sprite atlas. */
    float x_yards, y_yards; /* x=0..120 (including end zones), y=0..53 1/3 */
    float movement_x;
    bool away; /* Stationary actors retain home-right / away-left facing. */
} FootballRenderActor;
typedef struct {
    uint16_t home_score, away_score, seconds_left;
    uint8_t down, distance, quarter;
    float ball_x_yards;
    FootballRenderActor actors[FB_RENDER_ACTOR_CAP];
    unsigned actor_count;
} FootballRenderScene;
typedef struct {
    C2D_TextBuf text;
    menu_sprite sprites[FB_RENDER_ACTOR_CAP];
    size_t sprite_bytes[FB_RENDER_ACTOR_CAP], total_sprite_bytes;
} FootballRenderer;

/* After rr_init; renderer should be static/heap, zero initialized.
 * Caller frees it before rr_exit. Loading happens only between matches. */
bool football_render_init(FootballRenderer *renderer);
/* Synchronize GPU work before clearing/reloading borrowed sprite textures. */
void football_render_clear_sprites(FootballRenderer *renderer);
void football_render_free(FootballRenderer *renderer);
bool football_render_load_front(FootballRenderer *renderer, unsigned slot,
                                const menu_mon *mon);
const rr_frame *football_render_front_frame(const FootballRenderer *renderer,
                                            unsigned slot, uint32_t animation_ms);
/* Caller can draw inside its rr frame or use the complete dual-screen wrapper. */
bool football_render_draw_sprite(const rr_frame *front, float x, float y,
                                 float scale, bool faces_left);
void football_render_top(FootballRenderer *renderer, const FootballRenderScene *scene);
void football_render_bottom(FootballRenderer *renderer, const FootballPlaycall *ui);
void football_render_evolution_bottom(FootballRenderer *renderer,
                                       const FootballEvolutionMenu *menu);
void football_render_frame(FootballRenderer *renderer, const FootballRenderScene *scene,
                            const FootballPlaycall *ui);
#endif
