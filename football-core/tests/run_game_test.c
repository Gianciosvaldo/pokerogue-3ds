#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* The run game: the QB hands the ball off, every run starts from an alignment that can differ and be mirrored, the sweep is an edge run
 * with escorts, and none of it makes one run play an outlier. */
static FootballRoster roster;
static FootballLive m;
static const unsigned runs[8]={FB_RUN_INSIDE_ZONE,FB_RUN_DIVE,FB_RUN_DRAW,FB_RUN_TOSS,FB_RUN_POWER,FB_RUN_COUNTER,FB_RUN_OUTSIDE_ZONE,FB_RUN_JET_SWEEP};
static void setup(uint32_t seed,unsigned away,FootballPlayId play,FootballDefenseId defense)
{
    assert(football_live_begin(&m,&roster,seed,1));
    for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
    m.rules.offense=away?FB_AWAY:FB_HOME;m.rules.down=1;m.rules.ball=40;m.rules.first_down_at=50;m.rules.quarter=2;m.rules.seconds_left=900;
    assert(football_live_call(&m,play));
    m.defense=defense;football_live_defense_setup(&m);
}
static void alignments(void)
{
    unsigned seen_align[FB_PLAY_COUNT][FB_ALIGN_COUNT]={{0}},seen_flip[FB_PLAY_COUNT][2]={{0}};
    for(unsigned p=0;p<FB_PLAY_COUNT;++p)for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=120;++seed){
        setup(seed,away,(FootballPlayId)p,FB_DEF_MAN);
        unsigned off=away?11:0;float dir=away?-1:1;
        seen_align[p][m.align]++;seen_flip[p][m.flip]++;
        for(unsigned i=0;i<11;++i){
            const FootballLiveActor *a=&m.actors[off+i];
            /* The snap spots are what the actors stand on, inside the field, behind the line (the OL and TE on it). */
            assert(m.snap_x[i]==a->x && m.snap_y[i]==a->y);
            assert(a->y>=2 && a->y<=51.4f && (a->x-m.los)*dir<=.01f && (a->x-m.los)*dir>=-9);
            for(unsigned j=i+1;j<11;++j)assert(hypotf(a->x-m.actors[off+j].x,a->y-m.actors[off+j].y)>FB_BODY_DIAMETER+.05f);
        }
        assert(m.hand_to==FB_NO_ACTOR);
    }
    for(unsigned p=0;p<FB_PLAY_COUNT;++p){
        assert(seen_flip[p][0]>60 && seen_flip[p][1]>60); /* both sides, whatever the play */
        const FootballPlayDefinition *d=football_playbook_get((FootballPlayId)p);
        unsigned kinds=0;for(unsigned a=0;a<FB_ALIGN_COUNT;++a)kinds+=seen_align[p][a]>0;
        if(d->type==FB_PLAY_RUN && d->id!=FB_RUN_JET_SWEEP && d->id!=FB_RUN_TOSS && d->id!=FB_RUN_POWER && d->id!=FB_RUN_COUNTER)assert(kinds>=2);
    }
    /* The runs with a lead blocker go to the side the tight end lines up on, at every seed and both mirrors. */
    for(unsigned r=0;r<8;++r){
        const FootballPlayDefinition *d=football_playbook_get((FootballPlayId)runs[r]);if(!football_playbook_strong_side(d))continue;
        for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=80;++seed){
            setup(seed,away,(FootballPlayId)runs[r],FB_DEF_MAN);unsigned off=away?11:0;
            float end=26.67f-m.run_side*d->route[d->route_count-1].y;
            assert((end>26.67f)==(m.actors[off+5].y>26.67f));
            /* and the lead blocker is on that side too, or the back for the sweep */
            unsigned lead=football_playbook_lead(d)==FB_LEAD_RB?off+1:off+4;
            assert((m.actors[lead].y>26.67f)==(end>26.67f));
        }
    }
    /* The sweep's carrier lines up behind the line, off the weak side of the QB. */
    setup(3,0,FB_RUN_JET_SWEEP,FB_DEF_MAN);assert(m.align==FB_ALIGN_JET && m.actors[4].x<=m.los-3 && m.actors[4].x>=m.los-5);
}
static void handoffs(void)
{
    /* At the snap the ball is the QB's and the runner is named, wherever he lines up (a sweep's receiver is 4 yards away, the back 2 to 4). */
    for(unsigned r=0;r<8;++r)for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=30;++seed){
        setup(seed,away,(FootballPlayId)runs[r],(FootballDefenseId)(seed%FB_DEF_COUNT));
        unsigned off=away?11:0,runner=off+football_playbook_runner(football_playbook_get((FootballPlayId)runs[r]));
        m.autoplay=1;m.elapsed=1.2f; /* the CPU snaps on the next step */
        float ball_x=m.ball_x,ball_y=m.ball_y;
        football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
        assert(m.phase==FB_LIVE_PLAY && m.carrier==off && m.hand_to==runner && !football_live_is_blocker(&m,runner));
        assert(hypotf(m.ball_x-ball_x,m.ball_y-ball_y)<.01f && hypotf(m.ball_x-m.actors[off].x,m.ball_y-m.actors[off].y)<.01f);
    }
    /* Every run play completes its handoff, the ball never leaves the QB's hands until then, and the transfer is a short one. */
    unsigned slow=0,total=0;double draw_time=0,dive_time=0;unsigned draws=0,dives=0;
    for(unsigned r=0;r<8;++r)for(unsigned away=0;away<2;++away)for(uint32_t seed=1;seed<=40;++seed){
        setup(seed,away,(FootballPlayId)runs[r],(FootballDefenseId)(seed%FB_DEF_COUNT));
        unsigned off=away?11:0,runner=off+football_playbook_runner(football_playbook_get((FootballPlayId)runs[r]));
        m.autoplay=1;m.elapsed=1.3f;
        unsigned guard=0;float done=-1;
        while(m.phase!=FB_LIVE_DEAD && m.phase!=FB_LIVE_END && ++guard<2000){
            football_live_tick(&m,17,(FootballLiveInput){.pressed=m.phase==FB_LIVE_SET?FB_LIVE_ACTION:0u,.receiver=-1});
            if(m.phase!=FB_LIVE_PLAY)continue;
            if(m.hand_to<22){assert(m.carrier==off);assert(hypotf(m.ball_x-m.actors[off].x,m.ball_y-m.actors[off].y)<.01f);}
            else if(done<0){
                done=m.elapsed;assert(m.carrier==runner);
                /* the ball moves from the QB to the runner across a mesh point beside the QB, not across the field */
                assert(hypotf(m.actors[runner].x-m.actors[off].x,m.actors[runner].y-m.actors[off].y)<4.5f || m.elapsed>=FB_LIVE_HANDOFF_MAX);
            }
        }
        if(done<0)continue;                         /* the QB was brought down with the ball first: a loss on a run */
        total++;slow+=done>=FB_LIVE_HANDOFF_MAX;assert(done<=FB_LIVE_HANDOFF_MAX+.05f);
        if(runs[r]==FB_RUN_DRAW){draw_time+=done;draws++;}
        if(runs[r]==FB_RUN_DIVE){dive_time+=done;dives++;}
    }
    assert(total>500 && slow*10<total); /* handoffs are completed by the runner, not forced by the time limit */
    /* The draw shows pass first: its handoff comes later than the dive's. */
    assert(draws && dives && draw_time/draws>dive_time/dives+.1);

    /* The human gets the runner only after the handoff, and nothing he presses moves the QB or the ball before it. */
    for(unsigned r=0;r<8;++r)for(uint32_t seed=1;seed<=10;++seed){
        setup(seed,0,(FootballPlayId)runs[r],FB_DEF_MAN);
        assert(m.controlled==0);
        football_live_tick(&m,17,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});assert(m.phase==FB_LIVE_PLAY);
        float qb_x=m.actors[0].x,qb_y=m.actors[0].y;
        football_live_tick(&m,17,(FootballLiveInput){.x=-1,.y=1,.pressed=FB_LIVE_JUKE,.receiver=-1});
        if(m.hand_to<22){assert(m.controlled==0 && m.actors[0].x>=qb_x-.2f && fabsf(m.actors[0].y-qb_y)<.2f && m.cooldown<=0);}
        for(unsigned i=0;i<150 && m.hand_to<22;++i)football_live_tick(&m,17,(FootballLiveInput){.x=-1,.y=1,.receiver=-1});
        assert(m.hand_to==FB_NO_ACTOR && m.carrier==m.controlled && m.carrier!=0);
    }
}
typedef struct { double yards;unsigned n,neg,big; } Row;
static void balance(void)
{
    Row row[8]={{0}};
    for(unsigned r=0;r<8;++r)for(unsigned d=0;d<FB_DEF_COUNT;++d)for(uint32_t seed=1;seed<=40;++seed){
        setup(seed*7+d,seed&1,(FootballPlayId)runs[r],(FootballDefenseId)d);m.autoplay=1;
        unsigned before=m.snaps,guard=0;
        while(m.snaps==before && ++guard<4000)football_live_tick(&m,16,(FootballLiveInput){.receiver=-1});
        assert(m.snaps>before);
        row[r].yards+=m.result.yards;row[r].n++;row[r].neg+=m.result.yards<0;row[r].big+=m.result.yards>=10;
    }
    double all=0;unsigned all_n=0;
    for(unsigned r=0;r<8;++r){all+=row[r].yards;all_n+=row[r].n;}
    for(unsigned r=0;r<8;++r){
        double avg=row[r].yards/row[r].n;
        /* No run is a free yard-gainer or a certain loss: the sweep used to average three times the rest and never lose yardage. */
        assert(avg>-.5 && avg<2.8);
        assert(row[r].neg*100>row[r].n*5 && row[r].neg*100<row[r].n*60);
        assert(row[r].big*100<row[r].n*8);
    }
    double sweep=row[7].yards/row[7].n,others=(all-row[7].yards)/(all_n-row[7].n);
    assert(sweep<others+1.2);
    printf("run game: %u carries, mean %.2f yd, jet sweep %.2f yd, %.0f%% of sweeps lose yards\n",all_n,all/all_n,sweep,100.*row[7].neg/row[7].n);
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    alignments();handoffs();balance();
    printf("football run game: alignments, strong-side runs, handoff, human control, draw delay and run balance passed\n");
}
