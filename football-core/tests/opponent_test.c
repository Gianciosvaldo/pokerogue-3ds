#include "football_progress_fixture.h"
#include <stdio.h>
#include <string.h>
/* The playoff is thirteen matches against the Kanto line-up (lorenzo 2026-11-09): eight Gym Leaders, the Elite Four, the Champion. Every Gym
 * Leader's and Elite Four member's team is made of forms of their type, with no Pokemon twice on a team; the Champion's is stacked; and the
 * teams get stronger every round. */
static gc_run scratch;
static gc_system profile;
static FootballCareer career;
static FootballLive live;
static const pr_form *form_of(unsigned species,unsigned index)
{
    for(unsigned f=0;f<PR_FORM_COUNT;++f)if(pr_forms[f].species==species && pr_forms[f].form_index==index)return &pr_forms[f];
    return NULL;
}
static double team_strength(unsigned round,unsigned seed)
{
    assert(football_live_begin(&live,&career.roster,seed,round));
    double sum=0;
    for(unsigned i=0;i<22;++i){const uint8_t *a=(const uint8_t *)&live.units[FB_AWAY][i];for(unsigned k=0;k<sizeof(FootballAttributes);++k)sum+=a[k];}
    return sum/22;
}
int main(void)
{
    (void)fb_play_win;(void)fb_win_round; /* fixture helpers this test has no use for */
    football_test_unlock_all(&profile);
    fb_career_of(&career,&scratch,&profile,4);
    assert(FB_CAREER_ROUNDS==13 && FB_GYM_LEADERS+FB_ELITE_FOUR+1==FB_CAREER_ROUNDS);
    assert(!football_opponent(0) && !football_opponent(FB_CAREER_ROUNDS+1));
    assert(!football_live_begin(&live,&career.roster,1,0) && !football_live_begin(&live,&career.roster,1,FB_CAREER_ROUNDS+1));

    /* ---- the line-up: Kanto's, in the classic game's order, each with the type the classic game gives the trainer ---- */
    static const struct{const char *name;int trainer,role,type;}E[FB_CAREER_ROUNDS]={
        {"Brock",TRAINER_TYPE_BROCK,0,POKEMON_TYPE_ROCK},{"Misty",TRAINER_TYPE_MISTY,0,POKEMON_TYPE_WATER},{"Lt. Surge",TRAINER_TYPE_LT_SURGE,0,POKEMON_TYPE_ELECTRIC},
        {"Erika",TRAINER_TYPE_ERIKA,0,POKEMON_TYPE_GRASS},{"Janine",TRAINER_TYPE_JANINE,0,POKEMON_TYPE_POISON},{"Sabrina",TRAINER_TYPE_SABRINA,0,POKEMON_TYPE_PSYCHIC},
        {"Blaine",TRAINER_TYPE_BLAINE,0,POKEMON_TYPE_FIRE},{"Giovanni",TRAINER_TYPE_GIOVANNI,0,POKEMON_TYPE_GROUND},
        {"Lorelei",TRAINER_TYPE_LORELEI,1,POKEMON_TYPE_ICE},{"Bruno",TRAINER_TYPE_BRUNO,1,POKEMON_TYPE_FIGHTING},{"Agatha",TRAINER_TYPE_AGATHA,1,POKEMON_TYPE_GHOST},
        {"Lance",TRAINER_TYPE_LANCE,1,POKEMON_TYPE_DRAGON},{"Blue",TRAINER_TYPE_BLUE,2,-1}};
    extern const int8_t pr_trainer_specialty[];
    for(unsigned round=1;round<=FB_CAREER_ROUNDS;++round){
        const FootballOpponent *o=football_opponent(round);
        assert(o && !strcmp(o->name,E[round-1].name) && o->trainer_type==E[round-1].trainer && o->role==E[round-1].role && o->type==E[round-1].type);
        assert(o->type==pr_trainer_specialty[o->trainer_type]); /* the classic game's own specialty type */
        assert(!strcmp(football_opponent_role_name((FootballOpponentRole)o->role),round<=8?"Gym Leader":round<=12?"Elite Four":"Champion"));
    }
    assert(!strcmp(football_type_name(POKEMON_TYPE_ROCK),"Rock") && !strcmp(football_type_name(-1),"Any"));

    /* ---- the teams: every one of the 22 is a form of the leader's type (the Champion's: a strong one) ---- */
    unsigned players=0;
    for(unsigned round=1;round<=FB_CAREER_ROUNDS;++round)for(unsigned seed=1;seed<=40;++seed){
        assert(football_live_begin(&live,&career.roster,seed*7919u+round,round));
        const FootballOpponent *o=football_opponent(round);
        for(unsigned i=0;i<22;++i){
            const pr_form *f=form_of(live.away_species[i],live.away_form[i]);
            assert(f && football_opponent_form_fits(o,f));
            if(o->type>=0)assert(f->type1==o->type || f->type2==o->type);else assert(f->base_total>=FB_CHAMPION_MIN_BST);
            for(unsigned k=0;k<i;++k)assert(live.away_species[k]!=live.away_species[i]); /* no Pokemon twice on one team (lorenzo 2026-11-10) */
            ++players;
        }
    }
    /* Brock's team is full of Rock-types, and not the same forms every time. */
    {
        unsigned distinct=0;uint16_t seen[40*22];
        for(unsigned seed=1;seed<=40;++seed){
            assert(football_live_begin(&live,&career.roster,seed,1));
            for(unsigned i=0;i<22;++i){bool known=false;for(unsigned k=0;k<distinct;++k)known|=seen[k]==live.away_species[i];if(!known)seen[distinct++]=live.away_species[i];}
        }
        assert(distinct>=40);
    }

    /* ---- the teams get stronger: the Champion's stacked team and the round's scaling beat the first Gym Leader's by a wide margin ---- */
    double first=0,last=0,middle=0;
    for(unsigned seed=1;seed<=30;++seed){first+=team_strength(1,seed);middle+=team_strength(7,seed);last+=team_strength(FB_CAREER_ROUNDS,seed);}
    first/=30;middle/=30;last/=30;
    assert(middle>first && last>middle && last>first*1.35);
    /* ---- a Gym Leader's or Elite Four member's team is as strong as its round says, whatever its type (lorenzo 2026-11-10: Lt. Surge's, with the
     * Electric pool's Mega and legendary forms, came out about 10% above its neighbours): every team of rounds 1..12 is within 3% of the curve, so the
     * strength rises from one round to the next, and the Champion is far above the last Elite Four member ---- */
    {
        double previous=0;
        for(unsigned round=1;round<FB_CAREER_ROUNDS;++round){
            const double target=FB_LEADER_ATTRIBUTE_BASE*(1+(round-1)*.0625)*sizeof(FootballAttributes);double sum=0;
            for(unsigned seed=1;seed<=20;++seed){const double t=team_strength(round,seed);assert(t>target*.97 && t<target*1.03);sum+=t;}
            assert(sum/20>previous);previous=sum/20;
        }
        assert(last>previous*1.3);
    }
    printf("football opponents: 13 trainers, %u team members of the right type, team strength %.1f (Brock), %.1f (round 7), %.1f (Blue) passed\n",players,first,middle,last);
    puts("football opponent tests passed");
    return 0;
}
