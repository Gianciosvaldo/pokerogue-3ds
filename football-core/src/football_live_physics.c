#include "football_live_systems.h"
#include <math.h>
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
/* The match's one random stream: the live systems share it so a seed replays identically. */
uint32_t football_live_random(FootballLive *m){uint32_t x=m->rng;x^=x<<13;x^=x>>17;x^=x<<5;m->rng=x;return x;}
static unsigned offense(const FootballLive *m){return m->rules.offense==FB_HOME?0:11;}
bool football_live_is_blocker(const FootballLive *m,unsigned actor)
{
    unsigned off=offense(m);
    if(actor<off || actor>=off+11 || actor==m->carrier || actor==m->hand_to)return false;
    if(m->special && m->stage!=FB_STAGE_RETURN){
        /* The kicking line holds the rushers off until the foot meets the ball; the gunners are already gone, and a
         * kickoff has no line to hold. */
        if(m->stage!=FB_STAGE_SNAP || m->special==FB_SPECIAL_KICKOFF)return false;
        return actor>=off+6 || (m->special!=FB_SPECIAL_PUNT && actor>=off+2);
    }
    /* Eligible teammates block on runs and after the catch, never while running a pass route. */
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    return actor>=off+6 || (p && (p->type==FB_PLAY_RUN || m->caught) && actor>=off+1 && actor<=off+5);
}
static bool pass_protection(const FootballLive *m)
{
    unsigned off=offense(m);const FootballPlayDefinition *p=football_playbook_get(m->play);
    return !m->special && p && p->type==FB_PLAY_PASS && !m->caught && m->carrier==off &&
           (m->actors[off].x-m->los)*(off?-1:1)<=.5f;
}
bool football_live_run_path(const FootballLive *m,float depth,float lat,float look,float *x,float *y)
{
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    if(!p || p->type!=FB_PLAY_RUN || !p->route_count)return false;
    /* The first point of the called path still ahead of the runner: 2.5 yards further downfield than he is, or not far behind him
     * and still more than 2.5 yards to the side (the sweep's first points run sideways behind the line). Then `look` yards on along
     * the path. Route y is inside-positive around the middle of the field. */
    unsigned k=0;
    while(k+1<p->route_count && !(p->route[k].x>depth+2.5f || (p->route[k].x>depth-1.f && p->route[k].x<=.5f && fabsf(p->route[k].y-lat)>4.f)))++k;
    float cx=p->route[k].x,cy=p->route[k].y,left=look;
    for(++k;k<p->route_count && left>0;++k){
        float dx=p->route[k].x-cx,dy=p->route[k].y-cy,len=hypotf(dx,dy);
        if(len>left){cx+=dx*left/len;cy+=dy*left/len;left=0;break;}
        left-=len;cx=p->route[k].x;cy=p->route[k].y;
    }
    if(left>0)cx+=left; /* past the last point: straight on */
    *x=m->los+(m->rules.offense==FB_HOME?1.f:-1.f)*cx;*y=26.67f-m->run_side*cy;return true;
}
/* Play action: the back's fake lasts FB_LIVE_FAKE after the snap, while the QB still holds the ball behind the line. */
bool football_live_fake_active(const FootballLive *m)
{
    unsigned off=offense(m);
    return !m->special && m->phase==FB_LIVE_PLAY && football_playbook_fake(football_playbook_get(m->play)) && m->elapsed<FB_LIVE_FAKE &&
           !m->flying && !m->thrown && !m->scramble && m->carrier==off;
}
/* Beside the QB on the side the back lined up on, where a handoff would be. */
void football_live_fake_point(const FootballLive *m,float *x,float *y)
{
    unsigned off=offense(m);float dir=off?-1:1;const FootballLiveActor *qb=&m->actors[off];
    bool under=football_playbook_get(m->play)->formation==FB_FORM_UNDER_CENTER;
    float side=m->snap_y[1]>=m->snap_y[0]?1.f:-1.f;
    *x=qb->x+dir*(under?-.3f:.9f);*y=qb->y+side*1.4f;
}
/* HB Screen: after the release time the three linemen nearest the side the back slips out to leave their men and set a wall a few yards
 * downfield of where he catches it. Returns false for every other player and time. */
