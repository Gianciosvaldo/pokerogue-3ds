#include "football_live_art.h"
#include "football_live_fx.h"
#include "football_live_systems.h"
#include "menu_fakes.h"
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
static unsigned priced;
unsigned football_live_art_candy_milli(const FootballCareer *c,unsigned member,unsigned points){(void)c;++priced;return member<22?points*100u:0u;}
void C2D_TextBufClear(C2D_TextBuf b){assert(b);}
/* The yard numbers are painted on the turf: every all-digit text is drawn behind the banner's window (z .85), and nothing but them is. */
static char parsed[64];static unsigned field_numbers,numbers_over_banner,words_under_banner;
void C2D_TextParse(C2D_Text *t,C2D_TextBuf b,const char *s){(void)t;assert(b && s);snprintf(parsed,sizeof parsed,"%s",s);}
void C2D_TextOptimize(C2D_Text *t){(void)t;}
void C2D_DrawText(const C2D_Text *t,unsigned flags,float x,float y,float z,float sx,float sy,u32 color)
{
    (void)t;(void)flags;(void)x;(void)y;(void)sx;(void)sy;(void)color;
    bool digits=strlen(parsed)>=2;for(const char *p=parsed;*p;++p)if(*p<'0' || *p>'9')digits=false; /* two digits: a yard number, not a target's 1-5 */
    if(digits && z<.85f)++field_numbers;
    if(digits && z>=.85f)++numbers_over_banner;
    if(!digits && z<.85f)++words_under_banner;
}
static unsigned rects,line_rects;
static bool capture;static float cap_x0,cap_y0,cap_x1,cap_y1; /* the extent of the rectangles drawn while a card is being drawn */
/* The engine records these (football_live_fx); the art only reads them, so the test writes them itself. */
static void put_fx(FootballLive *m,unsigned kind,unsigned actor,unsigned other,int side){m->fx[m->fx_next]=(FootballFx){(uint8_t)kind,(uint8_t)actor,(uint8_t)other,(int8_t)side,m->steps,m->actors[actor].x,m->actors[actor].y};m->fx_next=(uint8_t)((m->fx_next+1)%FB_FX_COUNT);}
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 color){(void)color;line_rects+=z>.0595f && z<.0655f;assert(isfinite(w) && w>0 && isfinite(h) && h>0 && isfinite(x) && isfinite(y));++rects;
    if(capture){cap_x0=fminf(cap_x0,x);cap_y0=fminf(cap_y0,y);cap_x1=fmaxf(cap_x1,x+w);cap_y1=fmaxf(cap_y1,y+h);}return true;}
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
    /* The yard numbers sit under the banner and the HUD; a dead ball shows its banner, and no player target number, over the field. */
    m->phase=FB_LIVE_PLAY;m->special=FB_SPECIAL_NONE;m->event=FB_EVENT_NONE;field_numbers=numbers_over_banner=words_under_banner=0;
    camera.x=60;camera.y=14;drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(field_numbers>=2 && !numbers_over_banner && !words_under_banner);
    for(unsigned event=FB_EVENT_TOUCHDOWN;event<=FB_EVENT_KICKOFF;++event){
        m->phase=FB_LIVE_DEAD;m->event=(uint8_t)event;m->return_yards=23;field_numbers=numbers_over_banner=words_under_banner=0;
        drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(field_numbers>=2 && !numbers_over_banner && !words_under_banner);
    }
    m->phase=FB_LIVE_PLAY;m->event=FB_EVENT_NONE;m->special=FB_SPECIAL_FIELD_GOAL;
    /* The headline names every kick, and the try is labelled before and during its play. */
    assert(!strcmp(football_live_art_special_name(FB_SPECIAL_KICKOFF),"KICKOFF") && !strcmp(football_live_art_special_name(FB_SPECIAL_EXTRA_POINT),"EXTRA POINT") && !strcmp(football_live_art_special_name(FB_SPECIAL_NONE),"GO FOR TWO"));
    m->special=FB_SPECIAL_NONE;m->phase=FB_LIVE_CALL;m->event=FB_EVENT_NONE;
    for(unsigned conversion=1;conversion<=2;++conversion){drawn=0;m->conversion=(uint8_t)conversion;football_live_art_top(&renderer,&career,&camera,0);}
    m->conversion=0;
    /* A credited play adds the species line under the banner; a credit for the other side or none adds nothing. */
    m->phase=FB_LIVE_DEAD;m->event=FB_EVENT_INTERCEPTION;named=0;drawn=0;m->credit_count=0;football_live_art_top(&renderer,&career,&camera,0);assert(!named);
    m->credit_count=1;m->credits[0]=(FootballCredit){FB_AWAY,3,FB_CREDIT_INTERCEPTION};drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(!named);
    for(unsigned kind=FB_CREDIT_SACK;kind<=FB_CREDIT_TOUCHDOWN;++kind){m->credits[0]=(FootballCredit){FB_HOME,3,(uint8_t)kind};named=0;priced=0;drawn=0;football_live_art_top(&renderer,&career,&camera,0);assert(named==1 && priced==1);}
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
    /* The playcall cards on the bottom screen: one classic window each with its name inside, the diagram within the card, and the yellow
     * frame (four rectangles) on the chosen one only. */
    {
        const float x=10,y=40,w=145,h=80;unsigned cards=0;
        for(unsigned kind=0;kind<3;++kind){
            unsigned count=kind==0?FB_PLAY_COUNT:kind==1?FB_DEF_COUNT:FB_SPECIAL_KICKOFF+1;
            for(unsigned id=0;id<count;++id)for(unsigned pick=0;pick<2;++pick)for(unsigned enabled=0;enabled<2;++enabled){
                if(kind!=2 && !enabled)continue;
                fake_begin(RR_BOTTOM);assert(ui_space(UI_PHYS_BOTTOM));capture=true;cap_x0=cap_y0=1e9f;cap_x1=cap_y1=-1e9f;
                if(kind==0)football_live_art_card(&renderer,(FootballPlayId)id,x,y,w,h,pick);
                else if(kind==1)football_live_art_defense_card(&renderer,(FootballDefenseId)id,x,y,w,h,pick);
                else football_live_art_special_card(&renderer,(FootballSpecial)id,id==FB_SPECIAL_FIELD_GOAL?"58 yd: out of range":"45 yd   62% to make it",x,y,w,h,pick,enabled);
                capture=false;
                assert(fake.window_count==1 && fake.windows[0].x==x && fake.windows[0].y==y && fake.windows[0].w==w && fake.windows[0].h==h);
                assert(fake.rect_count==((pick && enabled)?4u:0u) && fake.text_count>=1);
                fake_check(); /* the name (and the detail) inside the window, clear of each other */
                assert(cap_x0>=x && cap_y0>=y && cap_x1<=x+w && cap_y1<=y+h); /* the diagram stays in the card */
                ++cards;
            }
        }
        assert(cards>40);
        football_live_art_card(NULL,FB_RUN_DRAW,x,y,w,h,false);football_live_art_card(&renderer,FB_PLAY_COUNT,x,y,w,h,false);football_live_art_defense_card(&renderer,FB_DEF_COUNT,x,y,w,h,false);
    }
    /* Route lines: the called paths show before the snap for the human's offense and nowhere else, and they leave with the snap. */
    {
        m->special=FB_SPECIAL_NONE;m->conversion=0;m->event=FB_EVENT_NONE;m->rules.offense=FB_HOME;m->los=40;m->run_side=1;memset(m->fx,0,sizeof m->fx);
        static const float lane[6]={26.67f,26.67f,8,15,40,47};
        for(unsigned r=0;r<6;++r){m->snap_x[r]=r?39:36;m->snap_y[r]=lane[r];m->actors[r].x=m->snap_x[r];m->actors[r].y=m->snap_y[r];}
        FootballCamera view={.x=42,.y=26.67f,.ready=true};
        unsigned shown_runs=0,shown_passes=0,most_lines=0;
        for(unsigned id=0;id<FB_PLAY_COUNT;++id){
            const FootballPlayDefinition *play=football_playbook_get((FootballPlayId)id);m->play=(FootballPlayId)id;
            unsigned frames[4]={0,0,0,0};
            for(unsigned how=0;how<4;++how){
                m->phase=how==3?FB_LIVE_SET:how==0?FB_LIVE_CALL:how==1?FB_LIVE_PLAY:FB_LIVE_DEAD;
                m->rules.offense=FB_HOME;
                drawn=0;line_rects=0;football_live_art_top(&renderer,&career,&view,0);frames[how]=line_rects;
            }
            assert(frames[0]==0 && frames[1]==0 && frames[2]==0); /* nothing once the ball is snapped, or before the play is set */
            assert(frames[3]>=8 && frames[3]<400);                  /* a handful of dots and end squares, a bounded number */
            if(frames[3]>most_lines)most_lines=frames[3];
            m->phase=FB_LIVE_SET;
            m->rules.offense=FB_AWAY;drawn=0;line_rects=0;football_live_art_top(&renderer,&career,&view,0);assert(line_rects==0); /* the CPU's offense keeps its plan */
            m->rules.offense=FB_HOME;m->special=FB_SPECIAL_PUNT;drawn=0;line_rects=0;football_live_art_top(&renderer,&career,&view,0);assert(line_rects==0);m->special=FB_SPECIAL_NONE;
            if(play->type==FB_PLAY_RUN)++shown_runs;else ++shown_passes;
        }
        assert(shown_runs==8 && shown_passes==FB_PLAY_COUNT-8);
        /* A run shows only the runner's path: a pass with five routes draws more than a run with one. */
        m->phase=FB_LIVE_SET;
        m->play=FB_RUN_INSIDE_ZONE;drawn=0;line_rects=0;football_live_art_top(&renderer,&career,&view,0);unsigned run_lines=line_rects;
        m->play=FB_PASS_VERTICALS;drawn=0;line_rects=0;football_live_art_top(&renderer,&career,&view,0);assert(line_rects>run_lines);
        m->phase=FB_LIVE_CALL;
        printf("route lines: at most %u rectangles in a frame\n",most_lines);
    }
    printf("motion: at most %u extra rectangles in a frame (a plain frame is %u)\n",worst,plain);
    puts("football live art: native-left front sprites, running directions and retreating QB receiver-facing submission passed");
}
