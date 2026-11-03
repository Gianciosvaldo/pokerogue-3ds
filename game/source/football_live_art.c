#include "football_live_art.h"
#include "football_live_fx.h"
#include "football_menu.h"
#include "football_live_systems.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Text at a depth: the HUD and the banners sit in front of everything (.9), the yard numbers are painted on the turf, under the players and
 * under any banner or window drawn over the field. */
static void label_at(FootballRenderer *r,const char *s,float x,float y,float z,float scale,u32 color)
{
    C2D_Text t;C2D_TextParse(&t,r->text,s);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,x,y,z,scale,scale,color);
}
static void label(FootballRenderer *r,const char *s,float x,float y,float scale,u32 color){label_at(r,s,x,y,.9f,scale,color);}
#define FIELD_NUMBER_Z .045f
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
/* Before the snap the human's offense sees where each eligible player is called to go: a dotted line from his spot along his route, a
 * square where he stops and a few fainter dots when he runs on, the primary read (the one the play card shows) in gold. A run shows the
 * runner's way to the handoff and on along the called path. They are gone the moment the ball is snapped. Under the players, over the
 * turf; dots off the screen are skipped so the cost stays a few dozen rectangles. */
static void route_lines(const FootballLive *m,const FootballCamera *cam)
{
    if(m->phase!=FB_LIVE_SET || m->rules.offense!=FB_HOME)return; /* a kick has no line: football_live_route_line answers 0 */
    const FootballPlayDefinition *p=football_playbook_get(m->play);if(!p)return;
    unsigned read=football_playbook_primary_receiver(p),primary=p->type==FB_PLAY_PASS?(read==4?1:read+2):football_playbook_runner(p);
    for(unsigned pass=0;pass<2;++pass)for(unsigned rank=1;rank<=5;++rank){
        if((rank==primary)!=(pass==1))continue; /* the gold line last, so it stays on top where two cross */
        float x[8],y[8];bool settles;unsigned n=football_live_route_line(m,rank,x,y,8,&settles);
        if(n<2)continue;
        u32 color=rank==primary?C2D_Color32(255,210,63,255):C2D_Color32(238,244,234,230),faint=rank==primary?C2D_Color32(255,210,63,120):C2D_Color32(238,244,234,110);
        for(unsigned k=1;k<n;++k){
            float sx=x[k]-x[k-1],sy=y[k]-y[k-1],len=hypotf(sx,sy);if(len<.01f)continue;
            sx/=len;sy/=len;
            for(unsigned d=0;d<(unsigned)ceilf(len/1.1f);++d){float t=(float)d*1.1f;
                float px=football_camera_x(cam,x[k-1]+sx*t),py=football_camera_y(cam,y[k-1]+sy*t);
                if(px>-4 && px<404 && py>40 && py<244)rect(px-1.5f,py-1.5f,3,3,.06f,color);
            }
        }
        float ex=football_camera_x(cam,x[n-1]),ey=football_camera_y(cam,y[n-1]);
        /* A player who does not settle runs straight on upfield past his last point (home attacks toward +x). */
        if(!settles)for(unsigned d=1;d<=3;++d){
            float px=football_camera_x(cam,x[n-1]+1.1f*d),py=ey;
            if(px>-4 && px<404 && py>40 && py<244)rect(px-1.5f,py-1.5f,3,3,.06f,faint);
        }
        if(ex>-4 && ex<404 && ey>40 && ey<244)rect(ex-3,ey-3,6,6,.065f,color);
    }
}
/* The line shown over the field while the ball is dead. */
static bool banner(const FootballLive *m,char *out,size_t size)
{
    if((m->phase!=FB_LIVE_DEAD && m->phase!=FB_LIVE_END) || m->event==FB_EVENT_NONE)return false;
    switch(m->event){
    case FB_EVENT_TOUCHDOWN:snprintf(out,size,"TOUCHDOWN!");break;
    case FB_EVENT_INTERCEPTION:if(m->return_yards>0)snprintf(out,size,"INTERCEPTION!   RETURN %d",(int)m->return_yards);else snprintf(out,size,"INTERCEPTION!");break;
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
            float py=football_camera_y(cam,8);if(px>=0 && px<380 && py>50 && py<220)label_at(r,n,px,py,FIELD_NUMBER_Z,.48f,white);
            py=football_camera_y(cam,44);if(px>=0 && px<380 && py>50 && py<220)label_at(r,n,px,py,FIELD_NUMBER_Z,.48f,white);
        }
    }
    for(unsigned yard=11;yard<110;++yard)for(unsigned hash=0;hash<2;++hash)world(cam,(float)yard,hash?29.75f:23.58f,.12f,.5f,.04f,white);
    world(cam,0,0,120,.18f,.05f,white);world(cam,0,53.15f,120,.18f,.05f,white);
    const FootballLive *m=&c->live;float dir=m->rules.offense==FB_HOME?1:-1;
    world(cam,m->los,0,.18f,53.333f,.05f,C2D_Color32(74,163,255,255));
    float first=m->rules.offense==FB_HOME?10+m->rules.first_down_at:110-m->rules.first_down_at;
    if(!m->special && !m->turnover)world(cam,first,0,.18f,53.333f,.05f,gold); /* nobody is going for a first down on a run-back */
    route_lines(m,cam);
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
    if(m->rules.offense==FB_HOME && !m->caught && !m->special && m->phase!=FB_LIVE_DEAD && m->phase!=FB_LIVE_END){static const unsigned targets[5]={2,3,4,5,1};
        for(unsigned i=0;i<5;++i){const FootballLiveActor *a=&m->actors[targets[i]];float x=bound(football_camera_x(cam,a->x),12,382),y=bound(football_camera_y(cam,a->y)-34,50,217);char n[3];snprintf(n,sizeof n,"%u",i+1);rect(x-7,y,18,19,.7f,navy);label(r,n,x-3,y,.5f,gold);}
    }
    float bx=football_camera_x(cam,m->ball_x),by=football_camera_y(cam,m->ball_y)-m->ball_z*5-football_live_fx_ball_lift(m);
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
            /* Points are steps of the species' candy bar: a legendary earns a sliver of a candy, a common Pokemon one or more. */
            const unsigned milli=football_live_art_candy_milli(c,member,candy);
            char gain[48];snprintf(gain,sizeof gain,"%.14s  +%u.%02u CANDY",football_live_art_species(c->roster.mon[member].b.species),milli/1000,milli%1000/10);
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
/* The cards of the playcall are classic windows with the emerald font: the window is the card, a yellow frame marks the one chosen, and
 * the diagram is drawn over it in plain rectangles. */
