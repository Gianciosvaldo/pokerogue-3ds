#include "football_stadium.h"
#include "football_goal.h"
#include <math.h>
/* Depth layers, back to front. The yard lines are at .04, so nothing here may reach it. */
#define Z_TURF .005f
#define Z_MOW .008f
#define Z_ZONE .012f
#define Z_ZONE_MARK .014f
#define Z_LOGO .016f
#define Z_APRON .02f
#define Z_BENCH .022f
#define Z_WALL .024f
#define Z_STAND .026f
#define Z_FAN .028f
#define Z_POST .03f
#define PITCH_W 120.0f
#define PITCH_H 53.333f
static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
/* The same window as the field art: nothing above the scoreboard, nothing off the screen, nothing degenerate. */
static void rect(float x,float y,float w,float h,float z,u32 color)
{
    float right=fminf(400,x+w),bottom=fminf(240,y+h);x=fmaxf(x,0);y=fmaxf(y,44);
    if(right>x && bottom>y)C2D_DrawRectSolid(x,y,z,right-x,bottom-y,color);
}
static void world(const FootballCamera *c,float x,float y,float w,float h,float z,u32 color)
{
    rect(football_camera_x(c,x),football_camera_y(c,y),w*FB_CAMERA_SCALE,h*FB_CAMERA_SCALE*FB_CAMERA_SQUASH,z,color);
}
static bool seen(const FootballCamera *c,float x,float y,float w,float h)
{
    float x0=football_camera_x(c,x),y0=football_camera_y(c,y),x1=x0+w*FB_CAMERA_SCALE,y1=y0+h*FB_CAMERA_SCALE*FB_CAMERA_SQUASH;
    return x1>0 && x0<400 && y1>44 && y0<240;
}
static unsigned hash2(unsigned a,unsigned b)
{
    unsigned h=a*73856093u^b*19349663u^0x9e3779b9u;
    h^=h>>13;h*=0x5bd1e995u;h^=h>>15;
    return h;
}
typedef struct { unsigned r,g,b; } Rgb;
static u32 shade(Rgb c,float k){return C2D_Color32((unsigned)clampf((float)c.r*k,0,255),(unsigned)clampf((float)c.g*k,0,255),(unsigned)clampf((float)c.b*k,0,255),255);}
/* Time of day by round: how much of the daylight is left. */
static float light(unsigned round){return round<=2?1.0f:round<=5?.88f:.74f;}
static const Rgb END_HOME={30,65,130},END_AWAY={143,29,38};
static const Rgb FANS_HOME[6]={{58,134,255},{76,201,240},{238,244,234},{30,65,130},{255,210,63},{154,193,255}};
static const Rgb FANS_AWAY[6]={{232,53,74},{255,210,63},{238,244,234},{143,29,38},{255,140,40},{255,168,160}};
static const Rgb BOARDS[6]={{20,26,46},{232,53,74},{255,210,63},{238,244,234},{58,134,255},{111,196,92}};
/* One fan: a colored head and shirt that belong to the side of the field the stand is on, now and then a neutral one. */
static Rgb fan_color(float x,unsigned id)
{
    unsigned h=hash2(id,17u);
    if(h%5==0)return (Rgb){(unsigned)(70+h%90),(unsigned)(70+(h>>8)%90),(unsigned)(70+(h>>16)%90)};
    return x<PITCH_W*.5f?FANS_HOME[(h>>3)%6]:FANS_AWAY[(h>>3)%6];
}
/* How high a fan is lifted (pixels, upward): a slow wave in a quiet crowd, everyone on their feet when it cheers. */
static float lift_of(unsigned id,unsigned steps,bool cheering)
{
    unsigned h=hash2(id,5u);
    if(cheering)return ((steps/7u+h)&1u)?4.f:0.f;
    return h%6==0 && ((steps/45u+(h>>4))&1u)?1.f:0.f;
}
/* One horizontal stand: `rows` bands from the wall outward (screen up for the far side, down for the near side), each with its fans. */
static void stand_side(const FootballCamera *cam,bool far_side,float light_k,unsigned steps,bool cheering)
{
    const float row_h=1.25f,apron=2.5f,wall_h=1.4f;
    const unsigned rows=far_side?5u:4u;
    float edge=far_side?0.f:PITCH_H;
    float apron_y=far_side?-apron:edge;
    /* The apron: where the benches stand. */
    world(cam,-1.5f,apron_y,PITCH_W+3.f,apron,Z_APRON,shade((Rgb){190,183,157},light_k));
    float wall_y=far_side?-apron-wall_h:edge+apron;
    if(!seen(cam,0,far_side?wall_y-row_h*(float)rows:wall_y,PITCH_W,wall_h+row_h*(float)rows))return;
    /* Ad boards along the wall: eight yards each, a pale rail on top. */
    float left=(floorf((cam->x-24.f)/8.f))*8.f;
    for(float x=fmaxf(left,-1.5f);x<fminf(cam->x+24.f,PITCH_W+1.5f);x+=8.f){
        unsigned id=(unsigned)((x+8.f)/8.f);
        world(cam,x,wall_y,8.f,wall_h,Z_WALL,shade(BOARDS[hash2(id,3u)%6],light_k));
        world(cam,x,far_side?wall_y:wall_y+wall_h-.25f,8.f,.25f,Z_WALL+.001f,shade((Rgb){238,244,234},light_k));
    }
    for(unsigned row=0;row<rows;++row){
        float y=far_side?wall_y-row_h*(float)(row+1):wall_y+wall_h+row_h*(float)row;
        if(!seen(cam,-1.5f,y,PITCH_W+3.f,row_h))continue;
        Rgb band=far_side?(Rgb){58,52,74}:(Rgb){34,32,48};
        world(cam,-1.5f,y,PITCH_W+3.f,row_h,Z_STAND,shade(band,light_k*(.8f+.05f*(float)row)));
        float first=floorf((cam->x-24.f)/1.8f)*1.8f;
        for(float x=fmaxf(first,-1.5f);x<fminf(cam->x+24.f,PITCH_W+.3f);x+=1.8f){
            unsigned col=(unsigned)((x+2.f)/1.8f),id=col*31u+row*7u+(far_side?0u:1000u);
            if(col%15u==7u)continue; /* an aisle */
            float lift=lift_of(id,steps,cheering);
            Rgb c=fan_color(x,id);
            float px=football_camera_x(cam,x+.2f),py=football_camera_y(cam,y+.15f)-lift;
            if(far_side){rect(px,py,9,5,Z_FAN,shade(c,light_k));}
            else{rect(px,py,9,4,Z_FAN,shade((Rgb){40,34,32},light_k));rect(px,py+4,9,4,Z_FAN,shade(c,light_k*.8f));} /* seen from behind: hair, then the shirt */
        }
    }
}
/* The stand behind an end line: a vertical wall of boards, then columns of fans. */
static void stand_end(const FootballCamera *cam,bool left_end,float light_k,unsigned steps,bool cheering)
{
    const float col_w=1.4f,wall_w=1.2f;
    const unsigned cols=5;
    float edge=left_end?0.f:PITCH_W;
    float wall_x=left_end?edge-wall_w:edge;
    float span=left_end?wall_x-col_w*(float)cols:wall_x;
    if(!seen(cam,span,-3.f,wall_w+col_w*(float)cols,PITCH_H+6.f))return;
    float top=floorf((cam->y-18.f)/3.f)*3.f;
    for(float y=fmaxf(top,-3.f);y<fminf(cam->y+22.f,PITCH_H+3.f);y+=3.f){
        unsigned id=(unsigned)((y+3.f)/3.f);
        world(cam,wall_x,y,wall_w,3.f,Z_WALL,shade(BOARDS[hash2(id,left_end?9u:11u)%6],light_k));
    }
    for(unsigned col=0;col<cols;++col){
        float x=left_end?wall_x-col_w*(float)(col+1):wall_x+wall_w+col_w*(float)col;
        world(cam,x,-3.f,col_w,PITCH_H+6.f,Z_STAND,shade((Rgb){58,52,74},light_k*(.8f+.05f*(float)col)));
        float first=floorf((cam->y-18.f)/2.2f)*2.2f;
        for(float y=fmaxf(first,-2.6f);y<fminf(cam->y+22.f,PITCH_H+2.f);y+=2.2f){
            unsigned row=(unsigned)((y+3.f)/2.2f),id=row*29u+col*13u+(left_end?2000u:3000u);
            if(row%9u==4u)continue;
            float lift=lift_of(id,steps,cheering);
            float px=football_camera_x(cam,x+.2f),py=football_camera_y(cam,y)-lift;
            rect(px,py,9,5,Z_FAN,shade(fan_color(left_end?20.f:100.f,id),light_k));
        }
    }
}
/* A bench on the far apron: a long seat and the players on it in the team's color. */
static void bench(const FootballCamera *cam,float x,Rgb team,float light_k)
{
    if(!seen(cam,x,-2.3f,12.f,1.6f))return;
    world(cam,x,-1.35f,12.f,.55f,Z_BENCH,shade((Rgb){92,60,36},light_k));
    for(unsigned i=0;i<6;++i)world(cam,x+.5f+1.9f*(float)i,-2.2f,1.2f,1.0f,Z_BENCH+.001f,shade(team,light_k));
}
/* An ellipse out of horizontal slices: the midfield ball. */
static void ellipse(const FootballCamera *cam,float cx,float cy,float a,float b,unsigned slices,float z,float light_k,Rgb top,Rgb bottom,Rgb mid)
{
    for(unsigned i=0;i<slices;++i){
        float t=((float)i+.5f)/(float)slices*2.f-1.f,half=a*sqrtf(fmaxf(0,1-t*t));
        float y=cy-b+(float)i*2.f*b/(float)slices;
        Rgb c=fabsf(t)<.14f?mid:t<0?top:bottom;
        world(cam,cx-half,y,half*2.f,2.f*b/(float)slices+.02f,z,shade(c,light_k));
    }
}
/* A goal post. The field is seen from the side and above (FB_CAMERA_SQUASH), where a real post is no help: the crossbar runs along the width
 * of the field and the uprights stand on its ends, so height and width are both "up the screen" and the whole thing is one vertical line
 * with gaps. So the post is drawn as the picture everyone knows, a pole with a crossbar and two uprights, standing on the ground at the
 * middle of the goal. It stands where the kicks are aimed (football_goal.h: FB_GOAL_POST_INSET yards inside each end line, the middle of
 * the field's width), so the ball flies at it and a kick that is good ends at its foot while a miss ends well to one side. */
