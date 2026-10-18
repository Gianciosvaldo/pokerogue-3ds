/* Replays native getOrInferTier/ModifierData observations through actual saves. */
#define main theft_host_main
#include "move_theft_host_test.c"
#undef main
static const char *keys[]={"LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"};
static const int classes[]={PR_MOD_TURN_HEAL,PR_MOD_HIT_HEAL,PR_MOD_SURVIVE_DAMAGE,PR_MOD_BYPASS_SPEED_CHANCE,PR_MOD_FLINCH_CHANCE,PR_MOD_RESET_NEGATIVE_STAT_STAGE,PR_MOD_TURN_HELD_TRANSFER,PR_MOD_BERRY,PR_MOD_MOVE_ACCURACY_BOOSTER,PR_MOD_MULTI_HIT,PR_MOD_CRIT_BOOSTER,PR_MOD_TURN_STATUS_EFFECT,PR_MOD_TURN_STATUS_EFFECT,PR_MOD_NATURE_WEIGHT,PR_MOD_DAMAGE_MONEY_REWARD,PR_MOD_ATTACK_TYPE_BOOSTER,PR_MOD_INSTANT_REVIVE};
int main(int argc,char **argv)
{
    FILE *f=argc==2?fopen(argv[1],"r"):NULL;if(!f)return 2;
    char line[256],key[80],tail;int version,count,total=0;
    CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"THEFT_TIERS %d %d %c",&version,&count,&tail)==2&&version==1&&count==17);
    for(int i=0;i<count;i++){
        int t[3],masks[3];CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"T %79s %d %d %d %d %d %d %c",key,&t[0],&t[1],&t[2],&masks[0],&masks[1],&masks[2],&tail)==7&&!strcmp(key,keys[i]));
        pr_modifier mod={.cls=classes[i]};
        for(int pool=0;pool<3;pool++){
            CHECK(t[pool]>=0&&t[pool]<=5);
            CHECK(pr_held_item_tier_for_pool(&mod,pool==2)==t[pool]);
            /* Generated canonical identities occupy at most one native tier
             * in a given pool. Pool-absent stocks are inferred via fallbacks. */
            CHECK(masks[pool]==0||masks[pool]==(1<<t[pool]));
        }
        for(int player=1;player>=0;player--)for(int tier=0;tier<6;tier++){
            int side,old,expected;CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"S %79s %d %d %d %c",key,&side,&old,&expected,&tail)==4&&!strcmp(key,keys[i])&&side==player&&old==tier&&expected>=-1&&expected<=5);
            setup(0,0,0);run.n_items=run.n_enemy_items=0;gc_item item={.key=keys[i],.arg=(i==7||i==15)?0:-1,.stack=1,.pokemon_id=run.bt.field[player?0:2]->id,.type_tier=tier+1};
            if(player){run.items[0]=item;run.n_items=1;}else{run.enemy_items[0]=item;run.n_enemy_items=1;}gc_items_to_battle(&run);
            size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);const char *why;CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);
            CHECK((player?run.items[0]:run.enemy_items[0]).type_tier==tier+1);
            CHECK((player?loaded.items[0]:loaded.enemy_items[0]).type_tier==expected+1);
            CHECK(loaded.bt.mods[player?0:1][0].type_tier==expected+1);total++;
        }
    }
    int resets;CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"THEFT_TIERS_COMPLETE %d %d %c",&count,&resets,&tail)==2&&count==17&&resets==total&&total==204);CHECK(fgetc(f)==EOF);fclose(f);
    printf("Native tier inference and reconstruction: 17 identities, 204 actual save/load cases, %d checks exact\n",checks);return 0;
}