bool football_live_screen_wall(const FootballLive *m,unsigned rank,float *x,float *y)
{
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    unsigned off=offense(m);
    if(m->special || !p || p->id!=FB_PASS_HB_SCREEN || m->caught || m->carrier!=off || m->elapsed<FB_LIVE_SCREEN_RELEASE || rank<6 || rank>10)return false;
    FootballRouteView path=football_playbook_receiver_path(p,1);
    float lane=m->snap_y[1],end=lane+path.point[path.count-1].y*(lane<26.67f?1.f:-1.f);
    /* Rank the five by how close they lined up to the back's landing spot; the nearest three are the wall. */
    unsigned place=0;
    for(unsigned r=6;r<=10;++r){
        if(r==rank)continue;
        float a=fabsf(m->snap_y[r]-end),b=fabsf(m->snap_y[rank]-end);
        if(a<b || (a==b && r<rank))++place;
    }
    if(place>=3)return false;
    /* Slot the three left to right by where they stood. */
    unsigned slot=0;
    for(unsigned r=6;r<=10;++r){
        if(r==rank)continue;
        unsigned other=0;float b=fabsf(m->snap_y[r]-end);
        for(unsigned q=6;q<=10;++q){if(q==r)continue;float c=fabsf(m->snap_y[q]-end);if(c<b || (c==b && q<r))++other;}
        if(other<3 && m->snap_y[r]<m->snap_y[rank])++slot;
    }
    *x=m->los+(off?-1.f:1.f)*3.f;*y=clamp(end+((float)slot-1)*2.4f,3,50.3f);
    return true;
}
/* Where the ball changes hands: beside the QB and a step toward the line, on the side the play calls for (football_playbook_handoff).
 * Stable during the handoff: it reads the QB and the runner's snap spot, not where he is now. `runner` is the actor who takes the ball
 * (hand_to once the ball is snapped; the screen asks before the snap, when hand_to is still empty). */
void football_live_mesh_point_for(const FootballLive *m,unsigned runner,float *x,float *y)
{
    unsigned off=offense(m);float dir=off?-1:1;
    const FootballLiveActor *qb=&m->actors[off];
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    FootballHandoff h=football_playbook_handoff(p);
    bool sweep=p && p->id==FB_RUN_JET_SWEEP,under=p && p->formation==FB_FORM_UNDER_CENTER;
    /* The strong side is where the called path ends: below or above the middle of the field. */
    float end=p && p->route_count?26.67f-m->run_side*p->route[p->route_count-1].y:26.67f,strong=end>26.67f?1.f:-1.f;
    float lined=runner>=off && runner<off+11?m->snap_y[runner-off]-m->snap_y[0]:0;
    float side=h.side==FB_MESH_STRONG?strong:h.side==FB_MESH_WEAK?-strong:fabsf(lined)>.4f?(lined>0?1.f:-1.f):strong;
    *x=qb->x+dir*(sweep?1.8f:under?-.3f:.9f);*y=qb->y+side*h.across;
}
void football_live_mesh_point(const FootballLive *m,float *x,float *y){football_live_mesh_point_for(m,m->hand_to,x,y);}
/* The called path of the player of this rank, in field coordinates, for the lines shown before the snap. [0] is where he lines up. A pass
 * route is the called points from his own spot (inside toward the middle of the field, the same clamp the receiver's steering applies); a
 * run is the runner's way to the mesh point and then the points of the called path he would aim for, picked by football_live_run_path.
 * It is the plan, not a prediction: coverage and blocking bend the real path. Returns 0 for the QB, the line, kicks and a run's other
 * players. `settles` is true when the player stops at the last point instead of running on upfield. */
