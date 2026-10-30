/* Trash to Treasure at upstream 8555c08: the two choices are a configured
 * Garbodor fight, or held healing items plus permanent shop-cost Black Sludge. */
#include "internal.h"
#include <math.h>
#include <string.h>
extern int pr_reward_generate(pr_reward_ctx *ctx,int item); /* generator API, as in gc_berries */

int gc_trash_items(gc_run *run,gc_item out[10])
{
    const pr_rng saved=run->run.rng;
    run->run.rng=pr_run_seed_offset_rng(&run->run,run->wave,NULL);
    pr_reward_ctx *ctx=gc_reward_ctx(run);
    int n=0;
    memset(out,0,10*sizeof *out);
    for(int i=0;i<6;i++) {
        int item=i<4?PR_ITEM_BERRY:PR_ITEM_BASE_STAT_BOOSTER;
        int arg=pr_reward_generate(ctx,item);
        out[n++]=(gc_item){.key=pr_item_keys[item],.arg=(int16_t)arg,.stack=1};
    }
    const char*keys[]={"TOXIC_ORB","SOOTHE_BELL","LUCKY_EGG","GOLDEN_EGG"};
    const int ranges[]={2,2,3,2},minimum[]={0,1,1,0};
    for(int i=0;i<4;i++) {
        int stack=pr_rng_seed_int(&run->run.rng,ranges[i],minimum[i]);
        out[n++]=(gc_item){.key=keys[i],.arg=-1,.stack=(int16_t)stack};
    }
    run->run.rng=saved;
    return n;
}

static void give_healing_item(gc_run *run,const char *key)
{
    for(int k=0;k<run->n_party;k++) {
        int p=run->party_order[k];
        const gc_mon*m=&run->party[p];
        int first=-1;
        for(int i=0;i<run->n_items;i++)if(run->items[i].pokemon_id==m->b.id&&!strcmp(run->items[i].key,key)){first=i;break;}
        /* Native find() reads the first matching class, not a sum across stocks. */
        if(first<0||run->items[first].stack+run->items[first].virtual_stack<gc_item_max_held(run,key,-1,m)) {
            gc_offer offer={.key=key,.arg=-1,.target=GC_TARGET_POKEMON};const char*why=NULL;
            (void)gc_port_apply_reward(run,&offer,p,-1,&why);
            break;
        }
    }
    gc_ev_msg(run,"battle:rewardGainCount",2,gc_m_item_arg("modifierName",key,-1),pr_arg_int("count",1));
}

void gc_trash_execute(gc_run *run,int option)
{
    if(option==1) {
        give_healing_item(run,"LEFTOVERS");give_healing_item(run,"SHELL_BELL");
        gc_port_give_reward(run,"MYSTERY_ENCOUNTER_BLACK_SLUDGE");
        gc_ev_msg(run,"battle:rewardGain",1,gc_m_item_arg("modifierName","MYSTERY_ENCOUNTER_BLACK_SLUDGE",-1));
        gc_me_leave_with_heal_shop(run);
        return;
    }
    gc_internal*in=gc_int(run);static gc_item raw[10],items[10];int total=gc_trash_items(run,raw),n=0;
    run->double_battle=false;
    /* Both awaits before initBattleWithEnemyConfig restore the option offset;
     * the constructor and its post-await thresholds use the ambient stream. */
    int base=pr_run_get_level_for_wave(&run->run,&run->run.rng,run->wave);
    int level=base+(int)fmax(floor((double)run->wave/10*0.5+0.5),0);
    gc_mon*boss=&run->enemy[0];
    pr_rng constructor_rng=run->run.rng;
    if(!gc_me_new_boss(run,&constructor_rng,SPECIES_ID_GARBODOR,level,true,boss)) {
        gc_stub(run,GC_STUB_UNPORTED_ENEMY);run->n_enemy=0;gc_me_leave(run);return;
    }
    run->run.rng=constructor_rng;
    run->n_enemy=1;run->battle_type=BATTLE_TYPE_WILD;
    boss->b.form=1;boss->b.passive_flag=false;boss->b.shiny=false;
    boss->b.boss_segments++;boss->b.boss_segment_index=(uint8_t)(boss->b.boss_segments-1);
    boss->b.n_moves=0;memset(boss->b.moves,0,sizeof boss->b.moves);memset(boss->b.pp_used,0,sizeof boss->b.pp_used);memset(boss->b.pp_up,0,sizeof boss->b.pp_up);
    for(int i=0;i<4;i++)boss->b.max_pp_override[i]=-1;
    const int moves[]={MOVE_ID_GUNK_SHOT,MOVE_ID_STOMPING_TANTRUM,MOVE_ID_HAMMER_ARM,MOVE_ID_PAYBACK};
    for(int i=0;i<4;i++)gc_mon_set_move(boss,i,moves[i]);
    /* generateEnemyModifiers(config) calls PersistentModifier.add in config
     * order. New stocks enter even above their cap; matching stocks refuse a
     * whole increment that would exceed it. Zero stocks disappear on update. */
    for(int i=0;i<total;i++)if(raw[i].stack>0) {
        int found=-1;
        for(int j=0;j<n;j++)if(!strcmp(items[j].key,raw[i].key)&&items[j].arg==raw[i].arg){found=j;break;}
        if(found<0)items[n++]=raw[i];
        else if(items[found].stack+raw[i].stack<=gc_item_max_held(run,raw[i].key,raw[i].arg,boss))items[found].stack+=raw[i].stack;
    }
    gc_enemy_finish_party_items(run,items,n);
    in->me_reward.active=true;in->me_reward.has_item=false;in->me_reward.func=PR_ITEM_LEFTOVERS;in->me_reward.fill=true;in->me_reward.before=false;in->me_reward.n_more=0;
    in->me_reward.n_tiers=3;in->me_reward.tiers[0]=MODIFIER_TIER_ROGUE;in->me_reward.tiers[1]=MODIFIER_TIER_ULTRA;in->me_reward.tiers[2]=MODIFIER_TIER_GREAT;
    in->me_no_flee=true;gc_me_add_start_effect(run,2,0,MOVE_ID_TOXIC);gc_me_add_start_effect(run,2,2,MOVE_ID_STOCKPILE);gc_me_begin_battle(run,GC_ME_TRASH_TO_TREASURE);
}