/* The cards below are on the bottom screen, drawn with the classic UI's own rectangles (depth 0, in drawing order), so a window pushed over
 * them, like the pause menu, covers them: a rectangle drawn with a depth of its own would show through it. */
void football_live_art_card(FootballRenderer *r,FootballPlayId id,float x,float y,float w,float h,bool selected)
{
    const FootballPlayDefinition *p=football_playbook_get(id);if(!p || !r)return;
    rr_color gold=RR_RGBA(255,210,63,255),white=RR_RGBA(246,246,242,255);
    football_menu_panel(x,y,w,h);if(selected)football_menu_select(x,y,w,h);
    football_menu_text(selected?TS_SUMMARY_GOLD:TS_WINDOW,FM_PX_BODY,x+10,y+8,w-20,p->name);
    for(unsigned i=0;i<5;++i)rr_rect(x+w*.5f-12+i*6,y+36,4,4,white);
    unsigned read=football_playbook_primary_receiver(p),rank=read==4?1:read+2;
    FootballRouteView path=p->type==FB_PLAY_PASS?football_playbook_receiver_path(p,rank):(FootballRouteView){p->route,p->route_count};
    float depth=1,back=0,lateral=1;for(unsigned i=0;i<path.count;++i){depth=fmaxf(depth,path.point[i].x);back=fminf(back,path.point[i].x);lateral=fmaxf(lateral,fabsf(path.point[i].y));}
    float scale_x=(h-46)/(depth-back),scale_y=fminf(2,(w*.4f)/lateral);
    float origin_y=y+h-12+back*scale_x;
    float last_x=x+w*.5f,last_y=origin_y;
    rr_rect(last_x-2,last_y-2,5,5,gold);
    for(unsigned i=0;i<18;++i){FootballRoutePoint point=football_playbook_sample_path(path,i/17.0f);float px=x+w*.5f+point.y*scale_y,py=origin_y-point.x*scale_x;
        rr_rect(px,py,3,3,gold);last_x=px;last_y=py;}
    rr_rect(last_x-2,last_y-3,7,3,gold);
}

