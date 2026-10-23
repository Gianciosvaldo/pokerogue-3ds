#include "football_match.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballTeam team(void)
{
    FootballTeam t={0};t.count=6;
    for(unsigned i=0;i<6;++i)t.players[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};
    return t;
}
int main(void)
{
    FootballDefenseMenu menu={0};FootballDefenseId out;
    for(unsigned i=0;i<FB_DEF_COUNT;++i) {
        memset(&menu,0,sizeof menu);FootballTouchRect r=football_defense_rect(i);
        football_defense_update(&menu,(FootballCallInput){.touch_down=true,.touch_x=(uint16_t)(r.x+r.w),.touch_y=(uint16_t)r.y});
        assert(!football_defense_take(&menu,&out));
        football_defense_update(&menu,(FootballCallInput){.touch_down=true,.touch_x=(uint16_t)r.x,.touch_y=(uint16_t)r.y});
        assert(football_defense_take(&menu,&out) && out==(FootballDefenseId)i);
        assert(!football_defense_take(&menu,&out));
    }
    football_defense_update(&menu,(FootballCallInput){.pressed=FB_INPUT_UP});assert(menu.selected==FB_DEF_BLITZ);
    football_defense_update(&menu,(FootballCallInput){.pressed=FB_INPUT_BACK|FB_INPUT_ACCEPT});assert(menu.selected==0 && !menu.ready);
    assert(!football_defense_get(FB_DEF_COUNT));
    FootballTeam t=team();FootballMatch m,saved;
    assert(football_match_begin(&m,&t,&t,123,256));m.manual_defense=true;saved=m;
    assert(!football_match_defend(&m,FB_DEF_BLITZ) && !memcmp(&m,&saved,sizeof m));
    m.rules.offense=FB_AWAY;saved=m;
    assert(football_match_waiting_for_defense(&m));
    for(unsigned i=0;i<100;++i)assert(!football_match_tick(&m,250));
    assert(!memcmp(&m,&saved,sizeof m)); /* Menu thinking advances no clock or RNG. */
    assert(!football_match_defend(&m,FB_DEF_COUNT) && !memcmp(&m,&saved,sizeof m));
    assert(football_match_defend(&m,FB_DEF_ZONE) && m.defense==FB_DEF_ZONE && m.phase==FB_MATCH_ANIMATION);
    saved=m;assert(!football_match_defend(&m,FB_DEF_MAN) && !memcmp(&m,&saved,sizeof m));
    unsigned sacks[FB_DEF_COUNT]={0},complete[FB_DEF_COUNT]={0},runs[FB_DEF_COUNT]={0};
    int run_yards[FB_DEF_COUNT]={0};
    for(unsigned seed=1;seed<=4000;++seed) {
        FootballPlayId offense=FB_PLAY_COUNT;
        for(unsigned defense=0;defense<FB_DEF_COUNT;++defense) {
            assert(football_match_begin(&m,&t,&t,seed,256));m.manual_defense=true;m.rules.offense=FB_AWAY;
            assert(football_match_defend(&m,(FootballDefenseId)defense));
            if(defense==0)offense=m.play.id;else assert(m.play.id==offense); /* No defense-dependent play peeking. */
            sacks[defense]+=m.resolved.kind==FB_RESULT_SACK;
            complete[defense]+=m.resolved.kind==FB_RESULT_COMPLETE;
            if(m.play.type==FB_PLAY_RUN && m.resolved.kind==FB_RESULT_RUN){runs[defense]++;run_yards[defense]+=m.resolved.yards;}
        }
    }
    assert(sacks[FB_DEF_BLITZ]>sacks[FB_DEF_MAN] && complete[FB_DEF_ZONE]<complete[FB_DEF_BLITZ]);
    assert(runs[FB_DEF_MAN] && runs[FB_DEF_ZONE]);
    assert(run_yards[FB_DEF_ZONE]*(int)runs[FB_DEF_MAN]>run_yards[FB_DEF_MAN]*(int)runs[FB_DEF_ZONE]);
    puts("football defense: all touch calls, no idle RNG/clock, atomic guards and 12,000 matched risk/reward snaps passed");
}
