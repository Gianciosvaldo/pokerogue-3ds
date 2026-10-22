#ifndef FOOTBALL_STATS_H
#define FOOTBALL_STATS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pr_data.h"

typedef enum {
    FB_QB, FB_WR, FB_RB, FB_OL_DL, FB_LB, FB_TE_S, FB_POSITION_COUNT
} FootballPosition;

typedef struct {
    uint8_t spd, agi, str, tak, sta, thp, tha, cth;
} FootballAttributes;

typedef struct {
    uint32_t roster_id; /* Existing roster owns EXP, evolution and inventory. */
    uint16_t species_id;
    uint8_t form_index;
    int8_t primary_type, secondary_type; /* pr_pokemon_type; -1 = absent */
    FootballPosition position;
    FootballAttributes attributes;
} FootballPlayer;

typedef struct { uint8_t stats[6]; } FootballDexMaxima;

/* Supply the entire supported Pokédex/forms, not the drafted roster.
 * All functions leave output unchanged on invalid input. */
bool football_stats_build_maxima(FootballDexMaxima *out,
                                const pr_form *forms, size_t count);
bool football_stats_generate(FootballPlayer *out, uint32_t roster_id,
                            const pr_form *form, const FootballDexMaxima *maxima);
float football_stats_type_affinity(int type, FootballPosition position);
FootballPosition football_stats_best_position(const FootballPlayer *player);
#endif
