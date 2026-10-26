#ifndef FOOTBALL_LIVE_FX_H
#define FOOTBALL_LIVE_FX_H
#include "football_career.h"
#include "football_camera.h"
#include "football_render.h"
/* Motion for the moments the engine records in FootballLive.fx (jukes, dives, tackles, sacks, interceptions, fumbles,
 * catches, touchdowns). Everything is drawn from the recorded moment and the match clock (FootballLive.steps), with plain
 * rectangles and the ordinary sprites turned, stretched and lifted: no new assets. */
typedef struct { float dx,dy,angle,sx,sy; } FootballFxPose;
/* +1 when a positive angle tips a sprite clockwise on the screen (citro2d's convention); the one number to flip if a
 * dive ever leans away from where it goes. */
#define FB_FX_TURN 1.0f
/* Where a player is lifted, shifted, turned and stretched from where the engine has them (pixels, radians, factors). */
FootballFxPose football_live_fx_pose(const FootballLive *match,unsigned actor);
/* How far the whole field shakes in pixels (a hit, a turnover, a touchdown). */
void football_live_fx_shake(const FootballLive *match,float *dx,float *dy);
/* Grows a banner for a moment when a big play lands: 1 when nothing is happening. */
float football_live_fx_banner_scale(const FootballLive *match);
/* Dust, speed lines, ghosts, impact stars, rings, popups, the touchdown flash and confetti; drawn above the players. */
void football_live_fx_draw(FootballRenderer *r,const FootballCareer *career,const FootballCamera *camera);
#endif