#define POST_STEM_H 16.f   /* pixels from the ground to the crossbar */
#define POST_UP_H 30.f     /* and from the crossbar to the tip of an upright */
#define POST_SPAN 26.f     /* the width between the uprights, in pixels */
static void goal_post(const FootballCamera *cam,float x,float light_k)
{
    float gx=football_camera_x(cam,x),gy=football_camera_y(cam,FB_GOAL_MIDDLE);
    if(gx<-40.f || gx>440.f || gy-POST_STEM_H-POST_UP_H>240.f || gy+4.f<44.f)return;
    u32 yellow=shade((Rgb){255,210,63},light_k),pole=shade((Rgb){226,230,222},light_k);
    rect(gx-7.f,gy-1.f,14.f,4.f,Z_POST-.001f,C2D_Color32(10,40,18,150));                       /* where it stands */
    rect(gx-1.5f,gy-POST_STEM_H,3.f,POST_STEM_H+1.f,Z_POST,pole);                              /* the pole */
    rect(gx-POST_SPAN*.5f,gy-POST_STEM_H-3.f,POST_SPAN,3.f,Z_POST+.001f,yellow);             /* the crossbar */
    rect(gx-POST_SPAN*.5f,gy-POST_STEM_H-POST_UP_H,3.f,POST_UP_H,Z_POST+.001f,yellow);       /* the uprights */
    rect(gx+POST_SPAN*.5f-3.f,gy-POST_STEM_H-POST_UP_H,3.f,POST_UP_H,Z_POST+.001f,yellow);
}
void football_stadium_draw(const FootballCamera *cam,unsigned round,unsigned steps,bool cheering)
{
    if(!cam)return;
    float k=light(round);
    /* Turf: five-yard bands, each mowed in a checker of two shades. */
    for(unsigned band=0;band<24;++band){
        float x=(float)band*5.f;
        if(!seen(cam,x,0,5.f,PITCH_H))continue;
        Rgb base=band%2?(Rgb){42,128,52}:(Rgb){47,143,58};
        world(cam,x,0,5.f,PITCH_H,Z_TURF,shade(base,k));
        for(unsigned row=0;row<4;row+=1)if((row+band)%2)world(cam,x,(float)row*PITCH_H/4.f,5.f,PITCH_H/4.f,Z_MOW,shade(base,k*.94f));
    }
    /* End zones: team colors with a lighter stripe every other 2.5 yards, and the painted border. */
    for(unsigned side=0;side<2;++side){
        float x=side?110.f:0.f;Rgb c=side?END_AWAY:END_HOME;
        if(!seen(cam,x,0,10.f,PITCH_H))continue;
        world(cam,x,0,10.f,PITCH_H,Z_ZONE,shade(c,k));
        for(unsigned s=0;s<4;s+=2)world(cam,x+(float)s*2.5f,0,2.5f,PITCH_H,Z_ZONE_MARK,shade(c,k*1.12f));
        u32 chalk=shade((Rgb){238,244,234},k*.9f);
        world(cam,x+(side?9.3f:.2f),1.2f,.5f,PITCH_H-2.4f,Z_ZONE_MARK,chalk);
        world(cam,side?x+.7f:x+.3f,1.2f,9.f,.35f,Z_ZONE_MARK,chalk);world(cam,side?x+.7f:x+.3f,PITCH_H-1.55f,9.f,.35f,Z_ZONE_MARK,chalk);
    }
    ellipse(cam,60.f,PITCH_H*.5f,5.6f,4.3f,10,Z_LOGO,k,(Rgb){10,13,20},(Rgb){10,13,20},(Rgb){10,13,20});
    ellipse(cam,60.f,PITCH_H*.5f,5.0f,3.8f,10,Z_LOGO+.001f,k,(Rgb){232,53,74},(Rgb){238,244,234},(Rgb){10,13,20});
    world(cam,58.9f,PITCH_H*.5f-1.0f,2.2f,2.0f,Z_LOGO+.002f,shade((Rgb){10,13,20},k));world(cam,59.4f,PITCH_H*.5f-.55f,1.2f,1.1f,Z_LOGO+.003f,shade((Rgb){238,244,234},k));
    /* The sidelines' surroundings. */
    stand_side(cam,true,k,steps,cheering);stand_side(cam,false,k,steps,cheering);
    stand_end(cam,true,k,steps,cheering);stand_end(cam,false,k,steps,cheering);
    bench(cam,22.f,END_HOME,k);bench(cam,86.f,END_AWAY,k);
    goal_post(cam,FB_GOAL_POST_INSET,k);goal_post(cam,PITCH_W-FB_GOAL_POST_INSET,k);
}
