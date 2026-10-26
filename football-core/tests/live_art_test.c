#include "football_live_art.h"
#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballCareer career;
static FootballRenderer renderer;
static rr_frame frame={.src_w=64,.src_h=64,.w=64,.h=64};
static unsigned drawn;
static bool mirror[22];
void C2D_TextBufClear(C2D_TextBuf b){assert(b);}
void C2D_TextParse(C2D_Text *t,C2D_TextBuf b,const char *s){(void)t;assert(b && s);}
void C2D_TextOptimize(C2D_Text *t){(void)t;}
void C2D_DrawText(const C2D_Text *t,unsigned flags,float x,float y,float z,float sx,float sy,u32 color){(void)t;(void)flags;(void)x;(void)y;(void)z;(void)sx;(void)sy;(void)color;}
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 color){(void)x;(void)y;(void)z;(void)color;assert(isfinite(w) && w>0 && isfinite(h) && h>0);return true;}
const rr_frame *football_render_front_frame(const FootballRenderer *r,unsigned slot,uint32_t ms){(void)r;(void)ms;assert(slot<22);return &frame;}
bool football_render_draw_sprite(const rr_frame *f,float x,float y,float scale,bool flip){(void)x;(void)y;assert(f && scale>0 && drawn<22);mirror[drawn++]=flip;return true;}
int main(void)
{
    renderer.text=&renderer;FootballCamera camera={.x=60,.y=26,.ready=true};
    FootballLive *m=&career.live;m->snap_offense=FB_HOME;m->rules.offense=FB_HOME;m->rules.first_down_at=50;
    /* Same y fixes painter order so the submission index is the actor index. */
    for(unsigned i=0;i<22;++i){m->actors[i].x=60;m->actors[i].y=26;m->actors[i].facing=1;}
    m->actors[2].x=65;m->actors[2].facing=-1;m->actors[0].vx=-4;m->carrier=0;
    football_live_art_top(&renderer,&career,&camera,0);
    assert(drawn==22 && mirror[0] && !mirror[2] && mirror[3]); /* left-native sheet mirrors ONLY to look right */
    drawn=0;m->actors[2].facing=1;m->actors[3].facing=-1;
    football_live_art_top(&renderer,&career,&camera,0);assert(mirror[0] && mirror[2] && !mirror[3]);
    /* Kick plays: the landing marker, the fair-catch hint and every dead-ball banner draw without degenerate rectangles. */
    drawn=0;m->special=FB_SPECIAL_PUNT;m->stage=FB_STAGE_FLIGHT;m->kick_side=FB_AWAY;m->to_x=70;m->to_y=26;m->phase=FB_LIVE_PLAY;
    football_live_art_top(&renderer,&career,&camera,0);
    drawn=0;m->special=FB_SPECIAL_KICKOFF;m->stage=FB_STAGE_FLIGHT;m->kick_side=FB_HOME;m->phase=FB_LIVE_PLAY;football_live_art_top(&renderer,&career,&camera,0);
    m->special=FB_SPECIAL_FIELD_GOAL;m->stage=FB_STAGE_SNAP;
    for(unsigned event=FB_EVENT_NONE;event<=FB_EVENT_KICKOFF;++event){
        for(unsigned returned=0;returned<2;++returned){drawn=0;m->phase=FB_LIVE_DEAD;m->event=(uint8_t)event;m->kick_yards=47;m->return_yards=(int16_t)(returned?23:0);football_live_art_top(&renderer,&career,&camera,0);}
    }
    /* The headline names every kick, and the try is labelled before and during its play. */
    assert(!strcmp(football_live_art_special_name(FB_SPECIAL_KICKOFF),"KICKOFF") && !strcmp(football_live_art_special_name(FB_SPECIAL_EXTRA_POINT),"EXTRA POINT") && !strcmp(football_live_art_special_name(FB_SPECIAL_NONE),"GO FOR TWO"));
    m->special=FB_SPECIAL_NONE;m->phase=FB_LIVE_CALL;m->event=FB_EVENT_NONE;
    for(unsigned conversion=1;conversion<=2;++conversion){drawn=0;m->conversion=(uint8_t)conversion;football_live_art_top(&renderer,&career,&camera,0);}
    m->conversion=0;
    puts("football live art: native-left front sprites, running directions and retreating QB receiver-facing submission passed");
}
