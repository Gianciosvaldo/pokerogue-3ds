#include "football_live_art.h"
#include "football_live_fx.h"
#include "football_live_systems.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
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
/* The line shown over the field while the ball is dead. */
static bool banner(const FootballLive *m,char *out,size_t size)
{
    if((m->phase!=FB_LIVE_DEAD && m->phase!=FB_LIVE_END) || m->event==FB_EVENT_NONE)return false;
    switch(m->event){
    case FB_EVENT_TOUCHDOWN:snprintf(out,size,"TOUCHDOWN!");break;
    case FB_EVENT_INTERCEPTION:snprintf(out,size,"INTERCEPTION!");break;
    case FB_EVENT_SACK:snprintf(out,size,"SACK!");break;
    case FB_EVENT_FUMBLE:snprintf(out,size,"FUMBLE!");break;
    case FB_EVENT_SAFETY:snprintf(out,size,"SAFETY");break;
    case FB_EVENT_PUNT:
        if(m->return_yards>0)snprintf(out,size,"PUNT  %d YDS   RETURN %d",(int)m->kick_yards,(int)m->return_yards);
        else snprintf(out,size,"PUNT  %d YDS",(int)m->kick_yards);
        break;
    case FB_EVENT_KICKOFF:
        if(m->return_yards>0)snprintf(out,size,"KICKOFF  %d YDS   RETURN %d",(int)m->kick_yards,(int)m->return_yards);
        else snprintf(out,size,"KICKOFF  %d YDS",(int)m->kick_yards);
        break;
    case FB_EVENT_FAIR_CATCH:snprintf(out,size,"FAIR CATCH");break;
    case FB_EVENT_TOUCHBACK:snprintf(out,size,"TOUCHBACK");break;
    case FB_EVENT_OUT_OF_BOUNDS:snprintf(out,size,"OUT OF BOUNDS");break;
    case FB_EVENT_MUFF:snprintf(out,size,"MUFFED CATCH!");break;
    case FB_EVENT_BLOCKED:snprintf(out,size,"BLOCKED!");break;
    case FB_EVENT_FIELD_GOAL:snprintf(out,size,"FIELD GOAL  %d YDS",(int)m->kick_yards);break;
    case FB_EVENT_FIELD_GOAL_MISSED:snprintf(out,size,"NO GOOD");break;
    case FB_EVENT_EXTRA_POINT:snprintf(out,size,"EXTRA POINT IS GOOD");break;
    case FB_EVENT_EXTRA_POINT_MISSED:snprintf(out,size,"EXTRA POINT NO GOOD");break;
    case FB_EVENT_TWO_POINT:snprintf(out,size,"TWO-POINT CONVERSION GOOD");break;
    case FB_EVENT_TWO_POINT_FAILED:snprintf(out,size,"TWO-POINT TRY STOPPED");break;
    default:return false;
    }
    return true;
}
void football_live_art_top(FootballRenderer *r,const FootballCareer *c,const FootballCamera *camera,unsigned ms)
{
    if(!r || !r->text || !c || !camera)return;
    C2D_TextBufClear(r->text);
    /* A hit, a turnover or a touchdown shakes the whole field for a moment (the scoreboard stays put). */
    float shake_x,shake_y;football_live_fx_shake(&c->live,&shake_x,&shake_y);
    FootballCamera shaken=*camera;shaken.x+=shake_x/FB_CAMERA_SCALE;shaken.y+=shake_y/(FB_CAMERA_SCALE*FB_CAMERA_SQUASH);
    const FootballCamera *cam=&shaken;
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
    if(!m->special)world(cam,first,0,.18f,53.333f,.05f,gold);
    unsigned order[22];for(unsigned i=0;i<22;++i){order[i]=i;for(unsigned j=i;j>0 && m->actors[order[j]].y<m->actors[order[j-1]].y;--j){unsigned t=order[j];order[j]=order[j-1];order[j-1]=t;}}
    for(unsigned rank=0;rank<22;++rank){unsigned i=order[rank];const FootballLiveActor *a=&m->actors[i];
        float x=football_camera_x(cam,a->x),y=football_camera_y(cam,a->y);
        if(x< -18 || x>418 || y<50 || y>242)continue;
        bool selected=i==m->controlled;
        u32 team=C2D_Color32(a->home?76:232,a->home?201:53,a->home?240:74,255);
        FootballFxPose pose=football_live_fx_pose(m,i);
        float lift=pose.dy<0?-pose.dy:0,shadow=20-lift*.7f;
        rect(x-shadow/2,y+8,shadow,4,.10f,C2D_Color32(15,50,23,220));
        if(selected){rect(x-16,y+10,32,3,.11f,gold);rect(x-16,y-17,3,29,.11f,gold);rect(x+13,y-17,3,29,.11f,gold);}
        const rr_frame *frame=football_render_front_frame(r,i,ms);
        float scale=frame && frame->src_w>0 && frame->src_h>0?fminf(38.0f/frame->src_w,38.0f/frame->src_h):1;
        /* Adversary front sheets natively face LEFT; flip them to face right. */
        if(!football_render_draw_sprite_pose(frame,x+pose.dx,y-6+pose.dy,scale,!football_live_faces_left(m,i),pose.sx,pose.sy,pose.angle))rect(x+pose.dx-9,y+pose.dy-15,18,25,.2f,team);
        rect(x+pose.dx-7,y+13,14,3,.3f,team);
    }
    football_live_fx_draw(r,c,cam);
    /* Receivers keep their numbered targets even when outside the closer view. */
    if(m->rules.offense==FB_HOME && !m->caught && !m->special){static const unsigned targets[5]={2,3,4,5,1};
        for(unsigned i=0;i<5;++i){const FootballLiveActor *a=&m->actors[targets[i]];float x=bound(football_camera_x(cam,a->x),12,382),y=bound(football_camera_y(cam,a->y)-34,50,217);char n[3];snprintf(n,sizeof n,"%u",i+1);rect(x-7,y,18,19,.7f,navy);label(r,n,x-3,y,.5f,gold);}
    }
    float bx=football_camera_x(cam,m->ball_x),by=football_camera_y(cam,m->ball_y)-m->ball_z*5;
    rect(bx-4,by-4,9,5,.6f,C2D_Color32(154,85,36,255));rect(bx-1,by-3,3,2,.61f,white);
    if((m->special==FB_SPECIAL_PUNT || m->special==FB_SPECIAL_KICKOFF) && m->stage==FB_STAGE_FLIGHT){
        /* Where the kick comes down, for the returner and for the coverage. */
        world(cam,m->to_x-1.4f,m->to_y-.6f,2.8f,1.2f,.07f,C2D_Color32(255,210,63,200));
        world(cam,m->to_x-.6f,m->to_y-1.4f,1.2f,2.8f,.07f,C2D_Color32(255,210,63,200));
        if(m->kick_side==FB_AWAY && m->special==FB_SPECIAL_PUNT)label(r,"A: FAIR CATCH",138,218,.45f,gold);
    }
    char line[40];
    if(banner(m,line,sizeof line)){
        float pop=football_live_fx_banner_scale(m),width=(float)strlen(line)*13*pop,top=70-18*pop; /* high up, so the players it is about stay in view */
        rect(200-width/2-14,top,width+28,36*pop,.85f,navy);rect(200-width/2-14,top,width+28,3,.86f,gold);rect(200-width/2-14,top+33*pop,width+28,3,.86f,gold);
        label(r,line,200-width/2,top+5*pop,.8f*pop,white);
        /* Whoever made the play earns species candy for it. */
        unsigned candy=0,member=22;
        for(unsigned i=0;i<m->credit_count && i<4;++i)if(m->credits[i].side==FB_HOME){candy+=football_credit_candy(m->credits[i].kind);member=m->credits[i].member;}
        if(candy && member<22){
            char gain[48];snprintf(gain,sizeof gain,"%.14s  +%u CANDY",football_live_art_species(c->roster.mon[member].b.species),candy);
            float gw=(float)strlen(gain)*7.5f,gy=top+36*pop+4;
            rect(200-gw/2-10,gy,gw+20,22,.85f,navy);rect(200-gw/2-10,gy,gw+20,2,.86f,gold);label(r,gain,200-gw/2,gy+4,.45f,gold);
        }
    }
    C2D_DrawRectSolid(0,0,.8f,400,44,navy);C2D_DrawRectSolid(0,41,.81f,400,3,gold);
    char score[72],situation[72];unsigned seconds=m->rules.seconds_left;
    if(m->phase==FB_LIVE_PLAY && m->rules.quarter<5)seconds=seconds>(unsigned)m->elapsed?seconds-(unsigned)m->elapsed:0;
    snprintf(score,sizeof score,"HOME %02u   AWAY %02u     Q%u  %u:%02u",m->rules.score[0],m->rules.score[1],m->rules.quarter,seconds/60,seconds%60);
    if(m->special)snprintf(situation,sizeof situation,"ROUND %u/7   %s",c->round,football_live_art_special_name(m->special));
    else if(m->conversion)snprintf(situation,sizeof situation,"ROUND %u/7   %s",c->round,m->conversion==1?"TRY AFTER TOUCHDOWN":"TWO-POINT TRY");
    else snprintf(situation,sizeof situation,"ROUND %u/7   %s  DOWN %u / %u YD",c->round,dir>0?">>>":"<<<",m->rules.down,football_rules_distance(&m->rules));
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

const char *football_live_art_special_name(FootballSpecial kind)
{
    return kind==FB_SPECIAL_PUNT?"PUNT":kind==FB_SPECIAL_FIELD_GOAL?"FIELD GOAL":kind==FB_SPECIAL_EXTRA_POINT?"EXTRA POINT":kind==FB_SPECIAL_KICKOFF?"KICKOFF":"GO FOR TWO";
}
void football_live_art_special_card(FootballRenderer *r,FootballSpecial kind,const char *detail,float x,float y,float w,float h,bool selected,bool enabled)
{
    if(!r || !r->text)return;
    u32 gold=C2D_Color32(255,210,63,255),white=C2D_Color32(246,246,242,255),dim=C2D_Color32(120,126,148,255),blue=C2D_Color32(74,163,255,255);
    C2D_DrawRectSolid(x,y,0,w,h,selected?gold:C2D_Color32(74,80,104,255));
    C2D_DrawRectSolid(x+2,y+2,.01f,w-4,h-4,C2D_Color32(27,37,71,255));
    label(r,football_live_art_special_name(kind),x+8,y+5,.46f,!enabled?dim:selected?gold:white);
    C2D_DrawRectSolid(x+8,y+31,.02f,w-16,1,enabled?blue:dim);
    if(kind==FB_SPECIAL_PUNT){
        /* A high arc down the field. */
        for(unsigned i=0;i<20;++i){float t=i/19.0f;C2D_DrawRectSolid(x+14+t*(w-40),y+h-24-4*t*(1-t)*(h-58)*1.6f,.03f,3,3,enabled?gold:dim);}
    }else if(kind==FB_SPECIAL_NONE){
        /* The try as a play: the end zone line, the ball carrier and the defenders standing in front of it. */
        C2D_DrawRectSolid(x+10,y+40,.03f,w-20,2,enabled?gold:dim);
        for(unsigned i=0;i<4;++i)C2D_DrawRectSolid(x+w*.25f+i*(w*.5f/3)-2,y+h-34,.03f,5,5,enabled?blue:dim);
        C2D_DrawRectSolid(x+w*.5f-3,y+h-24,.04f,7,7,enabled?gold:dim);
        for(unsigned i=0;i<8;++i)C2D_DrawRectSolid(x+w*.5f,y+h-28-i*(h-70)/8,.04f,2,2,enabled?white:dim);
    }else{
        /* The uprights, with the kick on its way. */
        float px=x+w-34;
        C2D_DrawRectSolid(px,y+38,.03f,2,24,enabled?gold:dim);C2D_DrawRectSolid(px+14,y+38,.03f,2,24,enabled?gold:dim);C2D_DrawRectSolid(px,y+62,.03f,16,2,enabled?gold:dim);C2D_DrawRectSolid(px+7,y+62,.03f,2,12,enabled?gold:dim);
        for(unsigned i=0;i<12;++i){float t=i/11.0f;C2D_DrawRectSolid(x+14+t*(w-58),y+h-18-t*(h-46)-2*t*(1-t)*20,.03f,3,3,enabled?white:dim);}
    }
    if(detail)label(r,detail,x+8,y+h-17,.33f,enabled?white:dim);
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
