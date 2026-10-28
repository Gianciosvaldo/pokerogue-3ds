#include "football_live_systems.h"
#include <math.h>
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
/* The match's one random stream: the live systems share it so a seed replays identically. */
uint32_t football_live_random(FootballLive *m){uint32_t x=m->rng;x^=x<<13;x^=x>>17;x^=x<<5;m->rng=x;return x;}
static unsigned offense(const FootballLive *m){return m->rules.offense==FB_HOME?0:11;}
bool football_live_is_blocker(const FootballLive *m,unsigned actor)
{
    unsigned off=offense(m);
    if(actor<off || actor>=off+11 || actor==m->carrier)return false;
    if(m->special && m->stage!=FB_STAGE_RETURN){
        /* The kicking line holds the rushers off until the foot meets the ball; the gunners are already gone, and a
         * kickoff has no line to hold. */
        if(m->stage!=FB_STAGE_SNAP || m->special==FB_SPECIAL_KICKOFF)return false;
        return actor>=off+6 || (m->special!=FB_SPECIAL_PUNT && actor>=off+2);
    }
    /* Five linemen always block; on a called run the receivers and the tight end do too. */
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    return actor>=off+6 || (p && p->type==FB_PLAY_RUN && actor>=off+2 && actor<=off+5);
}
static bool pass_protection(const FootballLive *m)
{
    unsigned off=offense(m);const FootballPlayDefinition *p=football_playbook_get(m->play);
    return !m->special && p && p->type==FB_PLAY_PASS && !m->caught && m->carrier==off &&
           (m->actors[off].x-m->los)*(off?-1:1)<=.5f;
}
unsigned football_live_block_target(const FootballLive *m,unsigned blocker)
{
    unsigned off=offense(m),def=11-off;
    if(!football_live_is_blocker(m,blocker))return 255;
    const FootballLiveActor *b=&m->actors[blocker];
    if(b->block_peer<22)return b->block_peer;
    unsigned best=255;float score=64;
    bool running=m->special || m->caught || m->carrier!=off || (m->actors[m->carrier].x-m->los)*(off?-1:1)>.5f;
    if(pass_protection(m)){
        unsigned plan[5]={255,255,255,255,255};bool used_blocker[5]={0},used_rusher[11]={0};
        /* Make one shared assignment, rather than five independent nearest-neighbor
         * decisions. Protect an edge with few reachable blockers before doubling a DT. */
        for(unsigned n=0;n<5;++n){
            unsigned rush=255,fewest=6;
            for(unsigned d=def;d<def+11;++d){
                bool user_rush=d==m->controlled && fabsf(m->actors[d].x-m->actors[off].x)<10;
                if(used_rusher[d-def] || m->actors[d].block_peer<22 ||
                   (d>=def+4 && !(m->defense==FB_DEF_BLITZ && d==def+5) && !user_rush))continue;
                unsigned count=0;
                for(unsigned k=0;k<5;++k){const FootballLiveActor *line=&m->actors[off+6+k];
                    float dx=m->actors[d].x-line->x,dy=m->actors[d].y-line->y;
                    if(!used_blocker[k] && line->block_peer>=22 && dx*dx+dy*dy<64)count++;
                }
                if(count && count<fewest){fewest=count;rush=d;}
            }
            if(rush>=22)break;
            unsigned line=255;float nearest=64;
            for(unsigned k=0;k<5;++k){const FootballLiveActor *a=&m->actors[off+6+k];
                float dx=m->actors[rush].x-a->x,dy=m->actors[rush].y-a->y,range=dx*dx+dy*dy;
                if(!used_blocker[k] && a->block_peer>=22 && range<nearest){nearest=range;line=k;}
            }
            if(line>=5)break;
            plan[line]=rush;used_blocker[line]=true;used_rusher[rush-def]=true;
        }
        return plan[blocker-off-6];
    }
    for(unsigned d=def;d<def+11;++d){const FootballLiveActor *a=&m->actors[d];
        bool user_rush=d==m->controlled && fabsf(a->x-m->actors[off].x)<10;
        if(!running && d>=def+4 && !(m->defense==FB_DEF_BLITZ && d==def+5) && !user_rush)continue;
        float dx=a->x-b->x,dy=a->y-b->y,range=dx*dx+dy*dy;
        if(a->block_peer<22)range+=25;
        if(range<score){score=range;best=d;}
    }
    return best;
}
static void release(FootballLive *m,unsigned i)
{
    FootballLiveActor *a=&m->actors[i];unsigned peer=a->block_peer;
    /* A rusher who beat his blocker gets a longer free run before anyone can pick him up again. */
    if(peer<22 && m->actors[peer].block_peer==i){m->actors[peer].block_peer=255;m->actors[peer].block_time=0;m->actors[peer].block_cooldown=.8f+(a->block_edge<0?-a->block_edge*.8f:0);}
    a->block_peer=255;a->block_time=0;a->block_cooldown=.8f;
}
void football_live_physics(FootballLive *m)
{
    unsigned off=offense(m),def=11-off;float dir=off?-1:1;
    bool protecting=pass_protection(m);
    for(unsigned i=0;i<22;++i)if(m->actors[i].block_cooldown>0)m->actors[i].block_cooldown-=FB_LIVE_STEP;
    for(unsigned i=off+2;i<off+11;++i){FootballLiveActor *b=&m->actors[i];if(!football_live_is_blocker(m,i))continue;
        if(b->block_peer<22){
            FootballLiveActor *a=&m->actors[b->block_peer];b->block_time-=FB_LIVE_STEP;
            float dx=a->x-b->x,dy=a->y-b->y;
            if(b->block_time<=0 || dx*dx+dy*dy>9){release(m,i);continue;}
            a->block_time=b->block_time;
            /* Contact permits a strength-dependent push; it never teleports a
             * rusher through a blocker, and controlled rushers use this too. */
            float push=clamp(.35f+(b->attributes.str-a->attributes.str)*.018f,-.5f,1.2f)*FB_LIVE_STEP*dir;
            a->x+=push;b->x+=push;
            /* A defender in a block is slowed to a crawl: that is what opens the running lanes. */
            /* Run blockers drive their man off the ball; pass blockers only shield the QB, and rushers shed them sooner. */
            bool running=m->carrier!=off || m->caught;float hold=clamp((running?.84f:.88f)-b->block_edge*.12f,.5f,.97f);a->vx*=hold;a->vy*=hold;
        }else if(b->block_cooldown<=0){
            unsigned peer=255;float best=1.65f*1.65f;
            unsigned assigned=protecting?football_live_block_target(m,i):255;
            for(unsigned d=def;d<def+11;++d){FootballLiveActor *a=&m->actors[d];float dx=a->x-b->x,dy=a->y-b->y;
                if(protecting && d!=assigned)continue;
                if(a->block_peer<22 || a->block_cooldown>0)continue;
                float dist=dx*dx+dy*dy;if(dist<best){best=dist;peer=d;}
            }
            if(peer<22){FootballLiveActor *a=&m->actors[peer];b->block_peer=(uint8_t)peer;a->block_peer=(uint8_t)i;
                /* Each engagement is a contest: strength tilts it, and the rest is who wins the leverage. */
                float edge=(b->attributes.str-a->attributes.str)*.012f+(((int)(football_live_random(m)%2001)-1000)/1000.0f)*.5f;
                b->block_edge=a->block_edge=edge;
                b->block_time=a->block_time=clamp(1.0f+edge*1.2f+(b->attributes.sta-50)*.003f,.3f,2.4f)*(m->carrier!=off || m->caught?1:.7f);
            }
        }
    }
    /* Fixed-radius bodies and inverse-strength mass. Four bounded solver
     * passes keep contact piles stable at 60 Hz. All 22 actors participate. */
    for(unsigned pass=0;pass<4;++pass)for(unsigned i=0;i<22;++i)for(unsigned j=i+1;j<22;++j){
        FootballLiveActor *a=&m->actors[i],*b=&m->actors[j];float dx=b->x-a->x,dy=b->y-a->y,d2=dx*dx+dy*dy;
        if(d2>=FB_BODY_DIAMETER*FB_BODY_DIAMETER)continue;
        float dist=sqrtf(d2),nx=dist>.0001f?dx/dist:1,ny=dist>.0001f?dy/dist:0;
        float ia=1.0f/(30+a->attributes.str),ib=1.0f/(30+b->attributes.str),total=ia+ib;
        float overlap=FB_BODY_DIAMETER-dist+.0001f;
        a->x-=nx*overlap*ia/total;a->y-=ny*overlap*ia/total;
        b->x+=nx*overlap*ib/total;b->y+=ny*overlap*ib/total;
        float closing=(b->vx-a->vx)*nx+(b->vy-a->vy)*ny;
        if(closing<0){float impulse=-closing/total;a->vx-=nx*impulse*ia;a->vy-=ny*impulse*ia;b->vx+=nx*impulse*ib;b->vy+=ny*impulse*ib;}
    }
    for(unsigned i=0;i<22;++i){m->actors[i].x=clamp(m->actors[i].x,0,120);m->actors[i].y=clamp(m->actors[i].y,-1,54.333f);}
}
void football_live_update_facing(FootballLive *m)
{
    for(unsigned i=0;i<22;++i){FootballLiveActor *a=&m->actors[i];
        if(fabsf(a->vx)>.08f)a->facing=a->vx<0?-1:1;
    }
}
bool football_live_faces_left(const FootballLive *m,unsigned actor)
{
    if(!m || actor>=22)return false;
    unsigned qb=m->snap_offense==FB_HOME?0:11;
    if(actor==qb){unsigned receiver=m->thrown && m->target<22 && m->target!=qb?m->target:qb+2;
        float dx=m->actors[receiver].x-m->actors[qb].x;
        if(fabsf(dx)>.1f)return dx<0;
        return m->snap_offense==FB_AWAY;
    }
    return m->actors[actor].facing<0;
}
