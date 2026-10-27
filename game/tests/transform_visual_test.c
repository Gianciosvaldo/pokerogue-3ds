/* Actual core -> application event conversion, including a same-species copy. */
#include "battle_events.h"
#include "battle_setup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"Transform visual line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
const char *i18n_language(void){return "en";}
const char *i18n_species_name(int id){static char name[32];snprintf(name,sizeof name,"Species_%d",id);return name;}
char *pak_read(const char *key,size_t *len){(void)key;if(len)*len=0;return NULL;}
void bv_play(const bv_event *ev,int n,void (*done)(void *),void *arg){(void)ev;(void)n;(void)done;(void)arg;}
static void run_case(int species,int form,bool imposter)
{
    static bs_battle b;bs_begin(&b,BIOME_ID_GRASS,3,"ditto-visual");b.eng.current=-1;b.eng.n_levels=1;b.bt.engine=&b.eng;
    int dm[]={MOVE_ID_TRANSFORM},tm[]={MOVE_ID_SKETCH,MOVE_ID_TACKLE};
    pr_pokemon *user=bs_add(&b,0,SPECIES_ID_DITTO,50,dm,1),*target=bs_add(&b,1,species,50,tm,2);
    user->gender=-1;user->shiny=true;user->variant=2;user->status=STATUS_EFFECT_POISON;
    target->form=form;target->gender=1;target->pp_used[0]=1;target->pp_up[1]=3;
    target->stat_stages[STAT_ACC-1]=4;target->n_summon_types=2;target->summon_types[0]=POKEMON_TYPE_FIRE;target->summon_types[1]=POKEMON_TYPE_DRAGON;
    bv_mon shown_user,shown_target;be_mon(&b.bt,user,&shown_user);be_mon(&b.bt,target,&shown_target);
    int hp=user->hp,max=user->stats[STAT_HP];
    int phase=pr_phase_new(&b.eng,PH_TRANSFORM,0);CHECK(phase>=0);b.eng.pool[phase].transform_target=2;b.eng.pool[phase].transform_sound=imposter;
    pr_phase_run(&b.bt,&b.eng,phase);
    static bv_event events[32];int n=be_convert(&b.bt,&b.eng,&shown_user,&shown_target,events,32),copies=0;
    for(int i=0;i<n;i++){
        CHECK(events[i].type!=BEV_FORM_CHANGE);
        if(events[i].type!=BEV_TRANSFORM)continue;
        copies++;
        const bv_mon *display=events[i].mon;CHECK(display!=NULL&&events[i].a==imposter);
        CHECK(display->species==species&&display->form==form&&display->gender==1&&display->female);
        CHECK(display->shiny&&display->variant==2&&!strcmp(display->name,"Species_132"));
        CHECK(display->sprite_scale==1); /* copied G-Max image does not make Ditto a G-Max Pokemon */
        CHECK(display->hp==hp&&display->max_hp==max&&display->status==STATUS_EFFECT_POISON);
        CHECK(display->n_moves==2&&display->moves[0]==MOVE_ID_SKETCH&&display->pp_max[0]==1&&display->pp_left[0]==1);
        CHECK(display->moves[1]==MOVE_ID_TACKLE&&display->pp_max[1]==5&&display->pp_left[1]==5);
        CHECK(display->n_types==2&&display->types[0]==POKEMON_TYPE_FIRE&&display->types[1]==POKEMON_TYPE_DRAGON&&display->stat_stages[STAT_ACC-1]==4);
    }
    CHECK(copies==1&&user->species==SPECIES_ID_DITTO&&user->form==0&&user->gender==-1);
    bv_mon restored;pr_reset_summon_data(user);be_mon(&b.bt,user,&restored);
    CHECK(restored.species==SPECIES_ID_DITTO&&restored.form==0&&restored.gender==-1&&!restored.female);
    CHECK(restored.moves[0]==MOVE_ID_TRANSFORM&&restored.hp==hp&&restored.max_hp==max);
}
static void switch_imposter(void)
{
    static bs_battle b;bs_begin(&b,BIOME_ID_GRASS,3,"ditto-switch-visual");
    int splash[]={MOVE_ID_SPLASH},transform[]={MOVE_ID_TRANSFORM};
    pr_pokemon *old=bs_add(&b,0,SPECIES_ID_BULBASAUR,40,splash,1);
    pr_pokemon *ditto=bs_add(&b,0,SPECIES_ID_DITTO,21,transform,1);
    pr_pokemon *enemy=bs_add(&b,1,SPECIES_ID_PIKACHU,50,splash,1);
    old->variant=1;ditto->gender=-1;ditto->variant=2;enemy->gender=1;
    ditto->base_ability=ditto->ability=ABILITY_ID_IMPOSTER;
    bv_mon shown,foe;be_mon(&b.bt,old,&shown);be_mon(&b.bt,enemy,&foe);
    bs_turn_switch(&b,1);CHECK(b.bt.field[0]==ditto&&ditto->transformed&&!b.bt.n_unported);
    static bv_event events[128];int n=be_convert(&b.bt,&b.eng,&shown,&foe,events,128),summoned=0,copied=0;
    for(int i=0;i<n;i++){
        if(events[i].type==BEV_SUMMON&&events[i].battler==0){
            CHECK(events[i].mon&&events[i].mon->species==SPECIES_ID_DITTO&&events[i].mon->gender==-1);
            CHECK(events[i].mon->level==21&&events[i].mon->variant==2);summoned++;
        }
        if(events[i].type==BEV_TRANSFORM&&events[i].battler==0){
            CHECK(summoned==1&&events[i].mon&&events[i].mon->species==SPECIES_ID_PIKACHU);
            CHECK(events[i].mon->level==21&&events[i].mon->variant==2&&events[i].mon->gender==1);
            CHECK(!strcmp(events[i].mon->name,"Species_132"));copied++;
        }
    }
    CHECK(summoned==1&&copied==1);
}
static void switch_identity_after_form_change(void)
{
    for(int zero_id=0;zero_id<2;zero_id++){
        static bs_battle b;bs_begin(&b,BIOME_ID_GRASS,3,"same-species-form-switch");
        int splash[]={MOVE_ID_SPLASH};
        pr_pokemon *other=bs_add(&b,0,SPECIES_ID_MORPEKO,50,splash,1);
        pr_pokemon *sent=bs_add(&b,0,SPECIES_ID_MORPEKO,50,splash,1);
        pr_pokemon *enemy=bs_add(&b,1,SPECIES_ID_BULBASAUR,50,splash,1);
        if(zero_id)sent->id=0;
        other->form=0;sent->form=1;b.bt.field[0]=sent;b.bt.engine=&b.eng;
        bv_mon shown,foe;be_mon(&b.bt,other,&shown);be_mon(&b.bt,enemy,&foe);
        b.eng.n_events=0;pr_event_add(&b.bt,&b.eng,PR_EV_SWITCH_IN,0,-1,0,1,NULL);
        CHECK(b.eng.n_events==1&&b.eng.events[0].field[0].pokemon_id_known);
        CHECK(b.eng.events[0].field[0].pokemon_id==sent->id);
        /* The next turn's Hunger Switch and a later party reorder leave the
         * other member looking exactly like the earlier switch snapshot. */
        sent->form=0;other->form=1;b.bt.party[0][0]=sent;b.bt.party[0][1]=other;
        be_view view={0};view.mon[0]=shown;view.mon[1]=foe;view.present[0]=view.present[1]=true;
        static bv_event events[32];int n=be_convert_events(&b.bt,b.eng.events,1,true,&view,events,32);
        CHECK(n>0&&view.not_found==0&&view.sent[0]==sent);
        int summons=0;for(int k=0;k<n;k++)if(events[k].type==BEV_SUMMON){
            CHECK(events[k].mon&&events[k].mon->species==SPECIES_ID_MORPEKO&&events[k].mon->form==1);summons++;
        }
        CHECK(summons==1);
    }
}
static void boss_damage_then_transform(void)
{
    static bs_battle b;bs_begin(&b,BIOME_ID_GRASS,10,"boss-shield-transform");
    int attack[]={MOVE_ID_TACKLE},copy[]={MOVE_ID_TRANSFORM};
    pr_pokemon *player=bs_add(&b,0,SPECIES_ID_RATTATA,40,attack,1);
    pr_pokemon *boss=bs_add(&b,1,SPECIES_ID_DITTO,40,copy,1);
    player->stats[STAT_ATK]=110;player->stats[STAT_SPD]=1000;
    boss->stats[STAT_HP]=boss->hp=100;boss->stats[STAT_DEF]=40;boss->stats[STAT_SPD]=1;
    boss->is_boss=true;boss->boss_segments=2;boss->boss_segment_index=1;
    pr_update_passive(&b.bt,boss);
    bv_mon shown_player,shown_boss;be_mon(&b.bt,player,&shown_player);be_mon(&b.bt,boss,&shown_boss);
    bs_turn_fight(&b,0);
    CHECK(boss->hp>0&&boss->transformed&&boss->boss_segment_index==0);
    static bv_event events[128];int n=be_convert(&b.bt,&b.eng,&shown_player,&shown_boss,events,128),shields=0,copies=0;
    for(int j=0;j<n;j++){
        const bv_event *e=&events[j];if(e->battler!=2)continue;
        if(e->type==BEV_BOSS_SHIELD){shown_boss.boss_segment_index=e->a;shields++;}
        if(e->type==BEV_TRANSFORM){
            CHECK(shields==1&&e->mon&&e->mon->boss&&e->mon->boss_segments==2);
            CHECK(e->mon->boss_segment_index==0);
            shown_boss=*e->mon;copies++;
        }
    }
    CHECK(shields==1&&copies==1&&shown_boss.boss_segment_index==boss->boss_segment_index);
}
int main(void){run_case(SPECIES_ID_PIKACHU,0,false);run_case(SPECIES_ID_CHARIZARD,1,true);run_case(SPECIES_ID_CHARIZARD,3,true);run_case(SPECIES_ID_DITTO,0,true);switch_imposter();switch_identity_after_form_change();boss_damage_then_transform();printf("Transform visual adapter: %d checks passed\n",checks);return 0;}
