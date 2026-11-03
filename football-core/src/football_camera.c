#include "football_camera.h"
#include <math.h>
static float clamp(float x,float a,float b){return x<a?a:x>b?b:x;}
void football_camera_follow(FootballCamera *c,float x,float y,float vx,float ms)
{
    if(!c || !isfinite(x) || !isfinite(y) || !isfinite(vx) || !isfinite(ms))return;
    float tx=clamp(x+clamp(vx*.35f,-4,4),12,108),ty=clamp(y,7,46.333f);
    if(!c->ready){c->x=tx;c->y=ty;c->ready=true;return;}
    float dt=clamp(ms,0,100)*.001f,gap=fabsf(tx-c->x);
    /* The easing is gentle (18 yd/s at most) while the target is in view. A target that is off the screen, or about to be (a switch to a
     * defender across the field), is caught up quickly: the pace grows with the distance past the edge of the view. */
    float rate=3.2f,speed=18*dt;
    if(gap>FB_CAMERA_YARDS*.4f){rate=12;speed=fmaxf(speed,(gap-FB_CAMERA_YARDS*.3f)*9*dt);}
    float alpha=1-expf(-rate*dt);
    c->x+=clamp((tx-c->x)*alpha,-speed,speed);
    c->y+=clamp((ty-c->y)*alpha,-speed*.7f,speed*.7f);
}
float football_camera_x(const FootballCamera *c,float x){return 200+(x-c->x)*FB_CAMERA_SCALE;}
float football_camera_y(const FootballCamera *c,float y){return 141+(y-c->y)*FB_CAMERA_SCALE*FB_CAMERA_SQUASH;}
