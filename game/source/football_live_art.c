#include "football_live_art.h"
#include <math.h>
#include <stdio.h>
static void label(FootballRenderer *r,const char *s,float x,float y,float scale,u32 color)
{
    C2D_Text t;C2D_TextParse(&t,r->text,s);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,x,y,.9f,scale,scale,color);
}
static float bound(float x,float a,float b){return x<a?a:x>b?b:x;}
static void rect(float x,float y,float w,float h,float z,u32 color)
{
    float right=fminf(400,x+w),bottom=fminf(240,y+h);x=fmaxf(x,0);y=fmaxf(y,44);
    if(right>x && bottom>y)C2D_DrawRectSolid(x,y,z,right-x,bottom-y,color);
}
static void world(const FootballCamera *c,float x,float y,float w,float h,float z,u32 color)
{
    rect(football_camera_x(c,x),football_camera_y(c,y),w*FB_CAMERA_SCALE,h*FB_CAMERA_SCALE*FB_CAMERA_SQUASH,z,color);
}
void football_live_art_top(FootballRenderer *r,const FootballCareer *c,const FootballCamera *cam,unsigned ms)
{
    if(!r || !r->text || !c || !cam)return;
    C2D_TextBufClear(r->text);
    u32 white=C2D_Color32(238,244,234,255),navy=C2D_Color32(20,26,46,255),gold=C2D_Color32(255,210,63,255);
    rect(0,44,400,196,0,navy);
    /* Reference palette, oblique turf and repeatable crowd pixels. All bounded
     * geometry: no giant offscreen canvas, browser textures or frame allocations. */
    for(unsigned band=0;band<24;++band)world(cam,band*5,0,5,53.333f,.01f,C2D_Color32(band%2?42:47,band%2?128:143,band%2?52:58,255));
    world(cam,0,0,10,53.333f,.02f,C2D_Color32(30,65,130,255));world(cam,110,0,10,53.333f,.02f,C2D_Color32(143,29,38,255));
    world(cam,0,-3,120,3,.02f,C2D_Color32(190,183,157,255));world(cam,0,53.333f,120,3,.02f,C2D_Color32(190,183,157,255));
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<64;++col){
        unsigned v=(col*37+row*17)%7;float px=football_camera_x(cam,col*2.0f),py=football_camera_y(cam,-5-row*1.7f);
        rect(px,py,7,5,.03f,v<3?gold:v<5?white:C2D_Color32(58,134,255,255));
    }
    for(unsigned yard=10;yard<=110;yard+=5){
        world(cam,(float)yard,0,.12f,53.333f,.04f,white);
        if(yard%10==0 && yard>10 && yard<110){
            char n[8];unsigned display=yard<=60?yard-10:110-yard;snprintf(n,sizeof n,"%u",display);
            float px=football_camera_x(cam,(float)yard)-8;
            float py=football_camera_y(cam,8);if(px>=0 && px<380 && py>50 && py<220)label(r,n,px,py,.48f,white);
            py=football_camera_y(cam,44);if(px>=0 && px<380 && py>50 && py<220)label(r,n,px,py,.48f,white);
        }
    }
    for(unsigned yard=11;yard<110;++yard)for(unsigned hash=0;hash<2;++hash)world(cam,(float)yard,hash?29.75f:23.58f,.12f,.5f,.04f,white);
    world(cam,0,0,120,.18f,.05f,white);world(cam,0,53.15f,120,.18f,.05f,white);
    const FootballLive *m=&c->live;float dir=m->rules.offense==FB_HOME?1:-1;
    world(cam,m->los,0,.18f,53.333f,.05f,C2D_Color32(74,163,255,255));
    float first=m->rules.offense==FB_HOME?10+m->rules.first_down_at:110-m->rules.first_down_at;
    world(cam,first,0,.18f,53.333f,.05f,gold);
    unsigned order[22];for(unsigned i=0;i<22;++i){order[i]=i;for(unsigned j=i;j>0 && m->actors[order[j]].y<m->actors[order[j-1]].y;--j){unsigned t=order[j];order[j]=order[j-1];order[j-1]=t;}}
    for(unsigned rank=0;rank<22;++rank){unsigned i=order[rank];const FootballLiveActor *a=&m->actors[i];
        float x=football_camera_x(cam,a->x),y=football_camera_y(cam,a->y);
        if(x< -18 || x>418 || y<50 || y>242)continue;
        bool selected=i==m->controlled;
        u32 team=C2D_Color32(a->home?76:232,a->home?201:53,a->home?240:74,255);
        rect(x-10,y+8,20,4,.10f,C2D_Color32(15,50,23,220));
        if(selected){rect(x-16,y+10,32,3,.11f,gold);rect(x-16,y-17,3,29,.11f,gold);rect(x+13,y-17,3,29,.11f,gold);}
        const rr_frame *frame=football_render_front_frame(r,i,ms);
        float scale=frame && frame->src_w>0 && frame->src_h>0?fminf(38.0f/frame->src_w,38.0f/frame->src_h):1;
        if(!football_render_draw_sprite(frame,x,y-6,scale,a->vx<0 || (a->vx==0 && !a->home)))rect(x-9,y-15,18,25,.2f,team);
        rect(x-7,y+13,14,3,.3f,team);
    }
    /* Receivers keep their numbered targets even when outside the closer view. */
    if(m->rules.offense==FB_HOME && !m->caught){static const unsigned targets[5]={2,3,4,5,1};
        for(unsigned i=0;i<5;++i){const FootballLiveActor *a=&m->actors[targets[i]];float x=bound(football_camera_x(cam,a->x),12,382),y=bound(football_camera_y(cam,a->y)-34,50,217);char n[3];snprintf(n,sizeof n,"%u",i+1);rect(x-7,y,18,19,.7f,navy);label(r,n,x-3,y,.5f,gold);}
    }
    float bx=football_camera_x(cam,m->ball_x),by=football_camera_y(cam,m->ball_y)-m->ball_z*5;
    rect(bx-4,by-4,9,5,.6f,C2D_Color32(154,85,36,255));rect(bx-1,by-3,3,2,.61f,white);
    C2D_DrawRectSolid(0,0,.8f,400,44,navy);C2D_DrawRectSolid(0,41,.81f,400,3,gold);
    char score[72],situation[72];unsigned seconds=m->rules.seconds_left;
    if(m->phase==FB_LIVE_PLAY && m->rules.quarter<5)seconds=seconds>(unsigned)m->elapsed?seconds-(unsigned)m->elapsed:0;
    snprintf(score,sizeof score,"HOME %02u   AWAY %02u     Q%u  %u:%02u",m->rules.score[0],m->rules.score[1],m->rules.quarter,seconds/60,seconds%60);
    snprintf(situation,sizeof situation,"ROUND %u/7   %s  DOWN %u / %u YD",c->round,dir>0?">>>":"<<<",m->rules.down,football_rules_distance(&m->rules));
    label(r,score,12,2,.53f,white);label(r,situation,12,23,.40f,gold);
}
void football_live_art_card(FootballRenderer *r,FootballPlayId id,float x,float y,float w,float h,bool selected)
{
    const FootballPlayDefinition *p=football_playbook_get(id);if(!p || !r || !r->text)return;
    u32 gold=C2D_Color32(255,210,63,255),white=C2D_Color32(246,246,242,255);
    C2D_DrawRectSolid(x,y,0,w,h,selected?gold:C2D_Color32(74,80,104,255));
    C2D_DrawRectSolid(x+2,y+2,.01f,w-4,h-4,C2D_Color32(27,37,71,255));
    label(r,p->name,x+8,y+5,.42f,selected?gold:white);
    C2D_DrawRectSolid(x+8,y+31,.02f,w-16,1,C2D_Color32(74,163,255,255));
    for(unsigned i=0;i<5;++i)C2D_DrawRectSolid(x+w*.5f-12+i*6,y+36,.03f,4,4,white);
    float last_x=x+w*.22f,last_y=y+h-12;
    C2D_DrawRectSolid(last_x-2,last_y-2,.03f,5,5,gold);
    for(unsigned i=0;i<18;++i){FootballRoutePoint point=football_playbook_route(p,i/17.0f);float px=x+w*.22f+point.y*2,py=y+h-12-point.x*1.2f;
        C2D_DrawRectSolid(px,py,.04f,3,3,gold);last_x=px;last_y=py;}
    C2D_DrawRectSolid(last_x-2,last_y-3,.04f,7,3,gold);
}

void football_live_art_defense_card(FootballRenderer *r,FootballDefenseId id,float x,float y,float w,float h,bool selected)
{
    const FootballDefenseDefinition *d=football_defense_get(id);if(!d || !r || !r->text)return;
    u32 gold=C2D_Color32(255,210,63,255),blue=C2D_Color32(76,201,240,255);
    C2D_DrawRectSolid(x,y,0,w,h,selected?gold:C2D_Color32(74,80,104,255));
    C2D_DrawRectSolid(x+2,y+2,.01f,w-4,h-4,C2D_Color32(27,37,71,255));
    label(r,d->name,x+8,y+5,.48f,selected?gold:blue);
    C2D_DrawRectSolid(x+10,y+h-24,.02f,w-20,1,blue);
    for(unsigned i=0;i<5;++i){float px=x+15+i*(w-30)/4;
        C2D_DrawRectSolid(px,y+34,.03f,5,5,gold);
        if(id==FB_DEF_ZONE)C2D_DrawRectSolid(px-6,y+43,.03f,18,5,blue);
        else for(unsigned step=0;step<5;++step)C2D_DrawRectSolid(px+(id==FB_DEF_BLITZ?0:step),y+42+step*3,.03f,2,2,blue);
    }
}
