#include "football_progress_fixture.h"
#include <stdio.h>
#include <string.h>
/* Football achievements: a match won counts a win (a title for the seventh round, a shutout when nothing was conceded, a blowout for a margin of
 * GC_FB_BLOWOUT_MARGIN), none of it reaches the profile before football_career_flush_achievements (the checkpoint), a loss counts nothing, and
 * what a flush unlocks is kept in achv_new for the screen. */
static gc_run scratch;
static gc_system profile;
static FootballCareer career;
static void win(unsigned home,unsigned away)
{
    career.live.rules.score[FB_HOME]=(uint16_t)home;career.live.rules.score[FB_AWAY]=(uint16_t)away;
    fb_play_win(&career);
}
int main(void)
{
    (void)fb_win_round; /* a fixture helper this test has no use for */
    football_test_unlock_all(&profile);
    assert(GC_ACHV_COUNT==100 && !strcmp(gc_achv_defs[91].name,"Kickoff") && !strcmp(gc_achv_defs[99].name,"Blowout") && gc_achv_defs[91].group==GC_ACHV_GROUP_FOOTBALL);
    const gc_achv_progress zero={0};
    assert(!memcmp(&profile.achv.fb_wins,&zero.fb_wins,sizeof zero.fb_wins) && profile.achv.fb_titles==0);

    /* ---- a 10-7 win: a win, nothing else; not the profile's before the flush ---- */
    fb_career_of(&career,&scratch,&profile,4);
    football_career_flush_achievements(&career);career.achv_new.n=0; /* the profile's other achievements (the fixture owns plenty) are settled first */
    assert(!gc_achv_unlocked(&profile,91));
    win(10,7);
    assert(career.achv_wins==1 && career.achv_titles==0 && career.achv_shutouts==0 && career.achv_blowouts==0);
    assert(profile.achv.fb_wins==0 && !gc_achv_unlocked(&profile,91));
    assert(football_career_flush_achievements(&career)==1 && profile.achv.fb_wins==1);
    assert(gc_achv_unlocked(&profile,91) && !gc_achv_unlocked(&profile,98) && !gc_achv_unlocked(&profile,99));
    assert(career.achv_new.n==1 && career.achv_new.id[0]==91);
    assert(football_career_flush_achievements(&career)==0 && profile.achv.fb_wins==1 && career.achv_new.n==1); /* once */
    assert(football_career_continue(&career));

    /* ---- 21-1 is a margin of 20 (not a blowout, not a shutout), 28-7 is 21 (a blowout); 14-0 is a shutout ---- */
    win(21,1);assert(career.achv_shutouts==0 && career.achv_blowouts==0); /* margin 20, one point conceded */
    assert(football_career_flush_achievements(&career)==0 && !gc_achv_unlocked(&profile,99) && !gc_achv_unlocked(&profile,98));
    assert(football_career_continue(&career));
    win(28,7);assert(football_career_flush_achievements(&career)==1 && gc_achv_unlocked(&profile,99) && !gc_achv_unlocked(&profile,98));
    assert(football_career_continue(&career));
    win(14,0);assert(football_career_flush_achievements(&career)==1 && gc_achv_unlocked(&profile,98));
    assert(career.achv_new.n==3 && career.achv_new.id[1]==99 && career.achv_new.id[2]==98);
    assert(profile.achv.fb_wins==4 && profile.achv.fb_shutouts==1 && profile.achv.fb_blowouts==1);
    assert(football_career_continue(&career));
    for(unsigned r=5;r<=FB_CAREER_ROUNDS;++r){
        assert(career.round==r);
        win(10,7);
        assert(career.achv_titles==(r==FB_CAREER_ROUNDS?1u:0u));
        football_career_flush_achievements(&career);
        assert(gc_achv_unlocked(&profile,95)==(r==FB_CAREER_ROUNDS));
        assert(football_career_continue(&career));
    }
    assert(profile.achv.fb_wins==FB_CAREER_ROUNDS && profile.achv.fb_titles==1 && !gc_achv_unlocked(&profile,96));

    /* ---- a loss counts nothing ---- */
    fb_career_of(&career,&scratch,&profile,4);
    career.live.rules.score[FB_HOME]=3;career.live.rules.score[FB_AWAY]=30;career.live.rules.winner=FB_AWAY;career.phase=FB_CAREER_RESULT;
    assert(football_career_continue(&career) && career.achv_wins==0 && career.achv_shutouts==0 && career.achv_blowouts==0);
    assert(football_career_flush_achievements(&career)==0 && profile.achv.fb_wins==FB_CAREER_ROUNDS);

    /* ---- the counts: 10, 50, 200 wins; 3 and 10 titles; the thresholds are exactly these ---- */
    memset(&profile.achv,0,sizeof profile.achv);memset(profile.achievement_flags,0,sizeof profile.achievement_flags);
    static const struct{int id;uint32_t wins,titles;}T[]={{91,1,0},{92,10,0},{93,50,0},{94,200,0},{95,0,1},{96,0,3},{97,0,10}};
    for(unsigned i=0;i<sizeof T/sizeof *T;++i){
        memset(&profile.achv,0,sizeof profile.achv);memset(profile.achievement_flags,0,sizeof profile.achievement_flags);
        profile.achv.fb_wins=T[i].wins?T[i].wins-1:0;profile.achv.fb_titles=T[i].titles?T[i].titles-1:0;
        gc_achv_evaluate(&profile,"fbcount",0,NULL);assert(!gc_achv_unlocked(&profile,T[i].id));
        gc_achv_football_won(&profile,T[i].wins?1:0,T[i].titles?1:0,0,0);
        gc_achv_evaluate(&profile,"fbcount",0,NULL);assert(gc_achv_unlocked(&profile,T[i].id));
    }
    gc_achv_football_won(&profile,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX);
    assert(profile.achv.fb_wins==UINT32_MAX && profile.achv.fb_blowouts==UINT32_MAX); /* saturates */
    gc_achv_football_won(NULL,1,1,1,1);
    football_career_flush_achievements(NULL);

    /* ---- a career with no profile earns nothing ---- */
    fb_career_of(&career,&scratch,&profile,4);career.profile=NULL;
    win(40,0);assert(career.achv_wins==0 && football_career_flush_achievements(&career)==0);
    puts("achv_football_test: all passed");
    return 0;
}
