#include "football_live_art.h"
#include "football_live_fx.h"
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
static unsigned named;
const char *football_live_art_species(unsigned species){named++;return species?"BULBASAUR":"?";}
void C2D_TextBufClear(C2D_TextBuf b){assert(b);}
void C2D_TextParse(C2D_Text *t,C2D_TextBuf b,const char *s){(void)t;assert(b && s);}
void C2D_TextOptimize(C2D_Text *t){(void)t;}
void C2D_DrawText(const C2D_Text *t,unsigned flags,float x,float y,float z,float sx,float sy,u32 color){(void)t;(void)flags;(void)x;(void)y;(void)z;(void)sx;(void)sy;(void)color;}
static unsigned rects;
/* The engine records these (football_live_fx); the art only reads them, so the test writes them itself. */
static void put_fx(FootballLive *m,unsigned kind,unsigned actor,unsigned other,int side){m->fx[m->fx_next]=(FootballFx){(uint8_t)kind,(uint8_t)actor,(uint8_t)other,(int8_t)side,m->steps,m->actors[actor].x,m->actors[actor].y};m->fx_next=(uint8_t)((m->fx_next+1)%FB_FX_COUNT);}
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 color){(void)x;(void)y;(void)z;(void)color;assert(isfinite(w) && w>0 && isfinite(h) && h>0 && isfinite(x) && isfinite(y));++rects;return true;}
const rr_frame *football_render_front_frame(const FootballRenderer *r,unsigned slot,uint32_t ms){(void)r;(void)ms;assert(slot<22);return &frame;}
static float last_angle,last_sx=1,last_sy=1,last_x,last_y;
bool football_render_draw_sprite_pose(const rr_frame *f,float x,float y,float scale,bool flip,float sx,float sy,float angle){assert(f && scale>0 && sx>0 && sy>0 && isfinite(x) && isfinite(y) && isfinite(angle) && drawn<22);mirror[drawn++]=flip;last_x=x;last_y=y;last_sx=sx;last_sy=sy;last_angle=angle;return true;}
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
    /* A credited play adds the species line under the banner; a credit for the other side or none adds nothing. */
    m->phase=FB_LIVE_DEAD;m->event=FB_EVENT_INTERCEPTION;named=0;drawn=0;m->credit_count=0;football_live_art_top(&renderer,&career,&camera,0);assert(!named);
    m->credit_count=1;m->credits[0]=(FootballCredit){FB_AWAY,3,FB_CREDIT_INTERCEPTION};drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(!named);
    for(unsigned kind=FB_CREDIT_SACK;kind<=FB_CREDIT_TOUCHDOWN;++kind){m->credits[0]=(FootballCredit){FB_HOME,3,(uint8_t)kind};named=0;drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(named==1);}
    m->credits[0].member=22;named=0;drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(!named);
    m->credit_count=0;
    unsigned worst=0;
    /* Motion: no moment recorded draws the plain scene; every kind of moment stays inside sane bounds all through its life. */
    memset(m->fx,0,sizeof m->fx);m->fx_next=0;m->phase=FB_LIVE_PLAY;m->event=FB_EVENT_NONE;m->steps=1000;
    {FootballFxPose p=football_live_fx_pose(m,3);assert(p.dx==0 && p.dy==0 && p.angle==0 && p.sx==1 && p.sy==1);}
    float sx,sy;football_live_fx_shake(m,&sx,&sy);assert(sx==0 && sy==0 && football_live_fx_banner_scale(m)==1);
    drawn=0;rects=0;football_live_art_top(&renderer,&career,&camera,0);unsigned plain=rects;assert(last_angle==0 && last_sx==1 && last_sy==1);
    for(unsigned kind=FB_FX_JUKE;kind<=FB_FX_TOUCHDOWN;++kind){
        for(int side=-1;side<=1;side+=2){
            memset(m->fx,0,sizeof m->fx);m->fx_next=0;
            m->steps=2000;put_fx(m,kind,2,16,side); /* actor 2 (home) acts, 16 (away) is on the receiving end */
            m->actors[3].x=m->actors[2].x+4;m->actors[3].y=m->actors[2].y; /* a teammate close by */
            m->actors[4].x=m->actors[2].x+40;                                 /* one far away */
            bool moved=false,flashed=false,shook=false,popped=false;unsigned most=0;
            for(unsigned step=0;step<=200;++step){
                m->steps=2000+step;
                for(unsigned who=0;who<22;++who){
                    FootballFxPose p=football_live_fx_pose(m,who);
                    assert(isfinite(p.dx) && isfinite(p.dy) && isfinite(p.angle) && p.dx>-20 && p.dx<20 && p.dy>-24 && p.dy<8 && fabsf(p.angle)<1.6f && p.sx>.7f && p.sx<1.3f && p.sy>.7f && p.sy<1.3f);
                    moved|=p.dx!=0 || p.dy!=0 || p.angle!=0;
                    if(who==4 && kind==FB_FX_TOUCHDOWN)assert(p.dy==0); /* too far from the scorer to celebrate */
                    if(who>=5 && who!=16 && kind!=FB_FX_TOUCHDOWN)assert(p.dx==0 && p.dy==0 && p.angle==0); /* only the people involved move */
                }
                football_live_fx_shake(m,&sx,&sy);assert(isfinite(sx) && isfinite(sy) && fabsf(sx)<=8 && fabsf(sy)<=8);shook|=sx!=0 || sy!=0;
                float pop=football_live_fx_banner_scale(m);assert(pop>=1 && pop<=1.56f);popped|=pop>1;
                drawn=0;rects=0;football_live_art_top(&renderer,&career,&camera,0);
                assert(rects<plain+200); /* a bounded number of extra rectangles */
                flashed|=rects>plain;if(rects>plain+most)most=rects-plain;
            }
            assert(moved && flashed); /* every moment moves someone and draws something */
            if(most>worst)worst=most;
            if(kind==FB_FX_TOUCHDOWN)assert(shook && popped && most>30); /* confetti, flash */
            if(kind==FB_FX_SACK || kind==FB_FX_INTERCEPTION)assert(shook && popped);
            /* A moment that has not happened yet, or is long over, changes nothing. */
            m->steps=1999;{FootballFxPose p=football_live_fx_pose(m,2);assert(p.dx==0 && p.dy==0 && p.angle==0);}
            m->steps=2000+400;drawn=0;rects=0;football_live_art_top(&renderer,&career,&camera,0);
            if(kind!=FB_FX_TACKLE && kind!=FB_FX_SACK && kind!=FB_FX_DIVE)assert(football_live_fx_pose(m,2).dy==0);
        }
    }
    /* Same moment, same picture: nothing depends on frame times. */
    memset(m->fx,0,sizeof m->fx);m->fx_next=0;m->steps=3000;put_fx(m,FB_FX_TOUCHDOWN,2,22,1);m->steps=3020;
    drawn=0;rects=0;football_live_art_top(&renderer,&career,&camera,0);unsigned first=rects;FootballFxPose a=football_live_fx_pose(m,3);
    drawn=0;rects=0;football_live_art_top(&renderer,&career,&camera,17);assert(rects==first);FootballFxPose b=football_live_fx_pose(m,3);assert(a.dy==b.dy && a.angle==b.angle && a.dy!=0);
    assert(!football_live_fx_pose(NULL,0).dy && football_live_fx_pose(m,22).dy==0);football_live_fx_shake(NULL,&sx,&sy);assert(sx==0 && sy==0);football_live_fx_shake(m,NULL,NULL);
    memset(m->fx,0,sizeof m->fx);m->phase=FB_LIVE_CALL;
    printf("motion: at most %u extra rectangles in a frame (a plain frame is %u)\n",worst,plain);
    puts("football live art: native-left front sprites, running directions and retreating QB receiver-facing submission passed");
}
