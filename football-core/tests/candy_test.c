#include "football_career.h"
#include "football_live_systems.h"
#include "gc/data.h"
#include <assert.h>
#include "football_profile_fixture.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballCareer career,loaded;
static gc_run scratch,other;
static gc_system profile,resumed_profile;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static FootballRoster roster;
static FootballLive m;
static FootballLiveInput idle(void){return (FootballLiveInput){.receiver=-1};}
static unsigned long total_candy(const gc_system *sys)
{
    unsigned long total=0;
    for(unsigned id=1;id<=GC_MAX_SPECIES_ID;++id)total+=(unsigned long)sys->starters[id].candy_count;
    return total;
}
/* A candy bar in points: the candies of a species, each worth its friendship cap, plus the part of the next one already filled. */
static int bar_of(unsigned id){int cost=gc_starter_cost((uint16_t)id);return getStarterValueFriendshipCap(cost<1?1:cost);}
static unsigned long bar_points(const gc_system *sys,unsigned id){return (unsigned long)sys->starters[id].candy_count*(unsigned long)bar_of(id)+sys->starters[id].friendship;}
static unsigned long total_points(const gc_system *sys)
{
    unsigned long total=0;
    for(unsigned id=1;id<=GC_MAX_SPECIES_ID;++id)if(gc_starter_cost((uint16_t)id)>=0)total+=bar_points(sys,id);
    return total;
}
static unsigned long pending(const FootballCareer *c)
{
    unsigned long total=0;
    for(unsigned i=0;i<22;++i)total+=c->candy_pending[i];
    return total;
}
/* What the human team's tallies are worth, by the species each member started as. */
static unsigned long worth(const FootballCareer *c)
{
    unsigned long total=0;
    for(unsigned i=0;i<22;++i)for(unsigned k=0;k<3;++k)total+=(unsigned long)c->live.stats[FB_HOME][i][k]*football_credit_candy(k+1);
    return total;
}
int main(void)
{
    /* The engine: one credit for the player who makes the play, tallied per member. */
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};
    assert(football_live_begin(&m,&roster,3,1));
    assert(football_credit_candy(FB_CREDIT_TOUCHDOWN)>football_credit_candy(FB_CREDIT_INTERCEPTION) && football_credit_candy(FB_CREDIT_INTERCEPTION)>football_credit_candy(FB_CREDIT_SACK) && football_credit_candy(FB_CREDIT_SACK)>0 && !football_credit_candy(FB_CREDIT_NONE));
    football_live_credit(&m,5,FB_CREDIT_SACK);football_live_credit(&m,16,FB_CREDIT_INTERCEPTION);football_live_credit(&m,5,(FootballCreditKind)9);football_live_credit(&m,22,FB_CREDIT_SACK);
    assert(m.credit_count==2 && m.credits[0].side==FB_HOME && m.credits[0].member==m.actors[5].roster && m.credits[0].kind==FB_CREDIT_SACK && m.credits[1].side==FB_AWAY);
    assert(m.stats[FB_HOME][m.actors[5].roster][FB_CREDIT_SACK-1]==1 && m.stats[FB_AWAY][m.actors[16].roster][FB_CREDIT_INTERCEPTION-1]==1);
    for(unsigned i=0;i<300;++i)football_live_credit(&m,5,FB_CREDIT_SACK);
    assert(m.credit_count==4 && m.stats[FB_HOME][m.actors[5].roster][FB_CREDIT_SACK-1]==255); /* the list and the tally are bounded */

    /* A touchdown is credited to whoever carried it in; a sack to the rusher who made it. */
    assert(football_live_begin(&m,&roster,3,1));m.phase=FB_LIVE_CALL;m.rules.ball=97;m.rules.first_down_at=100;assert(football_live_call(&m,FB_RUN_DIVE));
    unsigned guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});}
    unsigned runner=m.carrier;m.actors[runner].x=110.5f;football_live_tick(&m,16,idle());football_live_tick(&m,16,idle());
    assert(m.phase==FB_LIVE_DEAD && m.event==FB_EVENT_TOUCHDOWN && m.credit_count==1 && m.credits[0].side==FB_HOME && m.credits[0].kind==FB_CREDIT_TOUCHDOWN && m.credits[0].member==m.actors[runner].roster);
    bool sacked=false;
    for(uint32_t seed=1;seed<=40 && !sacked;++seed){
        assert(football_live_begin(&m,&roster,seed,1));m.phase=FB_LIVE_CALL;m.rules.offense=FB_AWAY;m.rules.ball=50;m.rules.first_down_at=60;assert(football_live_call(&m,FB_PASS_SLANTS));
        guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,idle());}
        /* The quarterback holds the ball and the human defender stands on him. */
        m.scramble=true;
        for(unsigned t=0;t<400 && m.phase==FB_LIVE_PLAY;++t){m.actors[4].x=m.actors[m.carrier].x-.6f;m.actors[4].y=m.actors[m.carrier].y;football_live_tick(&m,16,idle());}
        if(m.event==FB_EVENT_SACK){sacked=true;assert(m.credit_count==1 && m.credits[0].side==FB_HOME && m.credits[0].kind==FB_CREDIT_SACK && m.stats[FB_HOME][m.credits[0].member][FB_CREDIT_SACK-1]==1);}
    }
    assert(sacked);
    /* A return touchdown on a kick is credited to the returner too. */
    assert(football_live_begin(&m,&roster,8,1));m.phase=FB_LIVE_CALL;m.rules.offense=FB_HOME;m.rules.ball=30;m.rules.down=4;assert(football_live_special(&m,FB_SPECIAL_PUNT));
    guard=0;while(m.phase!=FB_LIVE_DEAD && !(m.stage==FB_STAGE_RETURN && m.phase==FB_LIVE_PLAY)){assert(++guard<6000);football_live_tick(&m,16,(FootballLiveInput){.pressed=m.phase==FB_LIVE_SET?FB_LIVE_ACTION:0u,.receiver=-1});}
    if(m.phase==FB_LIVE_PLAY){unsigned returner=m.carrier;m.actors[returner].x=9.5f;football_live_tick(&m,16,idle());football_live_tick(&m,16,idle());
        assert(m.event==FB_EVENT_TOUCHDOWN && m.credit_count==1 && m.credits[0].side==FB_AWAY && m.credits[0].member==m.actors[returner].roster && m.credits[0].kind==FB_CREDIT_TOUCHDOWN);}

    /* Whole games: every credit is one per scoring play, sack and interception, and the tallies add up to the credits. */
    unsigned long seen[2][3]={{0}};
    for(uint32_t seed=1;seed<=30;++seed){
        assert(football_live_begin(&m,&roster,seed,1));m.autoplay=1;guard=0;unsigned long credited[2][3]={{0}};
        while(m.phase!=FB_LIVE_END){
            assert(++guard<300000);
            if(m.phase==FB_LIVE_CALL)assert(football_live_defend(&m,FB_DEF_MAN) && m.credit_count==0);
            unsigned before=m.snaps;football_live_tick(&m,16,idle());
            if(m.snaps==before)continue;
            unsigned kinds[4]={0};
            for(unsigned i=0;i<m.credit_count;++i){assert(m.credits[i].side<2 && m.credits[i].member<22 && m.credits[i].kind>=1 && m.credits[i].kind<=3);kinds[m.credits[i].kind]++;credited[m.credits[i].side][m.credits[i].kind-1]++;}
            /* A touchdown pays once; a sack in the end zone is a safety and is still credited, so the event may differ. */
            assert(kinds[FB_CREDIT_TOUCHDOWN]==(m.event==FB_EVENT_TOUCHDOWN?1u:0u));
            assert(kinds[FB_CREDIT_SACK]<=1 && (m.event!=FB_EVENT_SACK || kinds[FB_CREDIT_SACK]==1));
            assert(kinds[FB_CREDIT_INTERCEPTION]<=1 && (m.event!=FB_EVENT_INTERCEPTION || kinds[FB_CREDIT_INTERCEPTION]==1));
        }
        for(unsigned side=0;side<2;++side)for(unsigned k=0;k<3;++k){
            unsigned long tally=0;for(unsigned i=0;i<22;++i)tally+=m.stats[side][i][k];
            assert(tally==credited[side][k]);seen[side][k]+=tally;
        }
    }
    /* seen[side][kind-1]; the fixture is lopsided (the human side wins by far), so these are accounting counts, not a balance claim. */
    assert(seen[0][FB_CREDIT_INTERCEPTION-1]>5 && seen[0][FB_CREDIT_SACK-1]>20 && seen[0][FB_CREDIT_TOUCHDOWN-1]>20 && seen[1][FB_CREDIT_INTERCEPTION-1]>5 && seen[1][FB_CREDIT_TOUCHDOWN-1]>0);
    printf("candy: 30 games, credits (human team / CPU team): %lu/%lu interceptions, %lu/%lu sacks, %lu/%lu touchdowns\n",
        seen[0][FB_CREDIT_INTERCEPTION-1],seen[1][FB_CREDIT_INTERCEPTION-1],seen[0][FB_CREDIT_SACK-1],seen[1][FB_CREDIT_SACK-1],seen[0][FB_CREDIT_TOUCHDOWN-1],seen[1][FB_CREDIT_TOUCHDOWN-1]);

    /* The career earns candy as the plays happen and for the whole team at the end, and the profile gets it only with a
     * checkpoint (football_career_flush_candy, which the screen calls when it saves one). */
    unsigned wins=0,losses=0,unsaved_runs=0,checkpoint_cases=0;unsigned long paid_plays=0,paid_team=0,paid_plays_milli=0,paid_team_milli=0;
    for(uint32_t run=1;run<=8;++run){
        football_test_unlock_all(&profile);
        assert(football_career_new(&career,&scratch,&profile,run));
        for(int i=0;i<gc_starter_cost_count && career.roster.count<22;++i){if(gc_starter_costs[i].cost>1)continue;gc_starter s={.species=gc_starter_costs[i].id,.tera_type=-1};football_roster_add(&career.roster,&scratch,&s,&profile,NULL);}
        assert(career.roster.count==22);football_roster_autofill(&career.roster);career.phase=FB_CAREER_READY;
        assert(football_career_start(&career));
        /* Exercise every play-reward owner at a real checkpoint independently of
         * whether this weak roster scores against the CPU in a particular seed.
         * The full games below still check every naturally occurring credit. */
        for(unsigned kind=FB_CREDIT_INTERCEPTION;kind<=FB_CREDIT_TOUCHDOWN;++kind){
            loaded=career;loaded.scratch=&other;resumed_profile=profile;loaded.profile=&resumed_profile;
            unsigned actor=0;while(actor<22 && !loaded.live.actors[actor].home)++actor;assert(actor<22);
            unsigned long before=total_points(&resumed_profile),points=football_credit_candy(kind);
            football_live_credit(&loaded.live,actor,(FootballCreditKind)kind);loaded.live.snaps++;
            football_career_tick(&loaded,0,idle());
            assert(loaded.candy_plays==points && pending(&loaded)==points && total_points(&resumed_profile)==before);
            football_career_tick(&loaded,0,idle());assert(pending(&loaded)==points); /* no duplicate delivery */
            assert(football_career_save_ready(&loaded) && football_career_save(&loaded,bytes,sizeof bytes));
            assert(total_points(&resumed_profile)==before); /* saving the career alone never pays the profile */
            football_career_flush_candy(&loaded);
            assert(!pending(&loaded) && total_points(&resumed_profile)==before+points*FB_CANDY_FRIENDSHIP_PER_POINT);
            assert(!football_career_flush_candy(&loaded));checkpoint_cases++;
        }
        career.live.autoplay=1;
        unsigned long base=total_candy(&profile),flushed=0,base_points=total_points(&profile),flushed_points=0,member_base[22];guard=0;
        for(unsigned i=0;i<22;++i)member_base[i]=bar_points(&profile,gc_species_root_for_starter(career.roster.mon[i].b.species));
        bool resumed=false,unsaved_checked=false;
        while(career.phase==FB_CAREER_MATCH){
            assert(++guard<300000);
            if(career.live.phase==FB_LIVE_CALL){
                /* Nothing reaches the profile between checkpoints: a game left here pays nothing for the plays it forgets. */
                if(!unsaved_checked && career.candy_plays){
                    assert(total_candy(&profile)-base==flushed && pending(&career)>0);unsaved_checked=true;
                }
                /* The checkpoint: the screen saves the career, then flushes, then saves the profile. */
                if(!resumed && career.live.snaps>20 && football_career_save_ready(&career)){
                    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n);
                    flushed_points+=pending(&career)*FB_CANDY_FRIENDSHIP_PER_POINT;flushed+=football_career_flush_candy(&career);resumed_profile=profile;
                    assert(football_career_load(&loaded,&other,&resumed_profile,bytes,n) && loaded.seen_snaps==loaded.live.snaps && !pending(&loaded));
                    /* A resumed match does not earn what it already earned: the load looks at no dead ball until a new one happens. */
                    unsigned long before=total_candy(&resumed_profile);football_career_tick(&loaded,1,idle());
                    assert(total_candy(&resumed_profile)==before && !loaded.candy_plays && !pending(&loaded));
                    resumed=true;
                }else{flushed_points+=pending(&career)*FB_CANDY_FRIENDSHIP_PER_POINT;flushed+=football_career_flush_candy(&career);}
                assert(football_live_defend(&career.live,FB_DEF_MAN));
            }
            football_career_tick(&career,16,idle());
            assert(total_candy(&profile)-base==flushed && total_points(&profile)-base_points==flushed_points && flushed_points/FB_CANDY_FRIENDSHIP_PER_POINT+pending(&career)==career.candy_plays && career.candy_plays==worth(&career) && !career.candy_team);
        }
        unsaved_runs+=unsaved_checked;assert(resumed && career.phase==FB_CAREER_RESULT && career.candy_plays==worth(&career));
        unsigned long plays_paid=career.candy_plays,plays_milli=career.candy_plays_milli;
        bool won=career.live.rules.winner==FB_HOME;
        assert(football_career_continue(&career));
        if(won){
            wins++;guard=0;
            while(career.phase!=FB_CAREER_SHOP){assert(++guard<20000);if(career.phase==FB_CAREER_EVOLVE)assert(football_career_evolve(&career,0));else football_career_tick(&career,16,idle());}
            assert(career.candy_team==22u*(FB_CANDY_PLAYED+FB_CANDY_WIN) && career.candy_team_milli==football_career_team_candy_milli(&career,true));
        }else{
            losses++;assert(career.phase==FB_CAREER_END && career.candy_team==22u*FB_CANDY_PLAYED && career.candy_team_milli==football_career_team_candy_milli(&career,false));
        }
        /* The shop (or end) checkpoint hands over the rest: the plays not yet flushed and the team's candy, nothing twice. */
        assert(total_candy(&profile)-base==flushed && career.candy_plays==plays_paid+career.candy_team);
        flushed_points+=pending(&career)*FB_CANDY_FRIENDSHIP_PER_POINT;flushed+=football_career_flush_candy(&career);
        assert(total_candy(&profile)-base==flushed && total_points(&profile)-base_points==flushed_points && flushed_points==career.candy_plays*FB_CANDY_FRIENDSHIP_PER_POINT && !pending(&career) && !football_career_flush_candy(&career));
        paid_plays+=plays_paid;paid_team+=career.candy_team;paid_plays_milli+=plays_milli;paid_team_milli+=career.candy_team_milli;
        /* The paid points sit on the species each member started as, and a cost 1 starter's bar (25) is nowhere near the 450 of a cost 8 one. */
        for(unsigned i=0;i<22;++i){uint16_t root=gc_species_root_for_starter(career.roster.mon[i].b.species);assert(root && bar_points(&profile,root)>member_base[i]);}
        /* The whole team's candy is worth the bar of each species: milli-candy is points * 6 * 1000 / bar, summed. */
        assert(career.candy_team_milli>0 && career.candy_team_milli<=football_career_team_candy_milli(&career,true));
    }
    assert(wins+losses==8 && checkpoint_cases==24);
    printf("candy: 24 deterministic unflushed checkpoint cases; %u/8 careers also earned natural play rewards before their final play\n",unsaved_runs);
    /* Nobody to pay: a career without a profile plays on and earns nothing. */
    assert(football_career_new(&career,&scratch,NULL,5));
    for(int i=0;i<gc_starter_cost_count && career.roster.count<22;++i){if(gc_starter_costs[i].cost>1)continue;gc_starter s={.species=gc_starter_costs[i].id,.tera_type=-1};football_roster_add(&career.roster,&scratch,&s,&profile,NULL);}
    football_roster_autofill(&career.roster);career.phase=FB_CAREER_READY;assert(football_career_start(&career));
    career.live.credit_count=1;career.live.credits[0]=(FootballCredit){FB_HOME,0,FB_CREDIT_TOUCHDOWN};career.live.snaps++;
    football_career_tick(&career,1,idle());assert(career.candy_plays==0 && !pending(&career) && !football_career_flush_candy(&career) && !football_career_flush_candy(NULL));
    printf("candy: 8 careers (%u won, %u lost) with cost 1 starters: %.1f points (%.2f candy) per match from the plays, %.1f points (%.2f candy) for the whole team; the profile only changed at checkpoints\n",wins,losses,paid_plays/8.0,paid_plays_milli/8000.0,paid_team/8.0,paid_team_milli/8000.0);

    /* The same points are worth far less to a legendary. Yveltal is a cost 8 starter: its bar is 450, so a touchdown (5 points =
     * 225 of 450) is exactly half a candy and two of them one candy, against 9 candies for a cost 1 starter (bar 25). */
    {
        gc_system pro;football_test_unlock_all(&pro);
        assert(football_career_new(&career,&scratch,&pro,3));
        career.roster.mon[0].b.species=717;
        assert(gc_starter_cost(717)==8 && getStarterValueFriendshipCap(8)==450 && bar_of(717)==450 && FB_CANDY_FRIENDSHIP_PER_POINT==45);
        assert(football_candy_milli(&career,0,FB_CANDY_TOUCHDOWN)==500 && football_candy_milli(&career,0,2*FB_CANDY_TOUCHDOWN)==1000 && football_candy_milli(&career,0,0)==0);
        assert(football_candy_milli(&career,0,FB_CANDY_SACK)==200 && football_candy_milli(&career,0,FB_CANDY_PLAYED+FB_CANDY_WIN)==200);
        unsigned cheap=0;for(unsigned id=1;id<=GC_MAX_SPECIES_ID;++id)if(gc_starter_cost((uint16_t)id)==1){cheap=id;break;}
        assert(cheap);career.roster.mon[1].b.species=(uint16_t)cheap;
        assert(football_candy_milli(&career,1,FB_CANDY_TOUCHDOWN)==9000 && football_candy_milli(&career,1,FB_CANDY_TOUCHDOWN)==football_candy_milli(&career,0,FB_CANDY_TOUCHDOWN)*18);
        assert(!football_candy_milli(NULL,0,5) && !football_candy_milli(&career,22,5));
        /* The profile: three points fill 135 of the 450, no candy yet; the bar carries over to the next match and pays one candy when full. */
        const int32_t had=pro.starters[717].candy_count;
        career.candy_pending[0]=3;
        assert(football_career_flush_candy(&career)==0 && pro.starters[717].candy_count==had && pro.starters[717].friendship==135 && !career.candy_pending[0]);
        career.candy_pending[0]=7;
        assert(football_career_flush_candy(&career)==1 && pro.starters[717].candy_count==had+1 && pro.starters[717].friendship==0);
        /* Two touchdowns at once are exactly one candy. */
        career.candy_pending[0]=2*FB_CANDY_TOUCHDOWN;
        assert(football_career_flush_candy(&career)==1 && pro.starters[717].candy_count==had+2 && pro.starters[717].friendship==0);
        /* Many candies at once on a cheap species (cap 25): 21 points are 945 friendship. */
        career.candy_pending[1]=21;
        const int32_t cheap_had=pro.starters[cheap].candy_count;
        assert(football_career_flush_candy(&career)==37 && pro.starters[cheap].candy_count==cheap_had+37 && pro.starters[cheap].friendship==20);
        /* A bar that is nearly full is not lost to the 16 bits of the field: 2000 points are 90000 friendship. */
        career.candy_pending[0]=2000;
        assert(football_career_flush_candy(&career)==200 && pro.starters[717].candy_count==had+202 && pro.starters[717].friendship==0);
        /* A full candy store keeps the bar one short. */
        pro.starters[717].candy_count=9999;career.candy_pending[0]=100;
        assert(football_career_flush_candy(&career)==0 && pro.starters[717].candy_count==9999 && pro.starters[717].friendship==449);
        puts("candy: a touchdown is 0.5 candy for Yveltal and 9 for a cost 1 starter");
    }
    puts("football species candy tests passed");
    return 0;
}
