#include "football_stadium.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The stadium is rectangles only: every one lies in the field window, stays behind the yard lines (z < .04), and the cost follows what the
 * camera can see (a few dozen in the middle of the field, more where a whole stand shows) and never the frame time. Set STADIUM_PPM=dir to
 * write the views as PPM pictures (painter's order by z), for looking at. */
typedef struct { float x,y,z,w,h;u32 color; } Rect;
static Rect rects[4096];static unsigned count;
bool C2D_DrawRectSolid(float x,float y,float z,float w,float h,u32 color)
{
    assert(count<4096 && isfinite(x) && isfinite(y) && isfinite(z) && isfinite(w) && isfinite(h) && w>0 && h>0);
    rects[count++]=(Rect){x,y,z,w,h,color};return true;
}
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static unsigned draw(float x,float y,unsigned round,unsigned steps,bool cheering)
{
    FootballCamera cam={.x=x,.y=y,.ready=true};count=0;football_stadium_draw(&cam,round,steps,cheering);return count;
}
static void ppm(const char *dir,const char *name)
{
    static unsigned char px[400*240*3];memset(px,20,sizeof px);
    static Rect order[4096];memcpy(order,rects,count*sizeof(Rect));
    /* An insertion sort by depth keeps equal depths in the order they were submitted, as the hardware does. */
    for(unsigned i=1;i<count;++i){Rect t=order[i];unsigned j=i;while(j>0 && order[j-1].z>t.z){order[j]=order[j-1];--j;}order[j]=t;}
    for(unsigned i=0;i<count;++i){
        int x0=(int)floorf(order[i].x),y0=(int)floorf(order[i].y),x1=(int)ceilf(order[i].x+order[i].w),y1=(int)ceilf(order[i].y+order[i].h);
        for(int y=y0<0?0:y0;y<y1 && y<240;++y)for(int x=x0<0?0:x0;x<x1 && x<400;++x){unsigned char *p=&px[(y*400+x)*3];p[0]=order[i].color&255;p[1]=(order[i].color>>8)&255;p[2]=(order[i].color>>16)&255;}
    }
    char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n400 240\n255\n");fwrite(px,1,sizeof px,f);fclose(f);
}
int main(void)
{
    static const float xs[]={12,30,60,90,108},ys[]={7,14,26.67f,40,46.333f};
    unsigned most=0,least=100000,central=0;
    for(unsigned xi=0;xi<5;++xi)for(unsigned yi=0;yi<5;++yi)for(unsigned round=1;round<=7;round+=3){
        unsigned n=draw(xs[xi],ys[yi],round,1234,false);
        for(unsigned i=0;i<n;++i){
            /* Inside the field window, behind the lines, and visible. */
            CHECK(rects[i].x>=0 && rects[i].y>=44 && rects[i].x+rects[i].w<=400.001f && rects[i].y+rects[i].h<=240.001f);
            CHECK(rects[i].z>0 && rects[i].z<.04f);
        }
        if(n>most){most=n;}
        if(n<least){least=n;}
        if(xs[xi]==60 && ys[yi]==26.67f)central=n;
        CHECK(n<420);
    }
    CHECK(central>10 && central<130 && most>central && least>0);
    /* The frame time changes nothing, and neither does the crowd's mood change what is drawn: only where the fans are. */
    for(unsigned yi=0;yi<5;++yi){
        unsigned a=draw(40,ys[yi],2,100,false),b=draw(40,ys[yi],2,9999,false),c=draw(40,ys[yi],2,100,true);
        CHECK((a>b?a-b:b-a)<=8 && (a>c?a-c:c-a)<=12); /* a fan lifted past the scoreboard's edge is clipped away */
    }
    draw(40,7,2,100,false);Rect quiet[4096];unsigned n=count;memcpy(quiet,rects,n*sizeof(Rect));
    double quiet_y=0,cheer_y=0;for(unsigned i=0;i<n;++i)quiet_y+=quiet[i].y;
    draw(40,7,2,100,true);for(unsigned i=0;i<count;++i)cheer_y+=rects[i].y;
    CHECK(count+12>=n && n+12>=count && cheer_y/count<quiet_y/n-.1); /* the cheering crowd is up on its feet */
    draw(40,7,2,100,false);CHECK(!memcmp(quiet,rects,n*sizeof(Rect))); /* same inputs, same picture */
    /* Night is darker than day. */
    draw(60,26.67f,1,0,false);unsigned long day=0;for(unsigned i=0;i<count;++i)day+=(rects[i].color&255)+((rects[i].color>>8)&255)+((rects[i].color>>16)&255);
    unsigned dn=count;draw(60,26.67f,7,0,false);unsigned long night=0;for(unsigned i=0;i<count;++i)night+=(rects[i].color&255)+((rects[i].color>>8)&255)+((rects[i].color>>16)&255);
    CHECK(dn==count && night<day);
    CHECK(draw(60,26.67f,0,0,false)>0 && draw(60,26.67f,99,0,false)>0); /* a bad round is still a stadium */
    football_stadium_draw(NULL,1,0,false);
    const char *dir=getenv("STADIUM_PPM");
    if(dir){
        draw(60,26.67f,1,0,false);ppm(dir,"midfield-day");
        draw(36,8,1,0,false);ppm(dir,"far-sideline-day");
        draw(24,42,4,0,false);ppm(dir,"near-sideline-dusk");
        draw(12,26.67f,7,0,true);ppm(dir,"end-zone-night-cheer");
    }
    printf("stadium: %u to %u rectangles a frame (%u at midfield), %u checks\n",least,most,central,checks);
    puts("football stadium tests passed");
    return 0;
}
