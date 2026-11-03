#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* Trips and Tight put three of WR1, WR2, WR3 and the TE on one side of the field. Cover 2 and Cover 3 used to have two underneath defenders there,
 * so the slot receiver was alone (open in 86% and 84% of the Trips snaps at 1.2 s). Now the underneath defenders nearest that sideline line up on the
 * three receivers' spots. Two and two (Spread, Jet) and every other call keep the fixed lanes. */
static FootballRoster roster;
static FootballLive m;
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static void setup(uint32_t seed,unsigned away,FootballDefenseId defense,const float snap[4])
{
    assert(football_live_begin(&m,&roster,seed,1));
    m.rules.offense=away?FB_AWAY:FB_HOME;m.rules.down=1;m.rules.ball=40;m.rules.first_down_at=50;m.rules.quarter=2;m.rules.seconds_left=900;
    m.autoplay=1;
    assert(football_live_call(&m,FB_PASS_SLANTS));
    for(unsigned k=0;k<4;++k)m.snap_y[2+k]=snap[k];            /* WR1, WR2, WR3, TE */
    m.defense=defense;football_live_defense_setup(&m);
}
static const float spread[4]={4,49,13,39},jet[4]={6,48,24.2f,36},trips[4]={3.5f,49.5f,9.5f,16},tight[4]={6,47.5f,38.2f,34.8f};
static bool close(float a,float b){return fabsf(a-b)<.001f;}
/* The expected lanes of ranks 4..10 (indexed by rank). */
static void expect(const float *lane,unsigned def)
{
    for(unsigned r=4;r<11;++r){CHECK(close(m.zone_y[r],lane[r]));CHECK(close(m.actors[def+r].y,lane[r]));}
}
static void lanes(void)
{
    for(unsigned away=0;away<2;++away){
        unsigned def=away?0:11;
        /* Cover 2: LB4 15, LB5 26.67, LB6 38, CB flats 5 and 48, safeties 18 and 36. */
        static const float c2_fixed[11]={0,0,0,0,15,26.67f,38,5,48,18,36};
        setup(7,away,FB_DEF_ZONE,spread);expect(c2_fixed,def);
        setup(7,away,FB_DEF_ZONE,jet);expect(c2_fixed,def);
        /* Trips on the low side: CB7, LB4 and LB5 take the spots 3.5 (the sideline is 4), 9.5 and 16; LB6, CB8 and the safeties stay. */
        static const float c2_trips[11]={0,0,0,0,9.5f,16,38,4,48,18,36};
        setup(7,away,FB_DEF_ZONE,trips);expect(c2_trips,def);
        /* Tight on the high side: CB8, LB6 and LB5 take 47.5, 38.2 and 34.8. */
        static const float c2_tight[11]={0,0,0,0,15,34.8f,38.2f,5,47.5f,18,36};
        setup(7,away,FB_DEF_ZONE,tight);expect(c2_tight,def);
        /* Cover 3: LB4 flat 5, LB5 hook 20, LB6 hook 34, S10 flat 48 (the corners and the free safety are deep and never move). */
        static const float c3_fixed[11]={0,0,0,0,5,20,34,5,48,26.67f,48};
        setup(7,away,FB_DEF_COVER3,spread);expect(c3_fixed,def);
        setup(7,away,FB_DEF_COVER3,jet);expect(c3_fixed,def);
        static const float c3_trips[11]={0,0,0,0,4,9.5f,16,5,48,26.67f,48};
        setup(7,away,FB_DEF_COVER3,trips);expect(c3_trips,def);
        static const float c3_tight[11]={0,0,0,0,5,34.8f,38.2f,5,48,26.67f,47.5f};
        setup(7,away,FB_DEF_COVER3,tight);expect(c3_tight,def);
    }
}
/* The other zone calls are untouched, whatever the set (the man calls follow their receivers' real spots, so they are not compared). */
static void others(void)
{
    static const float *sets[4]={spread,trips,tight,jet};
    for(unsigned d=0;d<FB_DEF_COUNT;++d){
        if(football_defense_matches_overload((FootballDefenseId)d) || !football_defense_is_zone((FootballDefenseId)d))continue;
        for(unsigned away=0;away<2;++away){
            float lane[11],x[11],y[11];unsigned def=away?0:11;
            setup(11,away,(FootballDefenseId)d,spread);
            for(unsigned r=0;r<11;++r){lane[r]=m.zone_y[r];x[r]=m.actors[def+r].x;y[r]=m.actors[def+r].y;}
            for(unsigned s=1;s<4;++s){
                setup(11,away,(FootballDefenseId)d,sets[s]);
                for(unsigned r=0;r<11;++r)CHECK(close(lane[r],m.zone_y[r]) && close(x[r],m.actors[def+r].x) && close(y[r],m.actors[def+r].y));
            }
        }
    }
    for(unsigned d=0;d<FB_DEF_COUNT;++d)CHECK(football_defense_matches_overload((FootballDefenseId)d)==(d==FB_DEF_ZONE||d==FB_DEF_COVER3));
}
/* Anywhere on the field: a lane inside the sideline and the same x as before. */
static void field(void)
{
    static const float *sets[4]={spread,trips,tight,jet};
    for(unsigned d=0;d<2;++d)for(unsigned s=0;s<4;++s)for(unsigned away=0;away<2;++away)for(unsigned mirror=0;mirror<2;++mirror){
        float flipped[4];for(unsigned k=0;k<4;++k)flipped[k]=mirror?53.333f-sets[s][k]:sets[s][k];
        setup(3,away,d?FB_DEF_COVER3:FB_DEF_ZONE,flipped);
        for(unsigned r=4;r<11;++r)CHECK(m.zone_y[r]>=3.99f && m.zone_y[r]<=49.34f);
    }
}
/* The measured thing: with the QB holding, the slot (WR3) is not left alone at 1.2 s in the sets that overload a side. It was 86% and 84% in Trips. */
static void held(void)
{
    static const FootballDefenseId calls[]={FB_DEF_ZONE,FB_DEF_COVER3};
    for(unsigned c=0;c<2;++c){
        unsigned n[FB_ALIGN_COUNT]={0},open[FB_ALIGN_COUNT]={0};
        for(unsigned p=0;p<FB_PLAY_COUNT;++p){
            const FootballPlayDefinition *pd=football_playbook_get((FootballPlayId)p);if(pd->type!=FB_PLAY_PASS)continue;
            for(unsigned k=0;k<40;++k){
                assert(football_live_begin(&m,&roster,1u+k*7717u+p*31u+calls[c],1));
                for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
                m.autoplay=0;m.rules.offense=FB_HOME;m.rules.down=1;m.rules.ball=40;m.rules.first_down_at=50;m.rules.quarter=2;m.rules.seconds_left=900;
                assert(football_live_call(&m,(FootballPlayId)p));m.defense=calls[c];football_live_defense_setup(&m);
                unsigned before=m.snaps,guard=0;bool snapped=false,seen=false;
                while(m.snaps==before && !seen){
                    FootballLiveInput in={.receiver=-1};if(m.phase==FB_LIVE_SET && !snapped){in.pressed=FB_LIVE_ACTION;snapped=true;}
                    football_live_tick(&m,16,in);assert(++guard<20000);
                    if(m.phase==FB_LIVE_PLAY && m.elapsed>=1.2f && !m.thrown && m.carrier==0 && !m.scramble){
                        seen=true;float near=999;
                        for(unsigned q=11;q<22;++q){float g=hypotf(m.actors[4].x-m.actors[q].x,m.actors[4].y-m.actors[q].y);if(g<near)near=g;}
                        ++n[m.align];if(near>4.f)++open[m.align];
                    }
                }
            }
        }
        for(unsigned a=FB_ALIGN_TRIPS;a<=FB_ALIGN_TIGHT;++a){
            printf("  %s, %s: WR3 more than 4 yd from every defender at 1.2 s: %u of %u (%.1f%%)\n",c?"Cover 3":"Cover 2",a==FB_ALIGN_TRIPS?"Trips":"Tight",open[a],n[a],100.*open[a]/n[a]);
            CHECK(n[a]>=40);
            CHECK(open[a]*100<=n[a]*(a==FB_ALIGN_TRIPS?10u:30u));   /* Trips was 86% / 84%, Cover 3 Tight 59% */
        }
    }
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    lanes();others();field();held();
    printf("defense overload: %u checks (Cover 2 and Cover 3 put the underneath defenders nearest the overloaded sideline on the three receivers' spots; Spread, Jet and the other calls keep the fixed lanes; lanes stay in the field; the slot is no longer left alone in Trips or Tight) passed\n",checks);
}
