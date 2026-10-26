#ifndef FOOTBALL_LIVE_ART_H
#define FOOTBALL_LIVE_ART_H
#include "football_career.h"
#include "football_camera.h"
#include "football_render.h"
void football_live_art_top(FootballRenderer *r,const FootballCareer *career,const FootballCamera *camera,unsigned animation_ms);
void football_live_art_defense_card(FootballRenderer *r,FootballDefenseId id,float x,float y,float w,float h,bool selected);
/* The name of a species for the candy line under a banner (the screen owns the species table). */
const char *football_live_art_species(unsigned species);
/* The label of a kick (FB_SPECIAL_NONE is the two-point try, played as an ordinary play from the 2). */
const char *football_live_art_special_name(FootballSpecial kind);
void football_live_art_special_card(FootballRenderer *r,FootballSpecial kind,const char *detail,float x,float y,float w,float h,bool selected,bool enabled);
void football_live_art_card(FootballRenderer *r,FootballPlayId id,float x,float y,float w,float h,bool selected);
#endif