const char *football_live_art_special_name(FootballSpecial kind)
{
    return kind==FB_SPECIAL_PUNT?"PUNT":kind==FB_SPECIAL_FIELD_GOAL?"FIELD GOAL":kind==FB_SPECIAL_EXTRA_POINT?"EXTRA POINT":kind==FB_SPECIAL_KICKOFF?"KICKOFF":"GO FOR TWO";
}
void football_live_art_special_card(FootballRenderer *r,FootballSpecial kind,const char *detail,float x,float y,float w,float h,bool selected,bool enabled)
{
    if(!r)return;
    rr_color gold=RR_RGBA(255,210,63,255),white=RR_RGBA(246,246,242,255),dim=RR_RGBA(84,84,96,255);
    football_menu_panel(x,y,w,h);if(selected && enabled)football_menu_select(x,y,w,h);
    football_menu_text(!enabled?TS_SHADOW_TEXT:selected?TS_SUMMARY_GOLD:TS_WINDOW,FM_PX_BODY,x+10,y+8,w-20,football_live_art_special_name(kind));
    if(detail)football_menu_text(enabled?TS_WINDOW:TS_SHADOW_TEXT,FM_PX_TINY,x+10,y+25,w-20,detail);
    if(kind==FB_SPECIAL_PUNT){
        /* A high arc down the field. */
        for(unsigned i=0;i<20;++i){float t=i/19.0f;rr_rect(x+14+t*(w-40),y+h-14-4*t*(1-t)*(h-58)*1.6f,3,3,enabled?gold:dim);}
    }else if(kind==FB_SPECIAL_NONE){
        /* The try as a play: the end zone line, the ball carrier and the defenders standing in front of it. */
        rr_rect(x+12,y+42,w-24,2,enabled?gold:dim);
        for(unsigned i=0;i<4;++i)rr_rect(x+w*.25f+i*(w*.5f/3)-2,y+h-30,5,5,enabled?white:dim);
        rr_rect(x+w*.5f-3,y+h-20,7,7,enabled?gold:dim);
        for(unsigned i=0;i<8;++i)rr_rect(x+w*.5f,y+h-24-i*(h-70)/8,2,2,enabled?white:dim);
    }else{
        /* The uprights, with the kick on its way. */
        float px=x+w-34;
        rr_rect(px,y+40,2,22,enabled?gold:dim);rr_rect(px+14,y+40,2,22,enabled?gold:dim);rr_rect(px,y+62,16,2,enabled?gold:dim);rr_rect(px+7,y+62,2,8,enabled?gold:dim);
        for(unsigned i=0;i<12;++i){float t=i/11.0f;rr_rect(x+14+t*(w-58),y+h-14-t*(h-50)-2*t*(1-t)*20,3,3,enabled?white:dim);}
    }
}

void football_live_art_defense_card(FootballRenderer *r,FootballDefenseId id,float x,float y,float w,float h,bool selected)
{
    const FootballDefenseDefinition *d=football_defense_get(id);if(!d || !r)return;
    rr_color gold=RR_RGBA(255,210,63,255),blue=RR_RGBA(76,201,240,255);
    football_menu_panel(x,y,w,h);if(selected)football_menu_select(x,y,w,h);
    football_menu_text(selected?TS_SUMMARY_GOLD:TS_SUMMARY_BLUE,FM_PX_BODY,x+10,y+8,w-20,d->name);
    unsigned rush=0,deep=0;
    for(unsigned rank=0;rank<11;++rank){rush+=football_defense_is_rusher(id,rank);deep+=(football_defense_deep_mask(id)&(1u<<rank))!=0;}
    /* The diagram shows this call's actual rush and shell rather than the same
     * five dots on every card. Top is deep; the rush arrows point to the QB. */
    for(unsigned row=0;row<3;++row){unsigned count=row==0?deep:row==1?11-rush-deep:rush;
        for(unsigned i=0;i<count;++i){float px=x+12+(i+.5f)*(w-24)/count,py=y+34+row*14;
            rr_rect(px-2,py,5,5,row==2?gold:blue);
            if(row==2){rr_rect(px,py+4,1,7,gold);rr_rect(px-2,py+8,5,1,gold);}
            else if(football_defense_is_zone(id))rr_rect(px-6,py+5,13,1,blue);
            else if(row==1)rr_rect(px,py+4,1,6,blue);
        }
    }
}
