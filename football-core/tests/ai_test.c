#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static FootballLive m;
typedef struct { unsigned run,pass,play[FB_PLAY_COUNT],repeat,calls; } Mix;
typedef struct { unsigned down,to_go,ball;int lead;unsigned quarter,seconds; } Situation;
static void set_situation(Situation s,uint32_t seed)
{
    assert(football_live_begin(&m,&roster,seed,1));
    m.rules.offense=FB_AWAY;m.rules.down=(uint8_t)s.down;m.rules.ball=(uint8_t)s.ball;
    m.rules.first_down_at=(uint8_t)(s.ball+s.to_go);m.rules.quarter=(uint8_t)s.quarter;m.rules.seconds_left=(uint16_t)s.seconds;
    m.rules.score[FB_AWAY]=s.lead>0?(uint16_t)s.lead:0;m.rules.score[FB_HOME]=s.lead<0?(uint16_t)-s.lead:0;
    assert(football_rules_distance(&m.rules)==s.to_go);
}
static Mix sample_calls(Situation s)
{
    Mix mix={0};set_situation(s,1234);int last=-1;
    for(unsigned n=0;n<4000;++n){
        FootballPlayId id=football_live_ai_call(&m);const FootballPlayDefinition *p=football_playbook_get(id);
        assert(p);mix.play[id]++;if(p->type==FB_PLAY_RUN)mix.run++;else mix.pass++;
        if(last==(int)id)mix.repeat++;
        last=(int)id;mix.calls++;
    }
    return mix;
}
static float share(unsigned part,unsigned total){return total?(float)part/total:0;}
typedef struct { unsigned snaps,runs,completions,incompletions,interceptions,sacks,fumbles,plays[FB_PLAY_COUNT];unsigned games,overtime,run_left,run_right,run_middle;double points;uint32_t signature; } Stats;
static Stats play_games(unsigned first_seed,unsigned games)
{
    Stats st={0};
    for(unsigned g=0;g<games;++g){
        assert(football_live_begin(&m,&roster,first_seed+g,1));
        /* Mirror match: both sides carry the same attributes, so only the CPU's decisions differ. */
        for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
        m.autoplay=1;unsigned guard=0;
        while(m.phase!=FB_LIVE_END){
            assert(++guard<200000);
            if(m.phase==FB_LIVE_CALL)assert(football_live_defend(&m,FB_DEF_MAN));
            unsigned before=m.snaps;FootballPlayId play=m.play;
            football_live_tick(&m,16,(FootballLiveInput){.receiver=-1});
            if(m.snaps==before)continue;
            if(m.special)continue; /* kicks are counted in special_test */
            st.snaps++;st.plays[play]++;
            switch(m.result.kind){
            case FB_RESULT_RUN:st.runs++;
                if(m.ball_y<26.67f-4)st.run_left++;else if(m.ball_y>26.67f+4)st.run_right++;else st.run_middle++;break;
            case FB_RESULT_COMPLETE:st.completions++;break;
            case FB_RESULT_INCOMPLETE:st.incompletions++;break;
            case FB_RESULT_INTERCEPTION:st.interceptions++;break;
            case FB_RESULT_SACK:st.sacks++;break;
            case FB_RESULT_FUMBLE:case FB_RESULT_FUMBLE_RECOVERED:st.fumbles++;break;
            }
            st.signature=st.signature*31u+(uint32_t)(m.result.kind*7+m.result.yards+100)+m.rules.ball;
        }
        st.games++;st.points+=m.rules.score[0]+m.rules.score[1];if(m.rules.quarter>4)st.overtime++;
        st.signature=st.signature*131u+m.rules.score[0]*17u+m.rules.score[1];
    }
    return st;
}
/* Receiver windows with equal value must not collapse to the first capped
 * candidate. These are isolated QB-read contexts, not overlapping live routes. */
