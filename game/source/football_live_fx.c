#include "football_live_fx.h"
#include "football_live_systems.h"
#include <math.h>
#include <string.h>
#define PI_F 3.14159265f
static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static float ease_out(float t){t=clampf(t,0,1);return 1-(1-t)*(1-t);}
/* 0 -> 1 -> 0 as t goes 0 -> 1, and 0 outside. */
static float arc(float t){return sinf(clampf(t,0,1)*PI_F);}
/* Seconds since the moment, or -1 for one that has not happened yet (or never did). */
static float age_of(const FootballLive *m,const FootballFx *f)
{
    if(f->kind==FB_FX_NONE)return -1;
    int32_t d=(int32_t)(m->steps-f->step);
    return d<0?-1:(float)d*FB_LIVE_STEP;
}
static u32 tone(unsigned r,unsigned g,unsigned b,float alpha){return C2D_Color32(r,g,b,(unsigned)(255*clampf(alpha,0,1)));}
/* Same clipping as the field art: nothing outside the field window, nothing degenerate. */
static void box(float x,float y,float w,float h,float z,u32 color)
{
    float right=fminf(400,x+w),bottom=fminf(240,y+h);x=fmaxf(x,0);y=fmaxf(y,44);
    if(right>x && bottom>y)C2D_DrawRectSolid(x,y,z,right-x,bottom-y,color);
}
static void words(FootballRenderer *r,const char *s,float x,float y,float scale,u32 color)
{
    C2D_Text t;C2D_TextParse(&t,r->text,s);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,x,y,.9f,scale,scale,color);
}
FootballFxPose football_live_fx_pose(const FootballLive *m,unsigned actor)
{
    FootballFxPose p={0,0,0,1,1};
    if(!m || actor>=22)return p;
    for(unsigned i=0;i<FB_FX_COUNT;++i){
        const FootballFx *f=&m->fx[i];float age=age_of(m,f),s=(float)f->side;
        if(age<0)continue;
        bool doer=f->actor==actor,done_to=f->other==actor;
        switch(f->kind){
        case FB_FX_JUKE:
            /* A feint: a shimmy that settles, a small hop, a quick stretch. */
            if(doer && age<.5f){float t=age/.5f;p.angle+=FB_FX_TURN*s*.32f*sinf(t*3*PI_F)*(1-t);p.dy-=3*arc(t);p.sx+=.1f*arc(t);p.sy-=.1f*arc(t);}
            break;
        case FB_FX_DIVE:
            /* Launch, lie flat, get up. */
            if(doer && age<1.4f){
                float k=ease_out(age/.3f)*(1-clampf((age-1)/.4f,0,1));
                p.dx+=s*16*k;p.dy+=-10*arc(age/.3f)+5*k;p.angle+=FB_FX_TURN*s*1.4f*k;p.sx+=.18f*k;p.sy-=.2f*k;
            }
            break;
        case FB_FX_TACKLE:case FB_FX_SACK:{
            bool sack=f->kind==FB_FX_SACK;
            if(doer){float t=age/(sack?.35f:.3f);p.dx+=s*(sack?10:5)*arc(t);p.angle+=FB_FX_TURN*s*(sack?.5f:0)*arc(t);p.sy-=.08f*arc(t);}
            if(done_to){float k=ease_out(age/(sack?.3f:.25f));p.angle+=FB_FX_TURN*s*(sack?1.5f:1.45f)*k;p.dx+=s*(sack?12:6)*k;p.dy+=(sack?5:4)*k;p.sx+=.13f*k;p.sy-=.19f*k;}
            break;}
        case FB_FX_FUMBLE:
            /* The carrier is spun round, the tackler drives in. */
            if(doer && age<.6f){p.angle+=FB_FX_TURN*s*.9f*arc(age/.6f);p.dy-=3*arc(age/.6f);}
            if(done_to && age<.4f)p.dx+=s*6*arc(age/.4f);
            break;
        case FB_FX_INTERCEPTION:
            /* Two jumps for the ball, a squash on each landing; the receiver it was meant for turns away. */
            if(doer && age<.9f){float h=fabsf(sinf(age/.45f*PI_F));p.dy-=13*h;p.sy+=.08f*h-.1f*(1-h);p.angle+=.12f*sinf(age*14)*(1-age/.9f);}
            if(done_to && age<1.2f){float k=ease_out(age/.3f);p.angle+=FB_FX_TURN*s*.4f*k;p.dy+=2*k;}
            break;
        case FB_FX_CATCH:
            if(doer && age<.3f)p.dy-=5*arc(age/.3f);
            break;
        case FB_FX_TOUCHDOWN:{
            /* The scorer jumps three times; the teammates around join in a beat apart. */
            if(doer && age<2.1f){float h=fabsf(sinf(age/.7f*PI_F));p.dy-=17*h;p.sy+=.1f*h-.12f*(1-h);p.angle+=.35f*sinf(age*9)*(1-age/2.1f);}
            else if(!doer && (actor<11)==(f->actor<11) && hypotf(m->actors[actor].x-f->x,m->actors[actor].y-f->y)<15){
                float t=age-(float)(actor%5)*.11f;
                if(t>0 && t<1.8f)p.dy-=9*fabsf(sinf(t/.6f*PI_F));
            }
            break;}
        default:break;
        }
    }
    return p;
}
void football_live_fx_shake(const FootballLive *m,float *dx,float *dy)
{
    if(!dx || !dy)return;
    *dx=*dy=0;
    if(!m)return;
    for(unsigned i=0;i<FB_FX_COUNT;++i){
        const FootballFx *f=&m->fx[i];float age=age_of(m,f),amp=0,life=0;
        switch(f->kind){
        case FB_FX_TACKLE:amp=2.2f;life=.25f;break;
        case FB_FX_SACK:amp=4.5f;life=.4f;break;
        case FB_FX_FUMBLE:amp=2.5f;life=.25f;break;
        case FB_FX_INTERCEPTION:amp=3.5f;life=.35f;break;
        case FB_FX_TOUCHDOWN:amp=3.5f;life=.35f;break;
        default:break;
        }
        if(age>=0 && age<life){float k=1-age/life;*dx+=amp*k*sinf(age*95);*dy+=amp*k*cosf(age*83);}
    }
}
float football_live_fx_banner_scale(const FootballLive *m)
{
    float scale=1;
    if(!m)return scale;
    for(unsigned i=0;i<FB_FX_COUNT;++i){
        const FootballFx *f=&m->fx[i];float age=age_of(m,f);
        if((f->kind==FB_FX_TOUCHDOWN || f->kind==FB_FX_INTERCEPTION || f->kind==FB_FX_SACK || f->kind==FB_FX_FUMBLE) && age>=0 && age<.3f){
            float k=1-age/.3f;scale=fmaxf(scale,1+.55f*k*k);
        }
    }
    return scale;
}
/* Lines that trail a runner, longest at the start of a move. */
static void streaks(const FootballLive *m,const FootballCamera *cam,unsigned actor,float strength)
{
    const FootballLiveActor *a=&m->actors[actor];
    float x=football_camera_x(cam,a->x),y=football_camera_y(cam,a->y),back=a->facing<0?1.f:-1.f;
    for(unsigned k=0;k<3;++k){
        float length=16*strength+6*k*strength,oy=-10+8.f*k;
        box(back>0?x+10:x-10-length,y+oy,length,1.5f,.14f,tone(238,244,234,.7f*strength));
    }
}
/* A ring of sparks around a hit. */
static void star(float x,float y,float age,float life,unsigned spikes,float reach)
{
    if(age<0 || age>=life)return;
    float fade=1-age/life;
    if(age<.12f)box(x-10*(1-age/.12f),y-6*(1-age/.12f),20*(1-age/.12f),12*(1-age/.12f),.5f,tone(255,210,63,.85f));
    for(unsigned k=0;k<spikes;++k){
        float angle=(float)k*2*PI_F/(float)spikes+(k%2)*.2f,radius=4+age*reach;
        for(unsigned j=0;j<3;++j){
            float r=radius-4.f*j,size=4-(float)j;
            if(r<0)continue;
            box(x+cosf(angle)*r-size*.5f,y+sinf(angle)*r*.62f-size*.5f,size,size,.5f,k%2?tone(238,244,234,fade):tone(255,210,63,fade));
        }
    }
}
/* A square ring that grows and thins out. */
static void ring(float x,float y,float t,u32 color)
{
    if(t<0 || t>1)return;
    float half=5+30*ease_out(t),hy=half*.62f,thick=2;
    box(x-half,y-hy,half*2,thick,.55f,color);box(x-half,y+hy-thick,half*2,thick,.55f,color);
    box(x-half,y-hy,thick,hy*2,.55f,color);box(x+half-thick,y-hy,thick,hy*2,.55f,color);
}
static void puffs(float x,float y,float age,float life,unsigned count)
{
    if(age<0 || age>=life)return;
    float fade=1-age/life;
    for(unsigned k=0;k<count;++k){
        float angle=(float)k*2*PI_F/(float)count,r=age*38,size=4+age*14;
        box(x+cosf(angle)*r-size*.5f,y-fabsf(sinf(angle))*r*.5f-size*.5f,size,size*.7f,.12f,tone(214,190,140,.7f*fade));
    }
}
static void popup(FootballRenderer *r,const char *s,float x,float y,float age,float life)
{
    if(age<0 || age>=life)return;
    float rise=14*ease_out(age/.5f),fade=age<life*.6f?1:1-(age-life*.6f)/(life*.4f);
    words(r,s,x-(float)strlen(s)*5.f,y-rise,.6f,tone(255,210,63,fade));
}
void football_live_fx_draw(FootballRenderer *r,const FootballCareer *c,const FootballCamera *cam)
{
    if(!r || !c || !cam)return;
    const FootballLive *m=&c->live;
    for(unsigned i=0;i<FB_FX_COUNT;++i){
        const FootballFx *f=&m->fx[i];float age=age_of(m,f);
        if(age<0)continue;
        float px=football_camera_x(cam,f->x),py=football_camera_y(cam,f->y),s=(float)f->side;
        const FootballLiveActor *who=&m->actors[f->actor];
        float ax=football_camera_x(cam,who->x),ay=football_camera_y(cam,who->y);
        switch(f->kind){
        case FB_FX_JUKE:
            if(age<.5f){
                float t=age/.5f,back=who->facing<0?1.f:-1.f;
                /* Two fading copies of the runner's outline, then the lines behind. */
                for(unsigned k=1;k<=2;++k)box(ax+back*6*k-9,ay-15,18,25,.15f,tone(who->home?76:232,who->home?201:53,who->home?240:74,(1-t)*.35f/(float)k));
                streaks(m,cam,f->actor,1-t);
            }
            break;
        case FB_FX_DIVE:
            if(age<.3f)streaks(m,cam,f->actor,1-age/.3f);
            puffs(px,py+8,age,.6f,6);
            puffs(px+s*16,py+8,age-.25f,.6f,5);
            break;
        case FB_FX_TACKLE:
            star(px,py-8,age,.35f,8,90);
            break;
        case FB_FX_SACK:
            star(px,py-8,age,.45f,12,130);
            puffs(px,py+8,age,.6f,6);
            break;
        case FB_FX_FUMBLE:
            star(px,py-8,age,.3f,6,70);
            popup(r,"FUMBLE!",ax,ay-42,age,.9f);
            break;
        case FB_FX_INTERCEPTION:
            ring(px,py-8,age/.5f,tone(255,210,63,1-age/.5f));
            ring(px,py-8,(age-.12f)/.5f,tone(238,244,234,1-(age-.12f)/.5f));
            star(px,py-8,age,.35f,8,80);
            popup(r,"PICK!",ax,ay-42,age,1.1f);
            break;
        case FB_FX_CATCH:
            ring(px,py-8,age/.3f,tone(238,244,234,(1-age/.3f)*.6f));
            break;
        case FB_FX_TOUCHDOWN:
            if(age<.3f)box(0,44,400,196,.74f,tone(255,255,255,(1-age/.3f)*.6f));
            if(age<2.4f){
                static const unsigned palette[5][3]={{255,210,63},{238,244,234},{76,201,240},{232,53,74},{255,140,40}};
                for(unsigned k=0;k<40;++k){
                    float t=age-(float)(k%10)*.04f;
                    if(t<0)continue;
                    unsigned h=k*2654435761u;
                    float x=(float)((h>>8)%400)+sinf(t*3.2f+(float)k)*10,y=36+t*(55.f+(float)((h>>17)%50)),flip=fabsf(sinf(t*9+(float)k));
                    float fade=age>1.9f?(2.4f-age)/.5f:1;
                    const unsigned *col=palette[k%5];
                    box(x,y,2+flip*3,5-flip*3,.5f,tone(col[0],col[1],col[2],fade));
                }
            }
            break;
        default:break;
        }
    }
}