unsigned football_live_route_line(const FootballLive *m,unsigned rank,float *x,float *y,unsigned max,bool *settles)
{
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    unsigned off=offense(m),n=0;float dir=off?-1:1;
    if(settles)*settles=false;
    if(m->special || !p || rank<1 || rank>5 || max<2)return 0;
    if(p->type==FB_PLAY_PASS){
        FootballRouteView path=football_playbook_receiver_path(p,rank);
        float lane=m->snap_y[rank],inside=lane<26.67f?1.f:-1.f;
        x[n]=m->snap_x[rank];y[n]=lane;++n;
        for(unsigned k=0;k<path.count && n<max;++k){x[n]=m->snap_x[rank]+dir*path.point[k].x;y[n]=clamp(lane+path.point[k].y*inside,2,51);++n;}
        if(settles)*settles=football_playbook_settles(p);
        return n;
    }
    if(rank!=football_playbook_runner(p) || !p->route_count)return 0;
    float mx,my;football_live_mesh_point_for(m,off+rank,&mx,&my);
    x[n]=m->snap_x[rank];y[n]=m->snap_y[rank];++n;x[n]=mx;y[n]=my;++n;
    float depth=(mx-m->los)*dir,lat=(26.67f-my)*m->run_side,last=p->route[p->route_count-1].x;
    while(n<max && depth<last){
        float px,py;if(!football_live_run_path(m,depth,lat,0,&px,&py) || (fabsf(px-x[n-1])<.05f && fabsf(py-y[n-1])<.05f))break;
        x[n]=px;y[n]=py;++n;depth=(px-m->los)*dir;lat=(26.67f-py)*m->run_side;
    }
    return n;
}
/* The player who escorts the runner (the sweep's back, the wing on power, counter and toss), or 255 when the play has none. */
static unsigned lead_actor(const FootballLive *m)
{
    unsigned off=offense(m);FootballLead lead=m->special?FB_LEAD_NONE:football_playbook_lead(football_playbook_get(m->play));
    return lead==FB_LEAD_RB?off+1:lead==FB_LEAD_WING?off+4:255;
}
unsigned football_live_block_target(const FootballLive *m,unsigned blocker)
{
    unsigned off=offense(m),def=11-off;
    if(!football_live_is_blocker(m,blocker))return 255;
    const FootballLiveActor *b=&m->actors[blocker];
    if(b->block_peer<22)return b->block_peer;
    unsigned best=255;float score=64;
    const FootballPlayDefinition *play=football_playbook_get(m->play);
    bool running=m->special || m->caught || m->carrier!=off || (play && play->type==FB_PLAY_RUN) || (m->actors[m->carrier].x-m->los)*(off?-1:1)>.5f;
    if(pass_protection(m)){
        unsigned plan[5]={255,255,255,255,255};bool used_blocker[5]={0},used_rusher[11]={0};
        for(unsigned k=0;k<5;++k){float wx,wy;if(football_live_screen_wall(m,6+k,&wx,&wy))used_blocker[k]=true;} /* the screen's wall has left */
        /* Make one shared assignment, rather than five independent nearest-neighbor
         * decisions. Protect an edge with few reachable blockers before doubling a DT. */
        for(unsigned n=0;n<5;++n){
            unsigned rush=255,fewest=6;float urgent=1e9f;
            for(unsigned d=def;d<def+11;++d){
                bool user_rush=d==m->controlled && fabsf(m->actors[d].x-m->actors[off].x)<10;
                if(used_rusher[d-def] || m->actors[d].block_peer<22 ||
                   (!football_defense_is_rusher(m->defense,d-def) && !user_rush))continue;
                unsigned count=0;
                for(unsigned k=0;k<5;++k){const FootballLiveActor *line=&m->actors[off+6+k];
                    float dx=m->actors[d].x-line->x,dy=m->actors[d].y-line->y;
                    if(!used_blocker[k] && line->block_peer>=22 && dx*dx+dy*dy<64)count++;
                }
                /* As in the supplied JS passPro, urgent rushers win ties.
                 * Keep scarce edge coverage ahead of urgency, and preserve
                 * existing engagements rather than rematching contact. */
                float qx=m->actors[d].x-m->actors[off].x,qy=m->actors[d].y-m->actors[off].y;
                float danger=qx*qx+qy*qy;
                if(count && (count<fewest || (count==fewest && danger<urgent))){fewest=count;rush=d;urgent=danger;}
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
    if(lead_actor(m)==blocker && !m->caught){
        /* Escort: go for the defender nearest the runner's path a few yards ahead, the one who would otherwise make the tackle. */
        const FootballLiveActor *runner=&m->actors[m->hand_to<22?m->hand_to:m->carrier];
        float px,py;
        if(football_live_run_path(m,(runner->x-m->los)*(off?-1:1),(26.67f-runner->y)*m->run_side,4,&px,&py)){
            unsigned pick=255;float near=100;
            for(unsigned d=def;d<def+11;++d){const FootballLiveActor *a=&m->actors[d];
                float dx=a->x-px,dy=a->y-py,range=dx*dx+dy*dy+(a->block_peer<22?36.f:0);
                if(range<near){near=range;pick=d;}
            }
            if(pick<22)return pick;
        }
    }
    for(unsigned d=def;d<def+11;++d){const FootballLiveActor *a=&m->actors[d];
        bool user_rush=d==m->controlled && fabsf(a->x-m->actors[off].x)<10;
        if(!running && !football_defense_is_rusher(m->defense,d-def) && !user_rush)continue;
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
    for(unsigned i=off+1;i<off+11;++i){FootballLiveActor *b=&m->actors[i];if(!football_live_is_blocker(m,i))continue;
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