static void receiver_reads(void)
{
    const unsigned slot[5]={2,3,4,5,1};
    const unsigned accuracy[3]={30,60,99};
    for(unsigned side=0;side<2;++side)for(unsigned profile=0;profile<3;++profile){
        set_situation((Situation){1,10,50,0,2,90},1);
        m.rules.offense=(FootballSide)side;m.play=FB_PASS_SLANTS;m.elapsed=2;
        unsigned off=side?11:0,def=11-off;float dir=side?-1.f:1.f;
        m.carrier=(uint8_t)off;m.thrown=m.flying=m.scramble=false;m.los=60;
        for(unsigned i=0;i<22;++i){m.actors[i].x=60+dir*50;m.actors[i].y=26.67f;m.actors[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};}
        m.actors[off].x=60-dir*5;m.actors[off].attributes.tha=(uint8_t)accuracy[profile];
        for(unsigned r=0;r<5;++r){m.actors[off+slot[r]].x=60+dir*5;m.actors[off+slot[r]].y=26.67f;}
        unsigned count[5]={0};
        for(unsigned seed=1;seed<=4000;++seed){m.rng=seed;int read=football_live_ai_qb(&m);assert(read>=0&&read<5);count[read]++;}
        for(unsigned r=0;r<5;++r)assert(count[r]>400&&count[r]<1200);
        /* A single open receiver still wins over four covered ones, at every
         * position; fairness between ties cannot override the actual window. */
        for(unsigned open=0;open<5;++open){
            const float lane[5]={4,49,13,39,26.67f};
            for(unsigned r=0;r<5;++r){m.actors[off+slot[r]].y=lane[r];m.actors[def+r].x=r==open?60+dir*50:60+dir*5.2f;m.actors[def+r].y=lane[r];}
            for(unsigned seed=1;seed<=100;++seed){m.rng=seed;assert(football_live_ai_qb(&m)==(int)open);}
        }
    }
}
static void outside_zone_responsibilities(void)
{
    for(uint32_t seed=1;seed<=8;++seed)for(unsigned side=0;side<2;++side)for(unsigned play=0;play<FB_PLAY_COUNT;++play){
        if(football_playbook_get((FootballPlayId)play)->type!=FB_PLAY_PASS)continue;
        set_situation((Situation){1,10,50,0,2,90},seed);m.rules.offense=(FootballSide)side;m.autoplay=1;
        assert(football_live_call(&m,(FootballPlayId)play));m.defense=FB_DEF_ZONE;football_live_defense_setup(&m);
        unsigned off=side?11:0,def=11-off;
        for(unsigned corner=7;corner<=8;++corner){float x,y;football_live_defense_intent(&m,def+corner,&x,&y);
            /* The corner on the low sideline takes the outside receiver who lined up there, whichever way the formation was mirrored. */
            bool low=corner==7;unsigned receiver=off+((m.actors[off+2].y<m.actors[off+3].y)==low?2:3);assert(m.coverage[corner]==receiver);
            float dx=m.actors[def+corner].x+x-m.actors[receiver].x,dy=m.actors[def+corner].y+y-m.actors[receiver].y;
            assert(hypotf(dx,dy)>FB_BODY_DIAMETER+.1f);
        }
    }
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};

    receiver_reads();outside_zone_responsibilities();

    /* First and ten in a tied game at midfield: every play is in the book, and it is not a coin flip between two of them. */
    Mix neutral=sample_calls((Situation){1,10,50,0,2,90});
    unsigned legacy_calls=0;for(unsigned id=0;id<8;++id)legacy_calls+=neutral.play[id];
    /* Preserve coverage within the original eight and cover every new concept.
     * Twenty absolute shares above five percent would be mathematically impossible. */
    for(unsigned id=0;id<8;++id)assert(share(neutral.play[id],legacy_calls)>.05f);
    for(unsigned id=0;id<FB_PLAY_COUNT;++id)assert(share(neutral.play[id],neutral.calls)>.015f);
    assert(share(neutral.run,neutral.calls)>.35f && share(neutral.run,neutral.calls)<.6f);
    /* Calling the play it just ran is rare (a uniform pick would repeat one time in eight). */
    assert(share(neutral.repeat,neutral.calls)<.06f);

    /* Situations: long yardage throws, short yardage pounds it, the goal line has no room for a long route. */
    Mix longer=sample_calls((Situation){3,12,30,0,2,90});assert(share(longer.pass,longer.calls)>.7f);
    Mix shorter=sample_calls((Situation){3,1,50,0,2,90});assert(share(shorter.run,shorter.calls)>.65f);
    Mix goal=sample_calls((Situation){2,2,98,0,2,90});
    assert(share(goal.play[FB_PASS_VERTICALS]+goal.play[FB_PASS_POST],goal.calls)<.04f);
    Mix behind=sample_calls((Situation){1,10,50,-10,4,40});assert(share(behind.pass,behind.calls)>.8f);
    Mix ahead=sample_calls((Situation){1,10,50,10,4,40});assert(share(ahead.run,ahead.calls)>.65f);

    /* The defense changes its look with the down: an obvious passing down brings more pressure. */
    unsigned blitz_first=0,blitz_third=0,scheme[FB_DEF_COUNT]={0};
    set_situation((Situation){1,10,50,0,2,90},99);
    for(unsigned n=0;n<4000;++n){FootballDefenseId d=football_live_ai_defense(&m);assert((unsigned)d<FB_DEF_COUNT);scheme[d]++;if(d==FB_DEF_BLITZ)blitz_first++;}
    for(unsigned id=0;id<FB_DEF_COUNT;++id)assert(share(scheme[id],4000)>.08f);
    set_situation((Situation){3,9,50,0,2,90},99);
    for(unsigned n=0;n<4000;++n)if(football_live_ai_defense(&m)==FB_DEF_BLITZ)blitz_third++;
    assert(blitz_third>blitz_first*3/2);

    /* The QB gives the routes time to develop, and does not stand in the pocket forever. */
    set_situation((Situation){1,10,50,0,2,90},5);
    m.phase=FB_LIVE_PLAY;m.carrier=11;m.thrown=m.flying=m.caught=false;m.elapsed=.2f;
    for(unsigned i=0;i<11;++i){m.actors[i].x=m.los-20+i;m.actors[i].y=2+i*4;}
    assert(football_live_ai_qb(&m)==FB_AI_HOLD);
    m.elapsed=3.7f;assert(football_live_ai_qb(&m)!=FB_AI_HOLD);

    /* Mirror matches played entirely by the CPU: they finish, and the numbers look like football. */
    /* 300 games for the bands. The sack share is about 2.25% of snaps against a 2% floor (600 games on the code before and after the passing
     * rework: 2.28% and 2.24%), so 40 games (45 sacks, give or take 7) made the floor a coin flip that any change to the passing game re-rolled.
     * The bounds are the same; the sample is big enough to mean something. */
    const unsigned games=300;
    Stats st=play_games(1,games);
    float snaps=(float)st.snaps;
    assert(st.games==games && st.snaps>games*35u);
    float dropbacks=(float)(st.completions+st.incompletions+st.interceptions+st.sacks);
    assert(dropbacks/snaps>.4f && dropbacks/snaps<.8f);
    assert((float)st.sacks/snaps>.02f && (float)st.sacks/snaps<.12f);
    assert((float)st.interceptions/snaps>.005f && (float)st.interceptions/snaps<.05f);
    float attempts=(float)(st.completions+st.incompletions+st.interceptions);
    assert((float)st.completions/attempts>.4f && (float)st.completions/attempts<.75f);
    unsigned legacy_snaps=0;for(unsigned id=0;id<8;++id)legacy_snaps+=st.plays[id];
    for(unsigned id=0;id<8;++id)assert((float)st.plays[id]/legacy_snaps>.04f);
    for(unsigned id=0;id<FB_PLAY_COUNT;++id)assert(st.plays[id]>0);
    assert(st.points/st.games>10 && st.points/st.games<70);
    /* Runs are not all funneled into the same hole: a fifth of them end more than four yards off the middle, on both sides. */
    float outside=(float)(st.run_left+st.run_right),runs=(float)(st.runs);
    assert(runs>200 && outside/runs>.2f && st.run_left>st.runs/10 && st.run_right>st.runs/10);
    printf("ai: %u games, %u snaps, %.1f pts/game, runs %.0f%% (outside %.0f%%), comp %.0f%%, sack %.1f%%, int %.1f%%, OT %u\n",
        st.games,st.snaps,st.points/st.games,100*runs/snaps,100*outside/runs,100*(float)st.completions/attempts,100*(float)st.sacks/snaps,100*(float)st.interceptions/snaps,st.overtime);

    /* Same seeds, same games. */
    Stats first=play_games(1,40),again=play_games(1,40);
    assert(again.signature==first.signature && again.snaps==first.snaps);
    Stats other=play_games(1001,10);assert(other.signature!=st.signature);
    puts("football ai tests passed");
    return 0;
}
