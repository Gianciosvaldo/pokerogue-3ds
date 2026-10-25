/* Deterministic native integration/economy audit. No fabricated win flags. */
#include "football_mode.h"
#include "football_playbook.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run roster;
static FootballMode mode;
typedef struct { const char *name;uint16_t species[3];unsigned count; } Squad;
static const Squad squads[]={
    {"starter_trio",{SPECIES_ID_BULBASAUR,SPECIES_ID_CHARMANDER,SPECIES_ID_SQUIRTLE},3},
    {"evolution_trio",{SPECIES_ID_NINCADA,SPECIES_ID_ABRA,SPECIES_ID_PIDGEY},3},
    {"legend_duo",{SPECIES_ID_MEWTWO,SPECIES_ID_PIDGEY},2},
    {"mega_duo",{SPECIES_ID_MAWILE,SPECIES_ID_BULBASAUR},2}
};
typedef struct { uint64_t matches,plays,earned,spent,left,vitamins,stones,rare,affordable,clears;unsigned reached[7],wins[7],max_plays; } Metrics;
static void input(unsigned key){football_mode_input(&mode,(FootballCallInput){.pressed=key},0);}
static void play(void)
{
    FootballMatch *m=&mode.session.match;
    if(football_match_waiting_for_home(m)){
        unsigned distance=football_rules_distance(&m->rules);
        FootballPlayId id=distance<=3 ? FB_RUN_DIVE : (m->plays%4==0 ? FB_PASS_VERTICALS : FB_PASS_SLANTS);
        const FootballPlayDefinition *p=football_playbook_get(id);
        PlayInfo call={p->formation,p->type,p->id,255};
        assert(football_match_call(m,call));
    }else if(football_match_waiting_for_defense(m)){
        FootballDefenseId defense=football_rules_distance(&m->rules)>6 ? FB_DEF_ZONE : FB_DEF_MAN;
        assert(football_match_defend(m,defense));
    }
}
static void shop(Metrics *totals)
{
    FootballSeason *s=&mode.season;uint32_t wallet=s->wallet;
    /* One reproducible policy: rare stone first if affordable, then vitamins
     * for the slowest/weakest eligible player. No offer re-rolls or fake money. */
    for(unsigned i=0;i<s->shop_count;++i)if(s->shop[i].item==FB_ITEM_MEGA_STONE){
        totals->rare++;totals->affordable+=s->wallet>=s->shop[i].price;
        if(football_season_buy(s,i,(unsigned)s->shop[i].target))totals->stones++;
    }
    for(unsigned offer=0;offer<s->shop_count;++offer)if(s->shop[offer].item!=FB_ITEM_MEGA_STONE){
        unsigned target=0,lowest=1000;
        for(unsigned i=0;i<(unsigned)roster.n_party;++i){
            unsigned rating=s->shop[offer].item==FB_ITEM_CARBOS ? s->players[i].attributes.spd : s->players[i].attributes.str;
            if(rating<lowest){lowest=rating;target=i;}
        }
        if(football_season_buy(s,offer,target))totals->vitamins++;
    }
    assert(s->wallet<=wallet);totals->spent+=wallet-s->wallet;
    for(unsigned i=0;i<s->shop_count;++i)if(s->shop[i].sold)assert(!football_season_buy(s,i,0));
    input(FB_INPUT_BACK);
}
int main(int argc,char **argv)
{
    unsigned seasons=512;if(argc>1){char *end;unsigned long n=strtoul(argv[1],&end,10);assert(!*end && n>=1 && n<=4096);seasons=(unsigned)n;}
    printf("{\"seeds_per_squad\":%u,\"difficulty_step_q8\":%u,\"exp_factor\":%u,\"policy\":\"short_yardage_dive_otherwise_slants_with_quarterly_verticals_zone_on_long_distance\",\"squads\":[\n",seasons,(unsigned)FB_DIFFICULTY_STEP_Q8,(unsigned)GC_FOOTBALL_EXP_FACTOR);
    for(unsigned squad=0;squad<sizeof squads/sizeof squads[0];++squad){
        Metrics totals={0};gc_starter draft[3]={0};
        for(unsigned i=0;i<squads[squad].count;++i){draft[i].species=squads[squad].species[i];draft[i].tera_type=-1;}
        const char *why=NULL;
        if(!gc_football_validate_draft(NULL,draft,(int)squads[squad].count,&why)){fprintf(stderr,"invalid squad %s: %s\n",squads[squad].name,why);return 1;}
        for(unsigned seed=1;seed<=seasons;++seed){
            assert(football_mode_new(&mode,&roster,NULL,seed));assert(football_mode_draft(&mode,draft,(int)squads[squad].count,NULL));
            uint64_t earned=0,spent_before=totals.spent;
            unsigned guard=0;
            while(mode.phase!=FB_MODE_END && ++guard<50000){
                FootballModePhase phase=mode.phase;
                if(phase==FB_MODE_ROUND){
                    totals.reached[mode.season.round-1]++;totals.matches++;
                    for(unsigned i=0;i<(unsigned)roster.n_party;++i)if(gc_football_owned_mega(&roster,i)){assert(football_season_select_mega(&mode.season,(int)i));break;}
                    input(FB_INPUT_ACCEPT);
                }else if(phase==FB_MODE_PLAY)play();
                else if(phase==FB_MODE_RESULT || phase==FB_MODE_MESSAGE)input(FB_INPUT_ACCEPT);
                else if(phase==FB_MODE_EVOLVE)input(FB_INPUT_ACCEPT); /* Explicit first eligible branch for this audit policy. */
                else if(phase==FB_MODE_LEARN)input(FB_INPUT_BACK);
                else if(phase==FB_MODE_SHOP)shop(&totals);
                unsigned wins=mode.season.wins,plays=mode.session.match.plays;
                football_mode_tick(&mode,250);
                assert(mode.phase!=FB_MODE_ERROR);
                if(mode.season.wins>wins){unsigned round=mode.season.round;totals.wins[round-1]++;earned+=400+100*round;}
                if(mode.session.match.plays>plays)totals.plays++;
                if(mode.session.match.plays>totals.max_plays)totals.max_plays=mode.session.match.plays;
            }
            assert(mode.phase==FB_MODE_END && guard<50000 && mode.season.wallet<=5600);
            assert(totals.spent-spent_before+mode.season.wallet==earned);
            totals.earned+=earned;totals.left+=mode.season.wallet;totals.clears+=mode.season.phase==FB_SEASON_CLEAR;
        }
        printf("%s{\"name\":\"%s\",\"seasons\":%u,\"clears\":%" PRIu64 ",\"matches\":%" PRIu64 ",\"plays\":%" PRIu64 ",\"max_match_plays\":%u,\"earned\":%" PRIu64 ",\"spent\":%" PRIu64 ",\"remaining\":%" PRIu64 ",\"vitamins\":%" PRIu64 ",\"stones\":%" PRIu64 ",\"rare_offers\":%" PRIu64 ",\"affordable_rare_offers\":%" PRIu64 ",\"reached\":[",squad?",\n":"",squads[squad].name,seasons,totals.clears,totals.matches,totals.plays,totals.max_plays,totals.earned,totals.spent,totals.left,totals.vitamins,totals.stones,totals.rare,totals.affordable);
        for(unsigned i=0;i<7;++i){printf("%s%u",i?",":"",totals.reached[i]);}printf("],\"wins\":[");
        for(unsigned i=0;i<7;++i){printf("%s%u",i?",":"",totals.wins[i]);}printf("]}");
    }
    puts("\n]}");return 0;
}
