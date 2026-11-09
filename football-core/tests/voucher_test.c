#include "football_progress_fixture.h"
#include <stdio.h>
#include <string.h>
/* Egg vouchers from the playoff (lorenzo 2026-11-09): beating a Gym Leader (rounds 1-8) pays one Regular voucher, an Elite Four member
 * (9-12) one Plus voucher, the Champion (13) one Premium voucher; nothing for a loss, and nothing in the profile before the checkpoint that
 * records the win (nor twice for one win). */
static gc_run scratch,other;
static gc_system profile;
static FootballCareer career,loaded;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static unsigned vouchers(int type){return profile.voucher_counts[type];}
int main(void)
{
    (void)fb_win_round; /* a fixture helper this test has no use for */
    football_test_unlock_all(&profile);
    const unsigned regular0=vouchers(GC_VOUCHER_REGULAR),plus0=vouchers(GC_VOUCHER_PLUS),premium0=vouchers(GC_VOUCHER_PREMIUM),golden0=vouchers(GC_VOUCHER_GOLDEN);
    assert(FB_CAREER_ROUNDS==13 && FB_VOUCHER_PER_WIN==1);
    assert(football_career_round_voucher(0)<0 && football_career_round_voucher(14)<0);
    for(unsigned round=1;round<=FB_CAREER_ROUNDS;++round)assert(football_career_round_voucher(round)==(round<=8?GC_VOUCHER_REGULAR:round<=12?GC_VOUCHER_PLUS:GC_VOUCHER_PREMIUM));
    assert(!strcmp(football_voucher_name(GC_VOUCHER_REGULAR),"Egg Voucher") && !strcmp(football_voucher_name(GC_VOUCHER_PLUS),"Egg Voucher Plus") && !strcmp(football_voucher_name(GC_VOUCHER_PREMIUM),"Egg Voucher Premium"));

    /* ---- a title run: 8 Regular vouchers for the Gym Leaders, 4 Plus for the Elite Four, 1 Premium for the Champion ---- */
    fb_career_of(&career,&scratch,&profile,4);
    assert(career.vouchers_regular==0 && career.vouchers_plus==0 && career.vouchers_premium==0 && football_career_flush_vouchers(&career)==0);
    unsigned got[3]={0,0,0}; /* regular, plus, premium paid so far */
    for(unsigned round=1;round<=FB_CAREER_ROUNDS;++round){
        assert(career.round==round);
        fb_play_win(&career);
        assert(career.wins==round);
        const unsigned kind=round<=8?0:round<=12?1:2;
        /* Due, but not the profile's yet: it is the checkpoint that moves them. */
        assert(career.vouchers_regular==(kind==0) && career.vouchers_plus==(kind==1) && career.vouchers_premium==(kind==2));
        assert(vouchers(GC_VOUCHER_REGULAR)==regular0+got[0] && vouchers(GC_VOUCHER_PLUS)==plus0+got[1] && vouchers(GC_VOUCHER_PREMIUM)==premium0+got[2]);
        assert(football_career_flush_vouchers(&career)==1);++got[kind];
        assert(vouchers(GC_VOUCHER_REGULAR)==regular0+got[0] && vouchers(GC_VOUCHER_PLUS)==plus0+got[1] && vouchers(GC_VOUCHER_PREMIUM)==premium0+got[2]);
        assert(football_career_flush_vouchers(&career)==0 && vouchers(GC_VOUCHER_REGULAR)==regular0+got[0]); /* once */
        if(round<FB_CAREER_ROUNDS){assert(football_career_continue(&career) && career.phase==FB_CAREER_READY);}
    }
    assert(got[0]==8 && got[1]==4 && got[2]==1);
    assert(football_career_continue(&career) && career.phase==FB_CAREER_END && career.wins==FB_CAREER_ROUNDS);
    assert(vouchers(GC_VOUCHER_GOLDEN)==golden0); /* no other kind is touched */

    /* ---- a run that is knocked out in round 4: three Regular vouchers, no Premium ---- */
    const unsigned regular1=vouchers(GC_VOUCHER_REGULAR),premium1=vouchers(GC_VOUCHER_PREMIUM);
    fb_career_of(&career,&scratch,&profile,4);
    for(unsigned round=1;round<=3;++round){fb_play_win(&career);assert(football_career_flush_vouchers(&career)==1);assert(football_career_continue(&career));}
    career.live.rules.winner=FB_AWAY;career.phase=FB_CAREER_RESULT;
    assert(career.round==4 && football_career_continue(&career) && career.phase==FB_CAREER_END && career.wins==3);
    assert(career.vouchers_regular==0 && career.vouchers_plus==0 && career.vouchers_premium==0 && football_career_flush_vouchers(&career)==0); /* the loss pays nothing */
    assert(vouchers(GC_VOUCHER_REGULAR)==regular1+3 && vouchers(GC_VOUCHER_PREMIUM)==premium1);

    /* ---- the checkpoint never knows about vouchers: a resumed career owes nothing it paid, and loses what it never wrote down ---- */
    const unsigned regular2=vouchers(GC_VOUCHER_REGULAR);
    fb_career_of(&career,&scratch,&profile,4);
    fb_play_win(&career);
    assert(career.vouchers_regular==1);
    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    assert(football_career_flush_vouchers(&career)==1 && vouchers(GC_VOUCHER_REGULAR)==regular2+1); /* the screen: write, then flush */
    assert(football_career_load(&loaded,&other,&profile,bytes,n) && loaded.wins==1 && loaded.phase==FB_CAREER_SHOP);
    assert(loaded.vouchers_regular==0 && loaded.vouchers_plus==0 && loaded.vouchers_premium==0 && football_career_flush_vouchers(&loaded)==0);
    assert(vouchers(GC_VOUCHER_REGULAR)==regular2+1); /* one win, one voucher, however often it is resumed */
    /* A career resumed from before the win replays the match and is paid for it once. */
    fb_career_of(&career,&scratch,&profile,4);career.phase=FB_CAREER_READY;
    n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    assert(football_career_load(&loaded,&other,&profile,bytes,n) && loaded.phase==FB_CAREER_READY && loaded.wins==0);
    loaded.live.rules.winner=FB_HOME;loaded.phase=FB_CAREER_RESULT;
    assert(football_career_continue(&loaded) && loaded.vouchers_regular==1);
    assert(football_career_flush_vouchers(&loaded)==1 && vouchers(GC_VOUCHER_REGULAR)==regular2+2);

    /* ---- no profile, no vouchers ---- */
    fb_career_of(&career,&scratch,&profile,4);
    career.profile=NULL;
    const unsigned regular3=vouchers(GC_VOUCHER_REGULAR);
    fb_play_win(&career);
    assert(career.vouchers_regular==0 && football_career_flush_vouchers(&career)==0 && vouchers(GC_VOUCHER_REGULAR)==regular3);
    assert(football_career_flush_vouchers(NULL)==0);

    printf("football vouchers: Regular per Gym Leader, Plus per Elite Four member, Premium for the Champion, nothing for a loss, once per win across checkpoints and resumes passed\n");
    return 0;
}
