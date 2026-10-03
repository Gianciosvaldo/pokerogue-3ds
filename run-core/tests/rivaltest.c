/* C half of Task 48 rival fixed-party differential test. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const pr_trainer_info *trainer_info(int type)
{
    for (int i=0;i<pr_trainer_count;i++) if (pr_trainers[i].type==type) return &pr_trainers[i];
    return NULL;
}
int main(int argc,char **argv)
{
    if(argc!=2){fprintf(stderr,"usage: %s <cases>\n",argv[0]);return 2;}
    FILE *f=fopen(argv[1],"r");if(!f){perror(argv[1]);return 2;}
    char line[16384];int cases=0,fail=0;
    while(fgets(line,sizeof line,f)){
        if(line[0]!='R')continue;
        char seed[64];int wave,type,n;
        char *p=line+1;
        if(sscanf(p,"%63s %d %d %d",seed,&wave,&type,&n)!=4)return 2;
        /* Walk past four tokens, then parse expected levels. */
        for(int k=0;k<4;k++){while(*p==' ')p++;while(*p&&*p!=' ')p++;}
        int exp_levels[PR_MAX_PARTY_TEMPLATE];
        for(int i=0;i<n;i++){while(*p==' ')p++;exp_levels[i]=(int)strtol(p,&p,10);}

        pr_run run;memset(&run,0,sizeof run);run.mode=GAME_MODES_CLASSIC;pr_seed_from_ascii(&run.seed,seed);
        pr_new_battle_ctx nbctx;memset(&nbctx,0,sizeof nbctx);nbctx.biome=BIOME_ID_PLAINS;nbctx.prev_wave=wave-1;nbctx.me.spawn_chance=3;nbctx.enable_fixed_rivals=true;
        pr_new_battle nb;
        int rc=pr_run_new_battle(&run,wave,&nbctx,&nb);
        bool bad=rc!=0||nb.battle_type!=BATTLE_TYPE_TRAINER||nb.trainer_type!=type||nb.n_levels!=n;
        for(int i=0;i<n&&!bad;i++)if(nb.levels[i]!=exp_levels[i])bad=true;

        const pr_trainer_info *ti=trainer_info(type);
        if(!ti||nb.template_index<0||nb.template_index>=ti->n_templates){bad=true;}
        pr_trainer_party_ctx pc;memset(&pc,0,sizeof pc);
        pc.enemy.wave=wave;pc.enemy.biome=BIOME_ID_PLAINS;pc.enemy.wave_cycle_offset=0;
        pc.enemy.battle_type=BATTLE_TYPE_TRAINER;pc.enemy.trainer_type=type;
        pc.enemy.trainer_id=1234;pc.enemy.secret_id=5678;
        pc.template_=ti?&pr_party_templates[ti->templates[nb.template_index<0?0:nb.template_index]]:NULL;
        pc.n_levels=n;for(int i=0;i<n;i++)pc.levels[i]=exp_levels[i];
        pr_trainer_party got;
        if(!bad&&pr_run_gen_trainer_party(&run,&pc,&got)<0)bad=true;
        /* The producer replaces generateAndPopulateMoveset with a no-op but still executes the
         * remainder of EnemyPokemon's constructor before the rival postProcess callback. */
        if(!bad)for(int i=0;i<n;i++){
            pr_enemy_gen_ctx ec=pc.enemy;
            ec.species=got.species[i];ec.level=exp_levels[i];ec.trainer_slot=1;ec.boss=false;ec.shiny_lock=false;
            if(pr_run_enemy_pokemon_finish(&run,&ec,&got.mons[i])<0){bad=true;break;}
        }

        for(int i=0;i<n;i++){
            if(!fgets(line,sizeof line,f)||line[0]!='M')return 2;
            int sp,lev,ab,tera,boss,shiny,var,form;
            if(sscanf(line+1,"%d %d %d %d %d %d %d %d",&sp,&lev,&ab,&tera,&boss,&shiny,&var,&form)!=8)return 2;
            if(!bad){
                const pr_enemy_gen *m=&got.mons[i];
                if(got.species[i]!=sp||got.levels[i]!=lev||m->ability_index!=ab||m->tera_type!=tera
                   ||m->boss_segments!=boss||m->shiny!=!!shiny||m->variant!=var
                   ||(form>=0&&m->form!=form))bad=true;
            }
        }
        cases++;
        if(bad){if(fail<20)fprintf(stderr,"FAIL case %d seed=%s wave=%d type=%d\n",cases,seed,wave,type);fail++;}
    }
    fclose(f);
    printf("rival summary: %d fights, %d mismatches\n",cases,fail);
    return fail?1:0;
}
