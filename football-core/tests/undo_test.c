#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* The human may take a called play (or kick) back until he snaps it: football_live_undo returns to the playcall with the rules, the random
 * state and so the CPU's defense exactly as they were, and calling the same play again lines up the same picture. It is refused once the ball is
 * snapped, for the CPU's plays, and where there is no playcall (a kickoff). The proof is a soak: whole games played by a script, once straight
 * through and once with every human call taken back and made again, end in the same state. */
static FootballRoster roster;
static FootballLive a,b;
static unsigned checks,undone;
#define CHECK(c) do{++checks;assert(c);}while(0)
static void begin(FootballLive *m,uint32_t seed)
{
    assert(football_live_begin(m,&roster,seed,1));
    m->autoplay=0;m->rules.offense=FB_HOME;m->rules.down=1;m->rules.ball=40;m->rules.first_down_at=50;m->rules.quarter=2;m->rules.seconds_left=90;
    m->phase=FB_LIVE_CALL;
}
static bool same_picture(const FootballLive *x,const FootballLive *y)
{
    return !memcmp(x->actors,y->actors,sizeof x->actors) && x->defense==y->defense && x->play==y->play && x->los==y->los && x->align==y->align && x->flip==y->flip &&
        x->run_side==y->run_side && !memcmp(x->snap_x,y->snap_x,sizeof x->snap_x) && !memcmp(x->snap_y,y->snap_y,sizeof x->snap_y) && x->rng==y->rng && x->controlled==y->controlled;
}
static void one_play(uint32_t seed,FootballPlayId play,FootballPlayId other)
{
    FootballLive before,first;begin(&a,seed);before=a;
    CHECK(!football_live_can_undo(&a) && !football_live_undo(&a)); /* nothing called yet */
    CHECK(football_live_call(&a,play) && a.phase==FB_LIVE_SET && football_live_can_undo(&a));
    first=a;
    CHECK(football_live_undo(&a) && a.phase==FB_LIVE_CALL && !a.special && !football_live_can_undo(&a));
    CHECK(!memcmp(&a.rules,&before.rules,sizeof a.rules) && a.rng==before.rng && a.defense==before.defense && a.play==before.play);
    CHECK(!football_live_undo(&a)); /* once is enough */
    CHECK(football_live_call(&a,play) && same_picture(&a,&first)); /* the same call lines up the same way, against the same defense */
    CHECK(football_live_undo(&a) && football_live_call(&a,other) && a.phase==FB_LIVE_SET && a.defense==first.defense); /* the defense does not depend on the call */
    ++undone;
    /* From the screen: the B press arrives as a tick. */
    CHECK(football_live_undo(&a) && football_live_call(&a,play));
    football_live_tick(&a,17,(FootballLiveInput){.pressed=FB_LIVE_UNDO,.receiver=-1});
    CHECK(a.phase==FB_LIVE_CALL && !football_live_can_undo(&a));
    CHECK(football_live_call(&a,play) && a.phase==FB_LIVE_SET);
    /* The snap ends it for good. */
    football_live_tick(&a,17,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});
    CHECK(a.phase==FB_LIVE_PLAY && !football_live_can_undo(&a) && !football_live_undo(&a));
    football_live_tick(&a,17,(FootballLiveInput){.pressed=FB_LIVE_UNDO,.receiver=-1});CHECK(a.phase==FB_LIVE_PLAY || a.phase==FB_LIVE_DEAD);
}
static void kicks(uint32_t seed)
{
    FootballLive before;
    /* A punt, and a field goal from inside range. */
    begin(&a,seed);before=a;
    CHECK(football_live_special(&a,FB_SPECIAL_PUNT) && a.special==FB_SPECIAL_PUNT && football_live_can_undo(&a));
    CHECK(football_live_undo(&a) && !a.special && a.phase==FB_LIVE_CALL && !memcmp(&a.rules,&before.rules,sizeof a.rules) && a.rng==before.rng);
    a.rules.ball=85;a.rules.first_down_at=95;a.rules.down=4;before=a;
    CHECK(football_live_special(&a,FB_SPECIAL_FIELD_GOAL) && football_live_can_undo(&a));
    CHECK(football_live_undo(&a) && !a.special && !memcmp(&a.rules,&before.rules,sizeof a.rules));
    /* The try: kicking it moves the ball to the 15; taking it back leaves the 4-yard line, and the choice of the try is still open. */
    begin(&a,seed);a.conversion=1;a.rules.ball=96;a.rules.down=1;a.rules.first_down_at=100;before=a;
    CHECK(football_live_special(&a,FB_SPECIAL_EXTRA_POINT) && a.rules.ball==85);
    CHECK(football_live_undo(&a) && a.rules.ball==96 && a.rules.first_down_at==100 && a.conversion==1 && a.phase==FB_LIVE_CALL && a.rng==before.rng);
    CHECK(football_live_go_for_two(&a) && football_live_call(&a,FB_PASS_SLANTS) && football_live_can_undo(&a));
    CHECK(football_live_undo(&a) && a.conversion==2 && a.rules.ball==96 && a.phase==FB_LIVE_CALL); /* the two-point play is still the one being called */
    /* A kickoff has no playcall to go back to: it is set up straight from the dead ball. */
    begin(&a,seed);a.special=FB_SPECIAL_KICKOFF;a.phase=FB_LIVE_SET;a.undo_valid=false;
    CHECK(!football_live_can_undo(&a) && !football_live_undo(&a) && a.phase==FB_LIVE_SET && a.special==FB_SPECIAL_KICKOFF);
}
static void cpu_plays(uint32_t seed)
{
    /* The human defends: the CPU snaps by itself after 1.2 s and nothing can be taken back. */
    begin(&a,seed);a.rules.offense=FB_AWAY;a.rules.ball=40;
    CHECK(football_live_defend(&a,FB_DEF_ZONE) && a.phase==FB_LIVE_SET && !football_live_can_undo(&a) && !football_live_undo(&a));
    football_live_tick(&a,17,(FootballLiveInput){.pressed=FB_LIVE_UNDO,.receiver=-1});CHECK(a.phase==FB_LIVE_SET);
    for(unsigned i=0;i<100 && a.phase==FB_LIVE_SET;++i)football_live_tick(&a,17,(FootballLiveInput){.receiver=-1});
    CHECK(a.phase==FB_LIVE_PLAY);
    /* Both sides CPU (a demo): no undo either. */
    begin(&a,seed);a.autoplay=1;CHECK(football_live_call(&a,FB_RUN_DIVE) && !football_live_can_undo(&a) && !football_live_undo(&a));
}
/* A script for the human: the same answers whatever the random state. */
static void script(FootballLive *m,bool undo_each)
{
    if(m->phase==FB_LIVE_CALL){
        bool human=!football_live_cpu(m,(FootballSide)m->rules.offense);
        if(!human){CHECK(football_live_defend(m,(FootballDefenseId)(m->snaps%FB_DEF_COUNT)));return;}
        bool done=false;
        if(m->conversion==1){done=football_live_special(m,FB_SPECIAL_EXTRA_POINT);}
        else if(!m->conversion && m->rules.down==4)done=football_live_special(m,football_live_special_allowed(m,FB_SPECIAL_FIELD_GOAL)?FB_SPECIAL_FIELD_GOAL:FB_SPECIAL_PUNT);
        if(!done)done=football_live_call(m,(FootballPlayId)((m->snaps*7+m->rules.ball)%FB_PLAY_COUNT));
        CHECK(done);
        if(undo_each){
            FootballLive keep=*m;
            CHECK(football_live_undo(m));
            /* Whatever was called goes round again: same call, same answer. */
            if(keep.special)CHECK(football_live_special(m,(FootballSpecial)keep.special));else CHECK(football_live_call(m,keep.play));
            CHECK(same_picture(m,&keep) && !memcmp(&m->rules,&keep.rules,sizeof m->rules));
            ++undone;
        }
    }
}
static void play_game(FootballLive *m,uint32_t seed,bool undo_each)
{
    assert(football_live_begin(m,&roster,seed,1));m->autoplay=0;
    for(unsigned tick=0;tick<400000 && m->phase!=FB_LIVE_END;++tick){
        script(m,undo_each);
        FootballLiveInput in={.receiver=-1};
        bool human_offense=!football_live_cpu(m,(FootballSide)m->rules.offense);
        if(m->phase==FB_LIVE_SET && human_offense)in.pressed=FB_LIVE_ACTION;
        const FootballPlayDefinition *p=football_playbook_get(m->play);
        if(m->phase==FB_LIVE_PLAY && human_offense && !m->special && p && p->type==FB_PLAY_PASS && !m->thrown && m->elapsed>1.f)in.receiver=(int)(m->snaps%5);
        football_live_tick(m,17,in);
    }
    assert(m->phase==FB_LIVE_END);
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    for(uint32_t seed=1;seed<=8;++seed)for(unsigned play=0;play<FB_PLAY_COUNT;++play)one_play(seed*7717u+1,(FootballPlayId)play,(FootballPlayId)((play+5)%FB_PLAY_COUNT));
    for(uint32_t seed=1;seed<=4;++seed){kicks(seed*7717u+1);cpu_plays(seed*7717u+1);}
    unsigned undone_before=undone,games=0;
    for(uint32_t seed=1;seed<=12;++seed){
        uint32_t s=seed*7717u+1;
        play_game(&a,s,false);play_game(&b,s,true);++games;
        CHECK(!memcmp(&a.rules,&b.rules,sizeof a.rules) && a.rng==b.rng && a.snaps==b.snaps && a.steps==b.steps);
        CHECK(!memcmp(a.stats,b.stats,sizeof a.stats));
    }
    CHECK(undone-undone_before>200);
    printf("undo: %u checks, %u undone calls over %u whole games each played straight and with every call taken back, same endings\n",checks,undone-undone_before,games);
    puts("football undo test passed");
    return 0;
}
