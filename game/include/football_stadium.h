#ifndef FOOTBALL_STADIUM_H
#define FOOTBALL_STADIUM_H
#include <stdbool.h>
#include "football_camera.h"
#include "football_render.h"
/* The field and everything around it on the top screen, as plain rectangles (no art, no textures): mowed turf in a checker, the two
 * painted end zones with their stripes, goal posts, the midfield ball, a tan apron with each team's bench, the ad boards, and the stands
 * behind both sidelines and both end lines, filled with a crowd. Everything is placed in field yards through the camera, so it scrolls
 * with the play, and is culled to the window: a frame costs a few dozen rectangles in the middle of the field and about a hundred and
 * fifty more where the camera shows a whole stand. Depth z stays under .04, below the yard lines (.04), the players and the effects.
 *
 * `round` (1..7) is the time of day (day, dusk, night) and so how dark the turf and the stands are; `steps` is the match clock, which
 * moves the crowd (never the frame time, so a moment looks the same at any speed); `cheering` makes the whole crowd jump. */
void football_stadium_draw(const FootballCamera *camera,unsigned round,unsigned steps,bool cheering);
#endif
