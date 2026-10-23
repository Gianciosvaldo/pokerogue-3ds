#ifndef FOOTBALL_MATCH_VIEW_H
#define FOOTBALL_MATCH_VIEW_H
#include "football_match.h"
#include "football_render.h"
/* Pure snapshot conversion; borrows already-loaded front sprite frames and
 * performs no texture loading or frame ownership operations. */
bool football_match_scene(FootballRenderScene *out, const FootballMatch *match,
                           const FootballRenderer *renderer, uint32_t animation_ms);
#endif
