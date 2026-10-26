#include "football_live_systems.h"
#include <math.h>
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
void football_live_defense_setup(FootballLive *m)
{
    if(!m)return;
    unsigned off=m->rules.offense==FB_HOME?0:11,def=11-off;
    float dir=off? -1:1;
    /* DL rush, LB covers WR3/RB/TE, corners cover WR1/WR2, two deep safeties.
     * A five-man blitz rotates the free safety onto the RB and keeps one high. */
    static const uint8_t man[11]={255,255,255,255,4,1,5,2,3,255,255};
    static const float depth[11]={2,2,2,2,7,8,7,10,10,18,18};
    static const float lane[11]={20,24,29,33,9,26.67f,44,12,41,16,38};
    for(unsigned rank=0;rank<11;++rank){
        m->coverage[rank]=man[rank]==255?255:(uint8_t)(off+man[rank]);
        m->zone_x[rank]=clamp(m->los+dir*depth[rank],1,119);m->zone_y[rank]=lane[rank];
        if(m->defense==FB_DEF_BLITZ){if(rank==5)m->coverage[rank]=255;if(rank==9)m->coverage[rank]=(uint8_t)(off+1);}
        if(rank>=4){
            FootballLiveActor *a=&m->actors[def+rank];unsigned target=m->coverage[rank];
            if(m->defense!=FB_DEF_ZONE && target!=255){
                a->x=m->los+dir*(rank==7 || rank==8?2:5);a->y=m->actors[target].y;
            }else if(m->defense==FB_DEF_ZONE){a->x=m->zone_x[rank];a->y=m->zone_y[rank];}
        }
    }
}
void football_live_defense_intent(FootballLive *m,unsigned actor,float *x,float *y)
{
    unsigned off=m->rules.offense==FB_HOME?0:11,def=11-off,rank=actor-def;
    float dir=off?-1:1;FootballLiveActor *a=&m->actors[actor];const FootballLiveActor *carrier=&m->actors[m->carrier];
    bool running=!m->flying && (m->caught || (m->carrier!=off && m->elapsed>=.4f) || (m->carrier==off && (carrier->x-m->los)*dir>.5f));
    bool rushing=rank<4 || (m->defense==FB_DEF_BLITZ && rank==5);
    float tx=a->x,ty=a->y;
    if(running || rushing){
        /* Pursue where the carrier will be, not where he is: lead grows with the distance to close. */
        float lead=clamp(hypotf(carrier->x-a->x,carrier->y-a->y)/(4.3f+a->attributes.spd*.045f),.1f,1.1f);
        tx=carrier->x+carrier->vx*lead;ty=carrier->y+carrier->vy*lead;
    }
    else if(m->defense==FB_DEF_ZONE){
        tx=m->zone_x[rank];ty=m->zone_y[rank];
        unsigned threat=255;float best=100000;
        for(unsigned receiver=1;receiver<=5;++receiver){const FootballLiveActor *r=&m->actors[off+receiver];
            float depth=(r->x-m->los)*dir,lateral=fabsf(r->y-m->zone_y[rank]);
            bool inside=rank>=9 ? depth>=8 && lateral<=16 : depth>=-3 && depth<=16 && lateral<=10;
            if(!inside)continue;
            float score=lateral*2+fabsf(r->x-m->zone_x[rank]);
            if(score<best){best=score;threat=off+receiver;}
        }
        m->coverage[rank]=(uint8_t)threat;
        if(threat!=255){const FootballLiveActor *r=&m->actors[threat];
            tx=r->x+dir*(rank>=9?2:1);ty=clamp(r->y+r->vy*.15f,m->zone_y[rank]-(rank>=9?15:9),m->zone_y[rank]+(rank>=9?15:9));
            if(rank<9)tx=m->los+dir*clamp((tx-m->los)*dir,0,18);
        }
        /* Break on a thrown pass only in this defender's responsibility. */
        if(m->flying && fabsf(m->to_y-m->zone_y[rank])<=(rank>=9?17:11) && ((m->to_x-m->los)*dir<=18 || rank>=9)) {tx=m->to_x;ty=m->to_y;}
    }else if(m->coverage[rank]!=255){
        unsigned mark=m->coverage[rank];const FootballLiveActor *r=&m->actors[mark];
        tx=r->x+dir*.8f+r->vx*.12f;ty=r->y+r->vy*.12f;
        if(m->flying && m->target==mark){tx=m->to_x;ty=m->to_y;}
    }else {
        /* Safety help stays over its half; no arbitrary three-second collapse. */
        tx=m->zone_x[rank];ty=m->zone_y[rank];
        for(unsigned receiver=1;receiver<=5;++receiver){const FootballLiveActor *r=&m->actors[off+receiver];
            bool half=rank==9?r->y<=27:r->y>=26;
            if(half && (r->x-m->los)*dir>8 && (r->x-tx)*dir> -3){tx=r->x+dir*3;ty=r->y;}
        }
        if(m->flying && (rank==9?m->to_y<=29:m->to_y>=24)){tx=m->to_x;ty=m->to_y;}
    }
    *x=tx-a->x;*y=ty-a->y;
}
