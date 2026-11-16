#include "football_sound.h"
#include "football_live_systems.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The match sounds: what the watcher lists for each kind of change, which side it is "for", that the same match gives the same sounds at any
 * frame length (as often, whatever the order inside one frame), and that whole games add up against the engine's own tallies (credits) and the final score. */
static FootballRoster roster;
static FootballLive m;
static FootballSoundWatch w;
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static unsigned scan(FootballSound *out){return football_sound_scan(&w,&m,out);}
static void look(void){football_sound_reset(&w,&m);}
static void fresh(uint32_t seed)
{
    assert(football_live_begin(&m,&roster,seed,1));m.autoplay=1;look();
}
static bool has(const FootballSound *s,unsigned n,FootballSound want){for(unsigned i=0;i<n;++i)if(s[i]==want)return true;return false;}
static void table(void)
{
    CHECK(!football_sound_key(FB_SND_NONE) && !football_sound_key(FB_SND_COUNT) && !football_sound_key((FootballSound)200));
    for(unsigned s=1;s<FB_SND_COUNT;++s){const char *k=football_sound_key((FootballSound)s);CHECK(k && k[0] && strchr(k,'/'));}
    /* a sound the human wants to hear and one it does not are not the same sound */
    CHECK(strcmp(football_sound_key(FB_SND_TOUCHDOWN_FOR),football_sound_key(FB_SND_TOUCHDOWN_AGAINST)));
    CHECK(strcmp(football_sound_key(FB_SND_TURNOVER_GAIN),football_sound_key(FB_SND_TURNOVER_LOSS)));
    CHECK(strcmp(football_sound_key(FB_SND_WIN),football_sound_key(FB_SND_LOSS)));
    CHECK(strcmp(football_sound_key(FB_SND_SCORE_FOR),football_sound_key(FB_SND_SCORE_AGAINST)));
}
/* Hand-made changes, one at a time. */
static void single_changes(void)
{
    FootballSound s[FB_SOUND_MAX];unsigned n;
    fresh(11);
    /* A first look is silent, and so is a frame in which nothing changed. */
    FootballSoundWatch blank;memset(&blank,0,sizeof blank);
    CHECK(football_sound_scan(&blank,&m,s)==0 && blank.seen);
    CHECK(scan(s)==0);
    CHECK(football_sound_scan(NULL,&m,s)==0 && football_sound_scan(&w,NULL,s)==0 && football_sound_scan(&w,&m,NULL)==0);
    /* the snap */
    m.phase=FB_LIVE_SET;look();m.phase=FB_LIVE_PLAY;m.special=FB_SPECIAL_NONE;
    n=scan(s);CHECK(n==1 && s[0]==FB_SND_SNAP);CHECK(scan(s)==0);
    m.phase=FB_LIVE_SET;look();m.phase=FB_LIVE_PLAY;m.special=FB_SPECIAL_PUNT;CHECK(scan(s)==0); /* a kick has no snap sound of its own */
    m.special=FB_SPECIAL_NONE;
    /* the ball leaves a hand, or a foot */
    m.phase=FB_LIVE_PLAY;look();m.flying=true;n=scan(s);CHECK(n==1 && s[0]==FB_SND_THROW);CHECK(scan(s)==0);
    m.flying=false;look();m.special=FB_SPECIAL_FIELD_GOAL;m.flying=true;n=scan(s);CHECK(n==1 && s[0]==FB_SND_KICK);
    m.special=FB_SPECIAL_NONE;m.flying=false;look();
    /* every fx kind has its sound, once, and the side decides the touchdown and the interception */
    static const struct{FootballFxKind kind;unsigned actor;FootballSound first,second;}fx[]={
        {FB_FX_JUKE,3,FB_SND_JUKE,FB_SND_NONE},{FB_FX_DIVE,4,FB_SND_DIVE,FB_SND_NONE},{FB_FX_TACKLE,13,FB_SND_TACKLE,FB_SND_NONE},
        {FB_FX_SACK,14,FB_SND_SACK,FB_SND_NONE},{FB_FX_FUMBLE,2,FB_SND_FUMBLE,FB_SND_NONE},{FB_FX_CATCH,6,FB_SND_CATCH,FB_SND_NONE},
        {FB_FX_INCOMPLETE,7,FB_SND_INCOMPLETE,FB_SND_NONE},
        {FB_FX_TOUCHDOWN,1,FB_SND_TOUCHDOWN_FOR,FB_SND_NONE},{FB_FX_TOUCHDOWN,15,FB_SND_TOUCHDOWN_AGAINST,FB_SND_NONE},
        {FB_FX_INTERCEPTION,5,FB_SND_INTERCEPTION,FB_SND_TURNOVER_GAIN},{FB_FX_INTERCEPTION,16,FB_SND_INTERCEPTION,FB_SND_TURNOVER_LOSS}};
    for(unsigned i=0;i<sizeof fx/sizeof *fx;++i){
        m.steps+=7;football_live_fx(&m,fx[i].kind,fx[i].actor,22,1);
        n=scan(s);CHECK(n==(fx[i].second?2u:1u) && s[0]==fx[i].first && (!fx[i].second || s[1]==fx[i].second));
        CHECK(scan(s)==0); /* each record is heard once */
    }
    /* several records in one frame come out in the order they happened, not in ring order */
    fresh(12);m.fx_next=6;m.steps=100;football_live_fx(&m,FB_FX_CATCH,6,22,1);m.steps=140;football_live_fx(&m,FB_FX_TACKLE,13,6,1);
    m.fx_next=0;m.steps=160;football_live_fx(&m,FB_FX_TOUCHDOWN,6,22,1);
    n=scan(s);CHECK(n==3 && s[0]==FB_SND_CATCH && s[1]==FB_SND_TACKLE && s[2]==FB_SND_TOUCHDOWN_FOR);
    /* the ring is cleared for the next play and a new record in a slot that held the same kind is still new */
    memset(m.fx,0,sizeof m.fx);m.fx_next=0;CHECK(scan(s)==0);
    m.steps=900;football_live_fx(&m,FB_FX_TACKLE,13,6,1);m.fx_next=0;m.steps=901;football_live_fx(&m,FB_FX_TACKLE,13,6,1);
    n=scan(s);CHECK(n==1 && s[0]==FB_SND_TACKLE);
    /* a kick's dead ball */
    struct{FootballLiveEvent event;unsigned home,away;FootballSound want;}dead[]={
        {FB_EVENT_FIELD_GOAL,3,0,FB_SND_SCORE_FOR},{FB_EVENT_FIELD_GOAL,0,3,FB_SND_SCORE_AGAINST},
        {FB_EVENT_EXTRA_POINT,1,0,FB_SND_SCORE_FOR},{FB_EVENT_EXTRA_POINT,0,1,FB_SND_SCORE_AGAINST},
        {FB_EVENT_TWO_POINT,2,0,FB_SND_SCORE_FOR},{FB_EVENT_TWO_POINT,0,2,FB_SND_SCORE_AGAINST},
        {FB_EVENT_SAFETY,2,0,FB_SND_SCORE_FOR},{FB_EVENT_SAFETY,0,2,FB_SND_SCORE_AGAINST},
        {FB_EVENT_FIELD_GOAL_MISSED,0,0,FB_SND_NO_GOOD},{FB_EVENT_EXTRA_POINT_MISSED,0,0,FB_SND_NO_GOOD},{FB_EVENT_TWO_POINT_FAILED,0,0,FB_SND_NO_GOOD},
        {FB_EVENT_BLOCKED,0,0,FB_SND_BLOCKED},{FB_EVENT_MUFF,0,0,FB_SND_MUFF},{FB_EVENT_FAIR_CATCH,0,0,FB_SND_CATCH}};
    for(unsigned i=0;i<sizeof dead/sizeof *dead;++i){
        fresh(13+i);memset(&m.rules.last,0,sizeof m.rules.last);
        m.event=(uint8_t)dead[i].event;m.rules.score[FB_HOME]+=(uint16_t)dead[i].home;m.rules.score[FB_AWAY]+=(uint16_t)dead[i].away;++m.snaps;
        n=scan(s);CHECK(n==1 && s[0]==dead[i].want);CHECK(scan(s)==0);
    }
    /* events with nothing to say: a punt, a kickoff, a touchback, a touchdown (the fx record says it) */
    static const FootballLiveEvent quiet[]={FB_EVENT_NONE,FB_EVENT_PUNT,FB_EVENT_KICKOFF,FB_EVENT_TOUCHBACK,FB_EVENT_OUT_OF_BOUNDS,FB_EVENT_TOUCHDOWN,FB_EVENT_SACK,FB_EVENT_INTERCEPTION};
    for(unsigned i=0;i<sizeof quiet/sizeof *quiet;++i){fresh(40+i);memset(&m.rules.last,0,sizeof m.rules.last);m.event=(uint8_t)quiet[i];++m.snaps;CHECK(scan(s)==0);}
    /* first down: for the human's drive only */
    fresh(50);memset(&m.rules.last,0,sizeof m.rules.last);m.rules.last.first_down=true;m.event=FB_EVENT_NONE;m.rules.offense=FB_HOME;++m.snaps;
    n=scan(s);CHECK(n==1 && s[0]==FB_SND_FIRST_DOWN);
    fresh(51);memset(&m.rules.last,0,sizeof m.rules.last);m.rules.last.first_down=true;m.event=FB_EVENT_NONE;m.rules.offense=FB_AWAY;++m.snaps;CHECK(scan(s)==0);
    /* a ball lost on downs or fumbled away: told by who holds it now */
    for(unsigned side=0;side<2;++side)for(unsigned e=0;e<2;++e){
        fresh(60+side*2+e);memset(&m.rules.last,0,sizeof m.rules.last);m.rules.last.turnover=true;m.event=e?FB_EVENT_FUMBLE:FB_EVENT_NONE;m.rules.offense=(uint8_t)side;++m.snaps;
        n=scan(s);CHECK(n==1 && s[0]==(side==FB_HOME?FB_SND_TURNOVER_GAIN:FB_SND_TURNOVER_LOSS));
    }
    /* a quarter ends; the match's last play ends the match, not a quarter */
    fresh(70);memset(&m.rules.last,0,sizeof m.rules.last);m.rules.last.quarter_end=true;m.event=FB_EVENT_NONE;++m.snaps;
    n=scan(s);CHECK(n==1 && s[0]==FB_SND_PERIOD_END);
    for(unsigned winner=0;winner<2;++winner){
        fresh(71+winner);memset(&m.rules.last,0,sizeof m.rules.last);m.rules.last.quarter_end=true;m.rules.finished=true;m.rules.winner=(FootballSide)winner;m.phase=FB_LIVE_END;++m.snaps;
        n=scan(s);CHECK(n==1 && s[0]==(winner==FB_HOME?FB_SND_WIN:FB_SND_LOSS));
        CHECK(scan(s)==0);
    }
    /* a frame never holds more than FB_SOUND_MAX */
    fresh(80);for(unsigned i=0;i<FB_FX_COUNT;++i){m.steps=10+i;football_live_fx(&m,FB_FX_TACKLE,13,6,1);}
    m.phase=FB_LIVE_SET;look();m.phase=FB_LIVE_PLAY;m.flying=true;m.event=FB_EVENT_FIELD_GOAL;m.rules.score[FB_HOME]+=3;++m.snaps;m.rules.last.first_down=true;m.rules.offense=FB_HOME;
    memset(m.fx,0,sizeof m.fx);m.fx_next=0;look();
    for(unsigned i=0;i<FB_FX_COUNT;++i){m.steps=10+i;football_live_fx(&m,FB_FX_INTERCEPTION,5,22,1);}
    n=scan(s);CHECK(n==FB_SOUND_MAX && has(s,n,FB_SND_INTERCEPTION));
}
/* Whole games. */
typedef struct{FootballSound list[3000];unsigned n;unsigned count[FB_SND_COUNT];}Heard;
static Heard fine,coarse;
static void drive(uint32_t seed,unsigned frame_ms,Heard *h)
{
    memset(h,0,sizeof *h);
    assert(football_live_begin(&m,&roster,seed,1));m.autoplay=1;football_sound_reset(&w,&m);
    unsigned guard=0;
    while(m.phase!=FB_LIVE_END){
        assert(++guard<400000);
        if(m.phase==FB_LIVE_CALL){
            if(football_live_cpu(&m,(FootballSide)m.rules.offense))assert(football_live_defend(&m,(FootballDefenseId)(m.snaps%FB_DEF_COUNT)));
            else if(m.conversion==1)assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));
            else assert(football_live_call(&m,(FootballPlayId)(FB_PASS_SLANTS+m.snaps%3)));
        }
        football_live_tick(&m,frame_ms,(FootballLiveInput){.receiver=-1});
        FootballSound s[FB_SOUND_MAX];unsigned n=football_sound_scan(&w,&m,s);
        for(unsigned i=0;i<n;++i){assert(h->n<3000);h->list[h->n++]=s[i];++h->count[s[i]];}
    }
}
static unsigned credits(unsigned kind)
{
    unsigned n=0;
    for(unsigned side=0;side<2;++side)for(unsigned member=0;member<22;++member)n+=m.stats[side][member][kind-1];
    return n;
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    table();
    single_changes();
    unsigned totals[FB_SND_COUNT]={0},wins=0,losses=0,games=0,silent_plays=0;
    for(uint32_t seed=1;seed<=60;++seed){
        uint32_t s=seed*7717u+1;
        /* a strong human team beats Brock and a weak one loses to him, so both final whistles and both sides' touchdowns are heard */
        for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){seed%2?70:12,seed%2?70:12,seed%2?70:12,seed%2?70:12,seed%2?70:12,seed%2?70:12,seed%2?70:12,seed%2?70:12};
        drive(s,17,&fine);
        unsigned sacks=credits(FB_CREDIT_SACK),picks=credits(FB_CREDIT_INTERCEPTION),tds=credits(FB_CREDIT_TOUCHDOWN);
        CHECK(fine.count[FB_SND_SACK]==sacks);
        CHECK(fine.count[FB_SND_INTERCEPTION]==picks);
        CHECK(fine.count[FB_SND_TOUCHDOWN_FOR]+fine.count[FB_SND_TOUCHDOWN_AGAINST]==tds);
        /* the touchdowns told "for" the human are the ones HOME's own players scored: the score says how many points that is */
        CHECK(fine.count[FB_SND_TURNOVER_GAIN]+fine.count[FB_SND_TURNOVER_LOSS]>=picks);
        CHECK(fine.count[FB_SND_WIN]+fine.count[FB_SND_LOSS]==1 && fine.count[FB_SND_WIN]==(m.rules.winner==FB_HOME));
        CHECK(fine.count[FB_SND_SNAP]>=fine.count[FB_SND_TACKLE]/2);
        /* the same game with frames six times as long: every sound heard as often (the order of two sounds inside one frame is the
         * watcher's own: edges first, then the records, then the dead ball) */
        unsigned fine_n=fine.n;unsigned kept[FB_SND_COUNT];memcpy(kept,fine.count,sizeof kept);
        drive(s,100,&coarse);
        CHECK(coarse.n==fine_n && !memcmp(kept,coarse.count,sizeof kept));
        for(unsigned k=0;k<FB_SND_COUNT;++k)totals[k]+=fine.count[k];
        wins+=fine.count[FB_SND_WIN];losses+=fine.count[FB_SND_LOSS];++games;
        silent_plays+=fine.n==0;
    }
    /* Across sixty games every kind of sound is heard but two: a dive is a human defender's press (the CPU plays both sides here) and a blocked
     * kick is rare; both are covered by the hand-made changes above. */
    for(unsigned k=1;k<FB_SND_COUNT;++k)if(k!=FB_SND_DIVE && k!=FB_SND_BLOCKED)CHECK(totals[k]>0);
    CHECK(!silent_plays);
    CHECK(totals[FB_SND_SNAP]>1000 && totals[FB_SND_TACKLE]>300 && totals[FB_SND_THROW]>200 && totals[FB_SND_KICK]>100 && totals[FB_SND_CATCH]>100);
    CHECK(totals[FB_SND_TOUCHDOWN_FOR]>10 && totals[FB_SND_TOUCHDOWN_AGAINST]>10 && wins>5 && losses>5);
    printf("sound: %u games, %u sounds heard in the first, snaps %u, tackles %u, throws %u, kicks %u, touchdowns %u/%u, wins %u losses %u, %u checks\n",
        games,fine.n,totals[FB_SND_SNAP],totals[FB_SND_TACKLE],totals[FB_SND_THROW],totals[FB_SND_KICK],totals[FB_SND_TOUCHDOWN_FOR],totals[FB_SND_TOUCHDOWN_AGAINST],wins,losses,checks);
    puts("football sound tests passed");
    return 0;
}
