#ifndef FOOTBALL_LIVE_ART_H
#define FOOTBALL_LIVE_ART_H
#include "football_career.h"
#include "football_camera.h"
#include "football_render.h"
void football_live_art_top(FootballRenderer *r,const FootballCareer *career,const FootballCamera *camera,unsigned animation_ms);
void football_live_art_defense_card(FootballRenderer *r,FootballDefenseId id,float x,float y,float w,float h,bool selected);
void football_live_art_card(FootballRenderer *r,FootballPlayId id,float x,float y,float w,float h,bool selected);
#endif
